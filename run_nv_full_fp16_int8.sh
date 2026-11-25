#!/usr/bin/env bash
set -uo pipefail

GEM5="${GEM5:-./build/RISCV/gem5.opt}"
CONFIG="${CONFIG:-configs/example/riscv/se_RTL.py}"

BASE_RISCV="${BASE_RISCV:-/data/ngiannopoulos/Phd/NVDLA/gem5_RISCV/gem5}"
NVDLA_QUANT="${NVDLA_QUANT:-/data/ngiannopoulos/Phd/NVDLA/nvdla_quantization}"

# Separate output root: these are PRIVATE-CACHE runs only.
OUT_BASE="${OUT_BASE:-$BASE_RISCV/m5out/riscv_verilator_private_cache_from_nvdla_quantization}"

LOADABLE_ROOT="$NVDLA_QUANT/out"

# =============================================================================
# Accelerator private-cache configuration
# =============================================================================

CACHE_SIZE="${CACHE_SIZE:-1MB}"
CACHE_ASSOC="${CACHE_ASSOC:-16}"
CACHE_TAG_LAT="${CACHE_TAG_LAT:-12}"
CACHE_DATA_LAT="${CACHE_DATA_LAT:-12}"
CACHE_RESP_LAT="${CACHE_RESP_LAT:-5}"
CACHE_MSHRS="${CACHE_MSHRS:-32}"
CACHE_TARGETS="${CACHE_TARGETS:-8}"
CACHE_WRITE_BUFFERS="${CACHE_WRITE_BUFFERS:-8}"
CACHE_CLUSIVITY="${CACHE_CLUSIVITY:-mostly_incl}"


TESTS=(

# ===========================================================================
# NV_FULL FP16
# ===========================================================================

# COMMENTED OUT BY USER REQUEST
# "fp16_alexnet|cifar_10_alexnet_fp16|$LOADABLE_ROOT/alexnet/cifar10/onnc_fp16_out/alexnet_nv_full_fp16.nvdla|$LOADABLE_ROOT/alexnet/cifar10/vp_in/vp_in_000.jpg"

# COMMENTED OUT BY USER REQUEST
# "fp16_densenet|cifar_10_densenet_fp16|$LOADABLE_ROOT/densenet/cifar10/onnc_fp16_out/densenet_nv_full_fp16.nvdla|$LOADABLE_ROOT/densenet/cifar10/vp_in/vp_in_000.jpg"

"fp16_googlenet|cifar_10_googlenet_fp16|$LOADABLE_ROOT/googlenet/cifar10/onnc_fp16_out/googlenet_nv_full_fp16.nvdla|$LOADABLE_ROOT/googlenet/cifar10/vp_in/vp_in_000.jpg"

# COMMENTED OUT BY USER REQUEST
# "fp16_resnet34|cifar_10_resnet34_fp16|$LOADABLE_ROOT/resnet34/cifar10/onnc_fp16_out/resnet34_nv_full_fp16.nvdla|$LOADABLE_ROOT/resnet34/cifar10/vp_in/vp_in_000.jpg"

# COMMENTED OUT BY USER REQUEST
# "fp16_squeezenet|cifar_10_squeezenet_fp16|$LOADABLE_ROOT/squeezenet/cifar10/onnc_fp16_out/squeezenet_nv_full_fp16.nvdla|$LOADABLE_ROOT/squeezenet/cifar10/vp_in/vp_in_000.jpg"

"fp16_vgg11|cifar_10_vgg11_fp16|$LOADABLE_ROOT/vgg11/cifar10/onnc_fp16_out/vgg11_nv_full_fp16.nvdla|$LOADABLE_ROOT/vgg11/cifar10/vp_in/vp_in_000.jpg"

"fp16_lenet|lenet_fp16|$LOADABLE_ROOT/lenet5/mnist/onnc_fp16_out/lenet5_nv_full_fp16.nvdla|$LOADABLE_ROOT/lenet5/mnist/vp_in_gray/vp_in_000.jpg"


# ===========================================================================
# NV_FULL INT8
# ===========================================================================

# COMMENTED OUT BY USER REQUEST
# "int8_alexnet|cifar_10_alexnet_int8|$LOADABLE_ROOT/alexnet/cifar10/onnc_int8_out/alexnet_nv_full_int8.nvdla|$LOADABLE_ROOT/alexnet/cifar10/vp_in/vp_in_000.jpg"

# COMMENTED OUT BY USER REQUEST
# "int8_densenet|cifar_10_densenet_int8|$LOADABLE_ROOT/densenet/cifar10/onnc_int8_out/densenet_nv_full_int8.nvdla|$LOADABLE_ROOT/densenet/cifar10/vp_in/vp_in_000.jpg"

"int8_googlenet|cifar_10_googlenet_int8|$LOADABLE_ROOT/googlenet/cifar10/onnc_int8_out/googlenet_nv_full_int8.nvdla|$LOADABLE_ROOT/googlenet/cifar10/vp_in/vp_in_000.jpg"

# COMMENTED OUT BY USER REQUEST
# "int8_resnet34|cifar_10_resnet34_int8|$LOADABLE_ROOT/resnet34/cifar10/onnc_int8_out/resnet34_nv_full_int8.nvdla|$LOADABLE_ROOT/resnet34/cifar10/vp_in/vp_in_000.jpg"

# COMMENTED OUT BY USER REQUEST
# "int8_squeezenet|cifar_10_squeezenet_int8|$LOADABLE_ROOT/squeezenet/cifar10/onnc_int8_out/squeezenet_nv_full_int8.nvdla|$LOADABLE_ROOT/squeezenet/cifar10/vp_in/vp_in_000.jpg"

"int8_vgg11|cifar_10_vgg11_int8|$LOADABLE_ROOT/vgg11/cifar10/onnc_int8_out/vgg11_nv_full_int8.nvdla|$LOADABLE_ROOT/vgg11/cifar10/vp_in/vp_in_000.jpg"

"int8_lenet|lenet_int8|$LOADABLE_ROOT/lenet5/mnist/onnc_int8_out/lenet5_nv_full_int8.nvdla|$LOADABLE_ROOT/lenet5/mnist/vp_in_gray/vp_in_000.jpg"

)


