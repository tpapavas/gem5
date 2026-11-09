#!/usr/bin/env bash
# One line per run directory: exit code, output.dimg SHA-256, final tick,
# NVDLA cycles (absolute, also after a restore), CSB writes since the start
# of that run, host seconds.
#   compare_runs.sh <run_dir>...
set -uo pipefail

printf '%-28s %4s %-16s %14s %10s %5s %7s\n' run exit output_sha256 final_tick nvdla_cyc csb_w host_s
for d in "$@"; do
    exit_code=$(cat "$d/.run_exit_code" 2>/dev/null || echo "-")
    sha=$( [[ -f "$d/output.dimg" ]] && sha256sum "$d/output.dimg" | cut -c1-16 || echo "-")
    stats="$d/stats.txt"
    # A run with periodic checkpoints dumps stats once, at the end
    final=$(grep -m1 '^finalTick' "$stats" 2>/dev/null | awk '{print $2}')
    cycles=$(grep -m1 'system.nvdla.nvdla_cycles' "$stats" 2>/dev/null | awk '{print $2}')
    csb=$(grep -m1 'system.nvdla.nvdla_csb_writes' "$stats" 2>/dev/null | awk '{print $2}')
    host=$(grep -m1 '^hostSeconds' "$stats" 2>/dev/null | awk '{print $2}')
    printf '%-28s %4s %-16s %14s %10s %5s %7s\n' "$(basename "$d")" "$exit_code" "$sha" \
        "${final:--}" "${cycles:--}" "${csb:--}" "${host:--}"
done
