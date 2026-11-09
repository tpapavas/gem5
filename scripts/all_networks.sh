#!/usr/bin/env bash
# Checkpoint test on every network of the Minor campaign
# (nvdla_cache, --dlas 1, same loadable, image and flags as that campaign).
#
#   all_networks.sh run [max_procs]   baseline + layers + 2 restores per config
#   all_networks.sh report            table of the finished runs
#   all_networks.sh list              the configs, longest first
#
# Per <model>_<prec> (runs/<model>_<prec>/):
#   baseline            plain run, the reference output
#   layers              checkpoint before the NVDLA and after every layer
#   restore_layer_<m>   run to the end from the middle layer
#   restore_layer_<L>   run to the end from the last layer
# A config passes when the output.dimg of all four is identical.
# GEM5=<absolute binary> picks another build (see system_ckpt.sh).
set -Eeuo pipefail

SELF="$(readlink -f "$0")"
TASK=/data/ngiannopoulos/Phd/tasks/task5_gem5_nvdla_checkpoint
CKPT="$TASK/scripts/system_ckpt.sh"
QUANT2=/data/ngiannopoulos/Phd/NVDLA/nvdla_quantization_2/out
IMG_ROOT=/data/ngiannopoulos/Phd/NVDLA/nvdla_quantization/out_verified_after_fix_pdp_v2/run_20261102_124920_4ynwxp
# Host hours of the Minor campaign, longest first, to start those first
CONFIGS=(
    densenet:cifar10:int8 densenet:cifar10:fp16 resnet34:cifar10:fp16
    alexnet:cifar10:fp16 resnet34:cifar10:int8 alexnet:cifar10:int8
    resnet18:cifar10:fp16 vgg11:cifar10:fp16 resnet18:cifar10:int8
    vgg11:cifar10:int8 squeezenet:cifar10:int8 squeezenet:cifar10:fp16
    googlenet:cifar10:fp16 googlenet:cifar10:int8 lenet5:mnist:fp16
    lenet5:mnist:int8
)

setup() { # <model> <dataset> <prec>: sets TAG, LOADABLE, IMAGE, SE_EXTRA
    TAG="$1_$3"
    LOADABLE="$QUANT2/$1/$2/onnc_$3_out/$3_onnc.nvdla"
    if [[ "$1" == lenet5 ]]; then
        IMAGE="$IMG_ROOT/lenet5/mnist/vp_in_gray/vp_in_000.pgm"
    else
        IMAGE="$IMG_ROOT/densenet/cifar10/onnc_fp16_out/vp_in_000.jpg"
    fi
    # FP16 gets --normalize 255, INT8 gets no --normalize at all
    SE_EXTRA=""
    [[ "$3" != fp16 ]] || SE_EXTRA="--normalize 255"
    [[ -f "$LOADABLE" && -f "$IMAGE" ]] || { echo "missing input for $TAG" >&2; exit 2; }
    export LOADABLE IMAGE SE_EXTRA
}

sha() { [[ -f "$1/output.dimg" ]] && sha256sum "$1/output.dimg" | cut -c1-16 || echo "-"; }

case "${1:-}" in
    list)
        printf '%s\n' "${CONFIGS[@]}"
        ;;
    run)
        P="${2:-12}"
        mkdir -p "$TASK/logs/all_networks"
        for c in "${CONFIGS[@]}"; do
            IFS=: read -r m d p <<< "$c"
            echo "chain $m $d $p"
            echo "baseline $m $d $p"
        done | xargs -P "$P" -L1 "$SELF" worker
        "$SELF" report
        ;;
    worker)
        kind="$2"; setup "$3" "$4" "$5"
        L="$TASK/logs/all_networks/${TAG}_$kind.log"
        case "$kind" in
            baseline)
                "$CKPT" baseline "$TAG" > "$L" 2>&1 || echo "baseline failed: $TAG" >> "$L"
                ;;
            chain)
                if "$CKPT" layers "$TAG" > "$L" 2>&1; then
                    n=$(($(wc -l < "$TASK/runs/$TAG/layers/layers.txt") - 2)) # layers, without the header and layer 0
                    mid=$(( (n + 1) / 2 ))
                    echo "layers=$n mid=$mid last=$n" > "$TASK/runs/$TAG/chain_layers.txt"
                    "$CKPT" restore-layer "$TAG" "$mid" > "${L%.log}_restore_mid.log" 2>&1 &
                    [[ "$n" == "$mid" ]] || "$CKPT" restore-layer "$TAG" "$n" > "${L%.log}_restore_last.log" 2>&1 &
                    wait
                else
                    echo "layers failed: $TAG" >> "$L"
                fi
                ;;
            *) echo "unknown kind $kind" >&2; exit 2 ;;
        esac
        ;;
    report)
        out="$TASK/results/all_networks_minor.tsv"
        mkdir -p "$TASK/results"
        {
            printf 'run\tlayers\tmid\tbaseline_exit\tlayers_exit\tmid_exit\tlast_exit\tsha_baseline\tsha_layers\tsha_mid\tsha_last\tcyc_baseline\tcyc_layers\tcyc_mid\tcyc_last\thost_h_baseline\thost_h_layers\tverdict\n'
            for c in "${CONFIGS[@]}"; do
                IFS=: read -r m d p <<< "$c"; TAG="${m}_$p"; R="$TASK/runs/$TAG"
                [[ -d "$R" ]] || continue
                n="-"; mid="-"
                if [[ -f "$R/chain_layers.txt" ]]; then
                    n=$(sed -E 's/.*layers=([0-9]+).*/\1/' "$R/chain_layers.txt")
                    mid=$(sed -E 's/.*mid=([0-9]+).*/\1/' "$R/chain_layers.txt")
                fi
                dirs=("$R/baseline" "$R/layers" "$R/restore_layer_$mid" "$R/restore_layer_$n")
                ex=(); sh=(); cy=()
                for x in "${dirs[@]}"; do
                    ex+=("$(cat "$x/.run_exit_code" 2>/dev/null || echo -)")
                    sh+=("$(sha "$x")")
                    cy+=("$(grep -a -m1 'system.nvdla.nvdla_cycles' "$x/stats.txt" 2>/dev/null | awk '{print $2}')")
                done
                hb=$(grep -a -m1 '^hostSeconds' "$R/baseline/stats.txt" 2>/dev/null | awk '{printf "%.2f", $2/3600}')
                hl=$(grep -a -m1 '^hostSeconds' "$R/layers/stats.txt" 2>/dev/null | awk '{printf "%.2f", $2/3600}')
                v=FAIL
                if [[ "${sh[0]}" != "-" && "${sh[0]}" == "${sh[1]}" && "${sh[0]}" == "${sh[2]}" && "${sh[0]}" == "${sh[3]}" ]]; then v=PASS; fi
                [[ "${sh[0]}" != "-" && "${sh[1]}" != "-" && "${sh[2]}" != "-" && "${sh[3]}" != "-" ]] || v=INCOMPLETE
                printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$TAG" "$n" "$mid" \
                    "${ex[0]}" "${ex[1]}" "${ex[2]}" "${ex[3]}" "${sh[0]}" "${sh[1]}" "${sh[2]}" "${sh[3]}" \
                    "${cy[0]}" "${cy[1]}" "${cy[2]}" "${cy[3]}" "${hb:--}" "${hl:--}" "$v"
            done
        } | tee "$out" | column -t -s $'\t'
        ;;
    *)
        sed -n 2,16p "$0"
        exit 2
        ;;
esac
