#!/usr/bin/env bash
set -u

GEM5="./build/RISCV/gem5.opt"
CONFIG="configs/example/riscv/se_RTL.py"

BASE_RISCV="/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/gem5"
NVDLA_QUANT="/data/ngiannopoulos/Phd/NVDLA/nvdla_quantization"

OUT_BASE="$BASE_RISCV/m5out/riscv_verilator_final_from_nvdla_quantization"

# ---------------------------------------------------------------------------
# Final generated NVDLA loadables
# ---------------------------------------------------------------------------
LOADABLE_ROOT="$NVDLA_QUANT/out"

# Keep the currently validated images for SE RTL execution
BASE_BIN="/data/ngiannopoulos/Phd/NVDLA/test_git_framework/binaries"
OLD_NN_SUITE="$BASE_BIN/nn_suite"

CIFAR_IMAGE="$OLD_NN_SUITE/0000.jpg"
LENET_IMAGE="$OLD_NN_SUITE/eight_invert.pgm"

SPM_SIZE="1MiB"
SPM_LAT="2"

TESTS=(

# ===========================================================================
# NV_FULL FP16
# ===========================================================================

#"fp16_alexnet|cifar_10_alexnet_fp16|$LOADABLE_ROOT/alexnet/cifar10/onnc_fp16_out/alexnet_nv_full_fp16.nvdla|$CIFAR_IMAGE"

"fp16_densenet|cifar_10_densenet_fp16|$LOADABLE_ROOT/densenet/cifar10/onnc_fp16_out/densenet_nv_full_fp16.nvdla|$CIFAR_IMAGE"

#"fp16_googlenet|cifar_10_googlenet_fp16|$LOADABLE_ROOT/googlenet/cifar10/onnc_fp16_out/googlenet_nv_full_fp16.nvdla|$CIFAR_IMAGE"

"fp16_resnet34|cifar_10_resnet34_fp16|$LOADABLE_ROOT/resnet34/cifar10/onnc_fp16_out/resnet34_nv_full_fp16.nvdla|$CIFAR_IMAGE"

#"fp16_squeezenet|cifar_10_squeezenet_fp16|$LOADABLE_ROOT/squeezenet/cifar10/onnc_fp16_out/squeezenet_nv_full_fp16.nvdla|$CIFAR_IMAGE"

#"fp16_vgg11|cifar_10_vgg11_fp16|$LOADABLE_ROOT/vgg11/cifar10/onnc_fp16_out/vgg11_nv_full_fp16.nvdla|$CIFAR_IMAGE"

#"fp16_lenet|lenet_fp16|$LOADABLE_ROOT/lenet5/mnist/onnc_fp16_out/lenet5_nv_full_fp16.nvdla|$LENET_IMAGE"


# ===========================================================================
# NV_FULL INT8
# ===========================================================================

#"int8_alexnet|cifar_10_alexnet_int8|$LOADABLE_ROOT/alexnet/cifar10/onnc_int8_out/alexnet_nv_full_int8.nvdla|$CIFAR_IMAGE"

"int8_densenet|cifar_10_densenet_int8|$LOADABLE_ROOT/densenet/cifar10/onnc_int8_out/densenet_nv_full_int8.nvdla|$CIFAR_IMAGE"

#"int8_googlenet|cifar_10_googlenet_int8|$LOADABLE_ROOT/googlenet/cifar10/onnc_int8_out/googlenet_nv_full_int8.nvdla|$CIFAR_IMAGE"

"int8_resnet34|cifar_10_resnet34_int8|$LOADABLE_ROOT/resnet34/cifar10/onnc_int8_out/resnet34_nv_full_int8.nvdla|$CIFAR_IMAGE"

#"int8_squeezenet|cifar_10_squeezenet_int8|$LOADABLE_ROOT/squeezenet/cifar10/onnc_int8_out/squeezenet_nv_full_int8.nvdla|$CIFAR_IMAGE"

#"int8_vgg11|cifar_10_vgg11_int8|$LOADABLE_ROOT/vgg11/cifar10/onnc_int8_out/vgg11_nv_full_int8.nvdla|$CIFAR_IMAGE"

#"int8_lenet|lenet_int8|$LOADABLE_ROOT/lenet5/mnist/onnc_int8_out/lenet5_nv_full_int8.nvdla|$LENET_IMAGE"

)

