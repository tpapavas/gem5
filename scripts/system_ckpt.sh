#!/usr/bin/env bash
# Checkpoint/restore for gem5 + one NVDLA, in SE mode.
#
# Before the NVDLA starts (no RTL state needed):
#   system_ckpt.sh probe    <tag>   find the tick of the first NVDLA access
#   system_ckpt.sh create   <tag>   write the checkpoint before that tick
#   system_ckpt.sh restore  <tag>   run to the end from the checkpoint
#   system_ckpt.sh baseline <tag>   full run without a checkpoint
#
# At any tick, also while the NVDLA runs (gem5 built with NVDLA_SAVABLE=1):
#   system_ckpt.sh at         <tag> <tick>    checkpoint at <tick>, continue
#   system_ckpt.sh restore-at <tag> <tick>    run to the end from that one
#   system_ckpt.sh every      <tag> <period>  checkpoint every <period> ticks
#
# At an NVDLA cycle, e.g. inside a layer. The NVDLA clock stops while it is
# idle, so a cycle has no fixed tick. The "(N)" in the simout is the cycle:
#   system_ckpt.sh at-cycle      <tag> <cycle>  checkpoint at <cycle>, continue
#   system_ckpt.sh restore-cycle <tag> <cycle>  run to the end from that one
#
# Before the NVDLA starts and after every HW layer, in one run:
#   system_ckpt.sh layers        <tag>          writes runs/<tag>/layers/layers.txt
#   system_ckpt.sh restore-layer <tag> <n>      run to the end from layer <n>
#                                               (0 = before the NVDLA starts)
#
# <tag> is lenet_int8 by default and names runs/<tag>. Any other input:
#   LOADABLE=<.nvdla> IMAGE=<image> system_ckpt.sh <stage> <tag> ...
# SE_EXTRA="<args>" adds se_RTL.py arguments, e.g. "--normalize 255" for fp16.
# The checkpoint holds the loaded loadable and image, so each pair needs its
# own <tag>, and a restore must use the same LOADABLE, IMAGE and SE_EXTRA.
# GEM5=<binary> runs another gem5 build, RUN_NAME=<name> names the run dir,
# CPT=<dir> picks the checkpoint of restore-at.
set -Eeuo pipefail

SELF="$(readlink -f "$0")"
STAGE="${1:?usage: system_ckpt.sh <stage> [tag] [tick|period]}"
TAG="${2:-lenet_int8}"
ARG="${3:-}"

TASK=/data/ngiannopoulos/Phd/tasks/task5_gem5_nvdla_checkpoint
GEM5_ROOT="$TASK/sources/gem5"
GEM5="${GEM5:-$GEM5_ROOT/build/ARM/gem5.opt}"
CONFIG="$GEM5_ROOT/configs/example/arm/se_RTL.py"
QUANT=/data/ngiannopoulos/Phd/NVDLA/nvdla_quantization
# Drain finishes the instructions already in the pipeline, so stop this many
# ticks early to be sure the first NVDLA access has not been executed.
MARGIN_TICKS="${MARGIN_TICKS:-10000000}"

if [[ -n "${LOADABLE:-}" || -n "${IMAGE:-}" ]]; then
    [[ -f "${LOADABLE:-}" && -f "${IMAGE:-}" ]] ||
        { echo "LOADABLE and IMAGE must both be existing files" >&2; exit 2; }
else case "$TAG" in
    lenet_int8)
        LOADABLE=/data/ngiannopoulos/Phd/NVDLA/nvdla_quantization_2/out/lenet5/mnist/onnc_int8_out/int8_onnc.nvdla
        IMAGE="$QUANT/out_verified_after_fix_pdp_v2/run_20261102_124920_4ynwxp/lenet5/mnist/vp_in_gray/vp_in_000.pgm"
        ;;
    *)
        echo "Unknown tag $TAG, give LOADABLE= and IMAGE=" >&2
        exit 2
        ;;
esac; fi

ARGS=(--loadable "$LOADABLE" --image "$IMAGE" --dlas 1
      --add-accel-private-cache --accel-pr-cache-size=1MiB
      --accel-pr-cache-assoc=16 --accel-pr-cache-tag-lat=12
      --accel-pr-cache-dat-lat=12 --accel-pr-cache-resp-lat=5)
# shellcheck disable=SC2206
[[ -z "${SE_EXTRA:-}" ]] || ARGS+=($SE_EXTRA)

case "$STAGE" in
    at|restore-at|every|at-cycle|restore-cycle|restore-layer)
        [[ "$ARG" =~ ^[0-9]+$ ]] || { echo "$STAGE needs a number" >&2; exit 2; }
        ;;
esac

