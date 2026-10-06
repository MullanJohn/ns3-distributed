#!/usr/bin/env bash
#
# Copyright (c) 2025 UCC
# SPDX-License-Identifier: GPL-2.0-only
#
# Sweeps continuum-showcase-example across every (scheduler, tier2Delay, seed)
# combination, writing per-run CSV logs. Re-run plot_results.py against the
# accumulated output any time the presentation needs to change -- there is no
# need to re-run this script unless the simulation parameters themselves
# change.
#
# Usage (from the ns-3 root, one level above contrib/):
#   contrib/continuum/examples/showcase/run_sweep.sh [-- extra example args...]
#
# Env var overrides:
#   SCHEDULERS   space-separated scheduler names (default: "leastloaded networkaware tiereddelay")
#   TIER2_DELAYS space-separated tier2Delay values in ms (default: "2 4 6 8 10")
#   RUNS         repetitions per (scheduler, tier2Delay) combination, i.e. RngRun 1..RUNS (default: 10)
#   OUTPUT_DIR   root directory for CSV logs (default: contrib/continuum/examples/showcase/results)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NS3_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
BINARY="${NS3_ROOT}/build/contrib/continuum/examples/ns3.44-continuum-showcase-example-default"

if [[ ! -x "${BINARY}" ]]; then
    echo "error: ${BINARY} not found or not executable." >&2
    echo "Build it first: cmake --build cmake-cache --target continuum-showcase-example" >&2
    exit 1
fi

SCHEDULERS="${SCHEDULERS:-leastloaded networkaware tiereddelay}" # <<<<<<<<<<<<<<<<<<<<<
TIER2_DELAYS="${TIER2_DELAYS:-0.5 1 2 4 8}"
RUNS="${RUNS:-10}"  #<<<<<<<<<<<<<<<<<<<<<
OUTPUT_DIR="${OUTPUT_DIR:-contrib/continuum/examples/showcase/results}"

cd "${NS3_ROOT}"

total=0
failed=0

for scheduler in ${SCHEDULERS}; do
    for delay in ${TIER2_DELAYS}; do
        for ((run = 1; run <= RUNS; run++)); do
            total=$((total + 1))
            echo "[${total}] scheduler=${scheduler} tier2DelayMs=${delay} RngRun=${run}"
            if ! "${BINARY}" \
                --scheduler="${scheduler}" \
                --tier2DelayMs="${delay}" \
                --RngRun="${run}" \
                --outputDir="${OUTPUT_DIR}" \
                "$@" > /dev/null; then
                echo "  FAILED (scheduler=${scheduler}, tier2DelayMs=${delay}, RngRun=${run})" >&2
                failed=$((failed + 1))
            fi
        done
    done
done

echo ""
echo "Sweep complete: ${total} runs, ${failed} failed."
echo "Logs under: ${NS3_ROOT}/${OUTPUT_DIR}"

if ((failed > 0)); then
    exit 1
fi
