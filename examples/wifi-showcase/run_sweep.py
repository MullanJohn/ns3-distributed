#!/usr/bin/env python3
"""
Drives the primary continuum-wifi-showcase-example experiment matrix:

    scenario     in {uncompressed, compressed}       (frame-size distribution)
  x macVariant   in {baseline, ulofdma}               (AP MAC configuration)
  x nClients     in {1, 2, 4, 8}                      (control parameter)
  x seed         in {1..10}                           (repeats)

  = 2 x 2 x 4 x 10 = 160 runs.

nClients >= --highLoadClientThreshold (default 8) use a shorter --simTimeHighLoad
instead of --simTime: per-station MAC-layer contention cost scales steeply with
client count in this ns-3 build (measured directly: 8 clients at simTime=5.0
takes minutes per run regardless of payload size), so keeping the full sweep's
wall-clock cost bounded means trading fewer samples per repeat at high client
counts specifically, rather than cutting frame rate or accepting an
unbounded-length run.

Every workload parameter (resolution, uplink/downlink compression factors,
frame rate) is a CLI argument (see sweep_common.py) with a default -- nothing
here is hardcoded that you can't override. MpduBufferSize/BE_MaxAmpduSize are
passed through at the example's own defaults; see run_buffer_study.py to vary
those specifically.

Run from the ns-3 root:

    python3 contrib/continuum/examples/wifi-showcase/run_sweep.py
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

import sweep_common

DEFAULT_BINARY = (
    "build/contrib/continuum/examples/ns3.44-continuum-wifi-showcase-example-default"
)

CLIENT_COUNTS = [1, 2, 4, 8]
MAC_VARIANTS = ["baseline", "ulofdma"]
SCENARIOS = ["uncompressed", "compressed"]
SEEDS = list(range(1, 11))  # 10 repeats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default=DEFAULT_BINARY, help="Path to the built example")
    parser.add_argument(
        "--simTime",
        type=float,
        default=5.0,
        help="Simulated duration for nClients below --highLoadClientThreshold",
    )
    parser.add_argument(
        "--simTimeHighLoad",
        type=float,
        default=2.0,
        help="Simulated duration for nClients at/above --highLoadClientThreshold",
    )
    parser.add_argument("--highLoadClientThreshold", type=int, default=8)
    parser.add_argument(
        "--outputDir",
        default="contrib/continuum/examples/wifi-showcase/results",
        help="Root CSV output directory",
    )
    parser.add_argument(
        "--dryRun", action="store_true", help="Print the commands without running them"
    )
    sweep_common.add_workload_args(parser)
    args = parser.parse_args()

    binary = Path(args.binary)
    if not args.dryRun and not binary.is_file():
        print(f"error: binary not found at {binary} (build it first, or pass --binary)", file=sys.stderr)
        return 1

    total = len(SCENARIOS) * len(MAC_VARIANTS) * len(CLIENT_COUNTS) * len(SEEDS)
    run_idx = 0
    start = time.monotonic()

    for scenario in SCENARIOS:
        for mac_variant in MAC_VARIANTS:
            for n_clients in CLIENT_COUNTS:
                for seed in SEEDS:
                    run_idx += 1
                    sim_time = (
                        args.simTimeHighLoad
                        if n_clients >= args.highLoadClientThreshold
                        else args.simTime
                    )
                    cmd = [
                        str(binary),
                        f"--numClients={n_clients}",
                        f"--simTime={sim_time}",
                        f"--macVariant={mac_variant}",
                        f"--scenarioLabel={scenario}",
                        f"--outputDir={args.outputDir}",
                        f"--RngRun={seed}",
                        *sweep_common.scenario_flags(args, scenario),
                    ]
                    elapsed = time.monotonic() - start
                    print(
                        f"[{run_idx}/{total}] (+{elapsed:6.0f}s) scenario={scenario} "
                        f"macVariant={mac_variant} nClients={n_clients} simTime={sim_time} seed={seed}"
                    )
                    if args.dryRun:
                        print("  " + " ".join(cmd))
                        continue
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    if result.returncode != 0:
                        print(f"  FAILED (exit {result.returncode}):", file=sys.stderr)
                        print(result.stdout[-2000:], file=sys.stderr)
                        print(result.stderr[-2000:], file=sys.stderr)

    print(f"Done: {run_idx} runs in {time.monotonic() - start:.0f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