RUN="$TASK/runs/$TAG"
case "$STAGE" in
    at)         OUT="$RUN/at_$ARG" ;;
    restore-at) OUT="$RUN/restore_at_$ARG" ;;
    every)      OUT="$RUN/every_$ARG" ;;
    restore-layer) OUT="$RUN/restore_layer_$ARG" ;;
    at-cycle)      OUT="$RUN/at_cycle_$ARG" ;;
    restore-cycle) OUT="$RUN/restore_cycle_$ARG" ;;
    *)          OUT="$RUN/$STAGE" ;;
esac
# RUN_NAME=<name> writes to runs/<tag>/<name> instead
[[ -z "${RUN_NAME:-}" ]] || OUT="$RUN/$RUN_NAME"
[[ ! -e "$OUT" ]] || { echo "$OUT already exists, refusing to reuse it" >&2; exit 1; }
mkdir -p "$OUT"
cd "$OUT"

case "$STAGE" in
    probe)
        EXTRA=(--exit-on-first-nvdla-access)
        ;;
    create)
        first=$(grep -m1 -o '\[NVDLA FIRST ACCESS\].*tick=[0-9]*' "$RUN/probe/simout" | sed 's/.*tick=//')
        [[ -n "$first" ]] || { echo "No first access tick in the probe" >&2; exit 1; }
        EXTRA=(--checkpoint-at "$((first - MARGIN_TICKS))" --checkpoint-exit)
        ;;
    restore)
        cpt=$(ls -d "$RUN"/create/cpt.* | tail -1)
        EXTRA=(--restore-from "$cpt")
        ;;
    at)
        EXTRA=(--checkpoint-at "$ARG")
        ;;
    restore-at)
        # CPT=<dir> restores another checkpoint of that tick, e.g. from every_*
        cpt="${CPT:-$RUN/at_$ARG/cpt.$ARG}"
        [[ -f "$cpt/m5.cpt" ]] || { echo "No checkpoint $cpt" >&2; exit 1; }
        EXTRA=(--restore-from "$cpt")
        ;;
    every)
        EXTRA=(--checkpoint-every "$ARG")
        ;;
    layers)
        # The probe gives the tick of the first NVDLA access, as for create
        if [[ ! -f "$RUN/probe/simout" ]]; then
            "$SELF" probe "$TAG"
        fi
        first=$(grep -a -m1 -o '\[NVDLA FIRST ACCESS\].*tick=[0-9]*' "$RUN/probe/simout" | sed 's/.*tick=//')
        [[ -n "$first" ]] || { echo "No first access tick in the probe" >&2; exit 1; }
        EXTRA=(--checkpoint-each-layer --checkpoint-at "$((first - MARGIN_TICKS))")
        ;;
    restore-layer)
        cpt="${CPT:-$(awk -v n="$ARG" '$1 == n {print $4}' "$RUN/layers/layers.txt" 2>/dev/null)}"
        [[ -f "$cpt/m5.cpt" ]] || { echo "No checkpoint for layer $ARG in $RUN/layers/layers.txt" >&2; exit 1; }
        EXTRA=(--restore-from "$cpt")
        ;;
    at-cycle)
        EXTRA=(--checkpoint-at-nvdla-cycle "$ARG")
        ;;
    restore-cycle)
        cpt="${CPT:-$(ls -d "$RUN/at_cycle_$ARG"/cpt.* 2>/dev/null | tail -1)}"
        [[ -f "$cpt/m5.cpt" ]] || { echo "No checkpoint for cycle $ARG" >&2; exit 1; }
        EXTRA=(--restore-from "$cpt")
        ;;
    baseline)
        EXTRA=()
        ;;
    *)
        echo "Unknown stage $STAGE" >&2
        exit 2
        ;;
esac

printf '%q ' "$GEM5" -e -r -d "$OUT" "$CONFIG" "${ARGS[@]}" "${EXTRA[@]}" > command.sh
printf '\n' >> command.sh
set +e
"$GEM5" -e -r -d "$OUT" "$CONFIG" "${ARGS[@]}" "${EXTRA[@]}"
status=$?
set -e
printf '%d\n' "$status" > .run_exit_code
if [[ "$STAGE" == layers ]]; then
    # layer 0 is the checkpoint before the NVDLA starts
    grep -a -E '^\[NVDLA LAYER DONE\]|^Checkpoint written to' simout | awk '
        BEGIN { print "layer cycle tick checkpoint" }
        /^\[NVDLA LAYER DONE\]/ {
            for (i = 1; i <= NF; i++) {
                split($i, kv, "=")
                if (kv[1] == "layer") l = kv[2]
                if (kv[1] == "cycle") c = kv[2]
                if (kv[1] == "tick") t = kv[2]
            }
            pending = 1; next }
        /^Checkpoint written to/ {
            if (pending) { print l, c, t, $4; pending = 0 }
            else { d = $4; sub(/.*cpt\./, "", d); print 0, 0, d, $4 } }' > layers.txt
fi
exit "$status"