# =============================================================================
# Resolve loadable only inside the selected precision directory.
# =============================================================================

resolve_loadable() {
    local expected="$1" directory candidate
    local -a matches=()

    if [[ -f "$expected" ]]; then
        printf '%s\n' "$expected"
        return 0
    fi

    directory="$(dirname "$expected")"

    if [[ ! -d "$directory" ]]; then
        echo "[ERROR] Missing directory: $directory" >&2
        return 1
    fi

    while IFS= read -r -d '' candidate; do
        matches+=("$candidate")
    done < <(find "$directory" -maxdepth 1 -type f -name '*.nvdla' -print0)

    if (( ${#matches[@]} != 1 )); then
        echo "[ERROR] Expected one .nvdla in $directory; found ${#matches[@]}" >&2
        if (( ${#matches[@]} )); then
            printf '  %s\n' "${matches[@]}" >&2
        fi
        return 1
    fi

    printf '%s\n' "${matches[0]}"
}


# =============================================================================
# Pre-flight
# =============================================================================

cd "$BASE_RISCV" || exit 1

[[ -x "$GEM5" ]] || {
    echo "[ERROR] Missing executable: $GEM5" >&2
    exit 1
}

[[ -f "$CONFIG" ]] || {
    echo "[ERROR] Missing config: $CONFIG" >&2
    exit 1
}

mkdir -p "$OUT_BASE"

SUMMARY="$OUT_BASE/run_private_cache_summary.txt"
PID_FILE="$OUT_BASE/run_private_cache_pids.txt"

: > "$SUMMARY"
: > "$PID_FILE"

declare -a PIDS NAMES OUT_DIRS

TOTAL=${#TESTS[@]}
INDEX=0

echo "============================================================"
echo "RISC-V Verilator NV_FULL - PRIVATE CACHE ONLY"
echo "============================================================"
echo "Enabled runs : $TOTAL"
echo "  FP16       : 3 (GoogLeNet, VGG11, LeNet)"
echo "  INT8       : 3 (GoogLeNet, VGG11, LeNet)"
echo
echo "COMMENTED OUT:"
echo "  AlexNet"
echo "  DenseNet"
echo "  ResNet34"
echo "  SqueezeNet"
echo
echo "Private cache:"
echo "  size       : $CACHE_SIZE"
echo "  assoc      : $CACHE_ASSOC"
echo "  tag lat    : $CACHE_TAG_LAT"
echo "  data lat   : $CACHE_DATA_LAT"
echo "  resp lat   : $CACHE_RESP_LAT"
echo "  MSHRs      : $CACHE_MSHRS"
echo "  targets    : $CACHE_TARGETS"
echo "  write bufs : $CACHE_WRITE_BUFFERS"
echo "  clusivity  : $CACHE_CLUSIVITY"
echo "============================================================"


echo
echo "Checking loadables and images..."

RESOLVED=()

for test in "${TESTS[@]}"; do
    IFS='|' read -r CASE_NAME OUT_DIR LOADABLE IMAGE <<< "$test"

    LOADABLE="$(resolve_loadable "$LOADABLE")" || exit 1

    [[ -f "$LOADABLE" ]] || {
        echo "[ERROR] Missing loadable: $LOADABLE" >&2
        exit 1
    }

    [[ -f "$IMAGE" ]] || {
        echo "[ERROR] Missing image: $IMAGE" >&2
        exit 1
    }

    RESOLVED+=("$CASE_NAME|$OUT_DIR|$LOADABLE|$IMAGE")

    echo "[FOUND] $CASE_NAME"
    echo "        loadable: $LOADABLE"
    echo "        image   : $IMAGE"
done

echo
echo "All inputs found."
echo


# =============================================================================
# Private-cache arguments
#
# IMPORTANT:
# The old DMA-embedded arguments:
#   --embed-spm-size=...
#   --embed-spm-lat=...
# are intentionally NOT passed here.
# =============================================================================

PRIVATE_CACHE_ARGS=(
    --add-accel-private-cache
    --accel-pr-cache-size "$CACHE_SIZE"
    --accel-pr-cache-assoc "$CACHE_ASSOC"
    --accel-pr-cache-tag-lat "$CACHE_TAG_LAT"
    --accel-pr-cache-dat-lat "$CACHE_DATA_LAT"
    --accel-pr-cache-resp-lat "$CACHE_RESP_LAT"
    --accel-pr-cache-mshr "$CACHE_MSHRS"
    --accel-pr-cache-tgts-per-mshr "$CACHE_TARGETS"
    --accel-pr-cache-wr-buf "$CACHE_WRITE_BUFFERS"
    --accel-pr-cache-clus "$CACHE_CLUSIVITY"
)


# =============================================================================
# Launch all selected simulations in parallel
# =============================================================================

for test in "${RESOLVED[@]}"; do

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
    echo "  mode     : PRIVATE CACHE"
    echo "  loadable : $LOADABLE"
    echo "  image    : $IMAGE"
    echo "  normalize: $NORMALIZE_MSG"
    echo "  output   : $FULL_OUT"
    echo "------------------------------------------------------------"

    CMD=(
        "$GEM5"
        -e
        -r
        -d "$FULL_OUT"
        "$CONFIG"
        --loadable "$LOADABLE"
        --image "$IMAGE"
        "${EXTRA_ARGS[@]}"
        "${PRIVATE_CACHE_ARGS[@]}"
    )

    # Save exact command.
    printf '%q ' "${CMD[@]}" > "$FULL_OUT/command.sh"
    printf '\n' >> "$FULL_OUT/command.sh"
    chmod +x "$FULL_OUT/command.sh"

    nohup "${CMD[@]}" \
        > "$LAUNCH_LOG" \
        2>&1 \
        < /dev/null &

    PID=$!

    PIDS+=("$PID")
    NAMES+=("$CASE_NAME")
    OUT_DIRS+=("$FULL_OUT")

    printf '%s\t%s\t%s\n' \
        "$CASE_NAME" \
        "$PID" \
        "$FULL_OUT" \
        >> "$PID_FILE"

    echo "[START] $CASE_NAME PID=$PID"
done


echo
echo "============================================================"
echo "All $TOTAL PRIVATE-CACHE simulations launched."
echo "Waiting for completion..."
echo "PID file: $PID_FILE"
echo "============================================================"


# =============================================================================
# Wait for all simulations
# =============================================================================

FAILED=0

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
        printf '%s\tOK\t%s\n' \
            "$CASE_NAME" \
            "$FULL_OUT" \
            >> "$SUMMARY"
    else
        FAILED=1
        echo "[FAILED] $CASE_NAME (exit code $RC)"
        printf '%s\tFAILED(%s)\t%s\n' \
            "$CASE_NAME" \
            "$RC" \
            "$FULL_OUT" \
            >> "$SUMMARY"
    fi

    echo "  simout: $FULL_OUT/simout"
    echo "  simerr: $FULL_OUT/simerr"
done


# =============================================================================
# Final summary
# =============================================================================

echo
echo "============================================================"
echo "FINAL SUMMARY"
echo "============================================================"
echo "Mode: PRIVATE CACHE ONLY"
echo "Output root: $OUT_BASE"
echo "------------------------------------------------------------"
cat "$SUMMARY"
echo "============================================================"

exit "$FAILED"
