#!/usr/bin/env bash
# NVDLA cycles of op enables, interrupts and busy DBB windows, from a simout,
# to choose the cycle of --checkpoint-at-nvdla-cycle.
#   nvdla_timeline.sh <simout> [min_gap_cycles]
# A window ends when DBB is idle for more than min_gap_cycles (default 20000).
set -euo pipefail
# The simout has non-text bytes, so grep -a, or it stops at "Binary file"

SIMOUT="${1:?usage: nvdla_timeline.sh <simout> [min_gap_cycles]}"
GAP="${2:-20000}"

echo "# op enables (write 1 to D_OP_ENABLE) and interrupts"
{
    grep -a -o -E 'WriteReq addr=0x(5010|6008|7008|8008|9008|a008|b038|c008|d008|e008|f008) size=4\([0-9]+\) write to nvdla: addr [0-9a-f]+, data 00000001' "$SIMOUT" |
        sed -E 's/WriteReq addr=0x([0-9a-f]+) size=4\(([0-9]+)\).*/\2 \1/' |
        awk 'BEGIN { n["5010"]="CDMA"; n["6008"]="CSC"; n["7008"]="CMAC_A";
                     n["8008"]="CMAC_B"; n["9008"]="CACC"; n["a008"]="SDP_RDMA";
                     n["b038"]="SDP"; n["c008"]="PDP_RDMA"; n["d008"]="PDP";
                     n["e008"]="CDP_RDMA"; n["f008"]="CDP" }
             { print $1, "enable", n[$2] }'
    grep -a -o -E '^\([0-9]+\) nvdla#0 interrupt!' "$SIMOUT" | tr -d '()' |
        awk '{ print $1, "interrupt" }'
} | sort -n -k1,1

echo
echo "# busy windows: DBB requests from the NVDLA"
grep -a -o -E '^\([0-9]+\) nvdla#0 DBB: (read|write) request' "$SIMOUT" |
    sed -E 's/^\(([0-9]+)\).*/\1/' |
    awk -v gap="$GAP" '
        NR == 1 { start = $1; prev = $1; next }
        $1 - prev > gap { printf "%d - %d (%d cycles)\n", start, prev, prev - start; start = $1 }
        { prev = $1 }
        END { if (NR) printf "%d - %d (%d cycles)\n", start, prev, prev - start }'