mkdir -p "$OUT_BASE"

SUMMARY="$OUT_BASE/run_all_parallel_summary.txt"
PID_FILE="$OUT_BASE/run_all_parallel_pids.txt"

: > "$SUMMARY"
: > "$PID_FILE"

declare -a PIDS NAMES OUT_DIRS

TOTAL=${#TESTS[@]}
INDEX=0

echo "============================================================"
echo "Launching $TOTAL RISC-V NVDLA NV_FULL simulations in parallel"
echo "  FP16 : 7"
echo "  INT8 : 7"
echo "============================================================"

# ---------------------------------------------------------------------------
# Pre-flight check
# ---------------------------------------------------------------------------
echo
echo "Checking loadables and images..."

for test in "${TESTS[@]}"; do
    IFS='|' read -r CASE_NAME OUT_DIR LOADABLE IMAGE <<< "$test"

    if [[ ! -f "$LOADABLE" ]]; then
        echo "[ERROR] Missing loadable:"
        echo "        $LOADABLE"
        exit 1
    fi

    if [[ ! -f "$IMAGE" ]]; then
        echo "[ERROR] Missing image:"
        echo "        $IMAGE"
        exit 1
    fi

    echo "[FOUND] $CASE_NAME"
    echo "        $LOADABLE"
done

echo
echo "All inputs found."
echo

# ---------------------------------------------------------------------------
# Launch all simulations
# ---------------------------------------------------------------------------
for test in "${TESTS[@]}"; do

    INDEX=$((INDEX + 1))

    IFS='|' read -r CASE_NAME OUT_DIR LOADABLE IMAGE <<< "$test"

    FULL_OUT="$OUT_BASE/$OUT_DIR"
    LAUNCH_LOG="$FULL_OUT/launcher.log"

    mkdir -p "$FULL_OUT"

    EXTRA_ARGS=()

    if [[ "$CASE_NAME" == fp16_* ]]; then
        EXTRA_ARGS+=(--normalize 255)
        NORMALIZE_MSG="255"
    else
        NORMALIZE_MSG="NONE"
    fi

    echo "------------------------------------------------------------"
    echo "[$INDEX/$TOTAL] $CASE_NAME"
    echo "  loadable : $LOADABLE"
    echo "  image    : $IMAGE"
    echo "  normalize: $NORMALIZE_MSG"
    echo "  output   : $FULL_OUT"
    echo "------------------------------------------------------------"

    nohup "$GEM5" -e -r \
        -d "$FULL_OUT" \
        "$CONFIG" \
        --embed-spm-size="$SPM_SIZE" \
        --embed-spm-lat="$SPM_LAT" \
        --loadable "$LOADABLE" \
        --image "$IMAGE" \
        "${EXTRA_ARGS[@]}" \
        > "$LAUNCH_LOG" 2>&1 &

    PID=$!

    PIDS+=("$PID")
    NAMES+=("$CASE_NAME")
    OUT_DIRS+=("$FULL_OUT")

    echo "$CASE_NAME $PID $FULL_OUT" >> "$PID_FILE"
done

echo
echo "============================================================"
echo "All $TOTAL simulations launched."
echo "Waiting for completion..."
echo "============================================================"

# ---------------------------------------------------------------------------
# Wait for all simulations
# ---------------------------------------------------------------------------
for i in "${!PIDS[@]}"; do

    PID="${PIDS[$i]}"
    CASE_NAME="${NAMES[$i]}"
    FULL_OUT="${OUT_DIRS[$i]}"

    if wait "$PID"; then
        RC=0
    else
        RC=$?
    fi

    if [[ "$RC" -eq 0 ]]; then
        echo "[OK] $CASE_NAME"
        echo "$CASE_NAME : OK" >> "$SUMMARY"
    else
        echo "[FAILED] $CASE_NAME (exit code $RC)"
        echo "$CASE_NAME : FAILED (exit code $RC)" >> "$SUMMARY"
    fi

    echo "  simout: $FULL_OUT/simout"
    echo "  simerr: $FULL_OUT/simerr"
done

echo
echo "============================================================"
echo "FINAL SUMMARY"
echo "============================================================"
cat "$SUMMARY"