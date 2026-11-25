#!/usr/bin/env bash
set -u

GEM5="./build/RISCV/gem5.opt"
CONFIG="configs/example/riscv/se_systemC.py"

BASE_RISCV="/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/gem5"
BASE_BIN="/data/ngiannopoulos/Phd/NVDLA/test_git_framework/binaries"

OUT_BASE="$BASE_RISCV/m5out/riscv_systemC_final"
NN_SUITE="$BASE_BIN/nn_suite"

CIFAR_IMAGE="$NN_SUITE/0000.jpg"
LENET_IMAGE="$NN_SUITE/eight_invert.pgm"

SPM_SIZE="1MiB"
SPM_LAT="2"

TESTS=(
"fp16_alexnet|cifar_10_alexnet_fp16|$NN_SUITE/alexnet_w_sf_nv_full_fp16.nvdla|$CIFAR_IMAGE"
"fp16_densenet|cifar_10_densenet_fp16|$NN_SUITE/densenet_no_bn_w_sf_nv_full_fp16.nvdla|$CIFAR_IMAGE"
"fp16_googlenet|cifar_10_googlenet_fp16|$NN_SUITE/googlenet_w_sf_nv_full_fp16.nvdla|$CIFAR_IMAGE"
"fp16_resnet34|cifar_10_resnet34_fp16|$NN_SUITE/resnet34_no_bn_w_sf_nv_full_fp16.nvdla|$CIFAR_IMAGE"
"fp16_squeezenet|cifar_10_squeezenet_fp16|$NN_SUITE/squeezenet_no_bn_w_sf_nv_full_fp16.nvdla|$CIFAR_IMAGE"
"fp16_vgg11|cifar_10_vgg11_fp16|$NN_SUITE/vgg11_w_sf_nv_full_fp16.nvdla|$CIFAR_IMAGE"
"fp16_lenet|lenet_fp16|$NN_SUITE/lenet_from_caffe_nv_full_fp16.nvdla|$LENET_IMAGE"

"int8_alexnet|cifar_10_alexnet_int8|$NN_SUITE/alexnet_w_sf_nv_full_int8.nvdla|$CIFAR_IMAGE"
"int8_densenet|cifar_10_densenet_int8|$NN_SUITE/densenet_no_bn_w_sf_nv_full_int8.nvdla|$CIFAR_IMAGE"
"int8_googlenet|cifar_10_googlenet_int8|$NN_SUITE/googlenet_w_sf_nv_full_int8.nvdla|$CIFAR_IMAGE"
"int8_resnet34|cifar_10_resnet34_int8|$NN_SUITE/resnet34_no_bn_w_sf_nv_full_int8.nvdla|$CIFAR_IMAGE"
"int8_squeezenet|cifar_10_squeezenet_int8|$NN_SUITE/squeezenet_no_bn_w_sf_nv_full_int8.nvdla|$CIFAR_IMAGE"
"int8_vgg11|cifar_10_vgg11_int8|$NN_SUITE/vgg11_w_sf_nv_full_int8.nvdla|$CIFAR_IMAGE"
"int8_lenet|lenet_int8|$NN_SUITE/lenet_from_caffe_nv_full_int8.nvdla|$LENET_IMAGE"
)

mkdir -p "$OUT_BASE"

SUMMARY="$OUT_BASE/run_all_parallel_summary.txt"
PID_FILE="$OUT_BASE/run_all_parallel_pids.txt"

: > "$SUMMARY"
: > "$PID_FILE"

declare -a PIDS NAMES OUT_DIRS

TOTAL=${#TESTS[@]}
INDEX=0

echo "================================================================"
echo "Launching $TOTAL RISC-V SystemC NVDLA simulations in parallel"
echo "FP16: --normalize 255"
echo "INT8: no --normalize"
echo "================================================================"

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

    echo "[$INDEX/$TOTAL] $CASE_NAME normalize=$NORMALIZE_MSG"
    echo "  output   : $FULL_OUT"
    echo "  loadable : $LOADABLE"
    echo "  image    : $IMAGE"
    echo "  log      : $LAUNCH_LOG"

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
echo "================================================================"
echo "All $TOTAL simulations launched."
echo "PID file: $PID_FILE"
echo "Waiting for completion..."
echo "================================================================"

for i in "${!PIDS[@]}"; do
    PID="${PIDS[$i]}"
    CASE_NAME="${NAMES[$i]}"
    FULL_OUT="${OUT_DIRS[$i]}"

    if wait "$PID"; then
        RC=0
    else
        RC=$?
    fi

    if [ "$RC" -eq 0 ]; then
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
echo "================================================================"
echo "All parallel RISC-V SystemC NVDLA runs finished."
echo "Summary:"
cat "$SUMMARY"
echo "================================================================"
