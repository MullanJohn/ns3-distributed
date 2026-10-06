#!/usr/bin/env python3
"""
Follow-up study (not the primary sweep): explores MpduBufferSize x
BE_MaxAmpduSize tuning, fully crossed with macVariant, at a deliberately
narrower scope than run_sweep.py -- everything about that scope is itself
CLI-overridable, nothing hardcoded:

    mpduBufferSize in {512, 1024}
  x maxAmpduSize    in {1 MiB, 3 MiB}
  x macVariant      in {baseline, ulofdma}
  x nClients        in --nClientsList (default: just 8, the stress case)
  x seed            in 1..--seeds (default 5)

Default run count: 2 x 2 x 2 x 1 x 5 = 40. Widen with e.g.
--nClientsList=4,8 or --seeds=10 if you want more coverage.

Workload defaults to the "uncompressed" scenario (the stress case this study
is about) via sweep_common, but every resolution/compression/frame-rate
argument there is still overridable.

Run from the ns-3 root, after building the example:

    python3 contrib/continuum/examples/wifi-showcase/run_buffer_study.py
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

MPDU_BUFFER_SIZES = [512, 1024]
MAX_AMPDU_SIZES = [1048576, 3145728]  # 1 MiB, 3 MiB
MAC_VARIANTS = ["baseline", "ulofdma"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default=DEFAULT_BINARY)
    parser.add_argument("--simTime", type=float, default=2.0)
    parser.add_argument(
        "--nClientsList", default="8", help="Comma-separated client counts, e.g. 4,8"
    )
    parser.add_argument("--seeds", type=int, default=5, help="Number of repeats (seeds 1..N)")
    parser.add_argument("--scenario", default="uncompressed", choices=["uncompressed", "compressed"])
    parser.add_argument(
        "--outputDir", default="contrib/continuum/examples/wifi-showcase/results"
    )
    parser.add_argument("--dryRun", action="store_true")
    sweep_common.add_workload_args(parser)
    args = parser.parse_args()

    binary = Path(args.binary)
    if not args.dryRun and not binary.is_file():
        print(f"error: binary not found at {binary} (build it first, or pass --binary)", file=sys.stderr)
        return 1

    client_counts = [int(n) for n in args.nClientsList.split(",")]
    seeds = list(range(1, args.seeds + 1))

    total = (
        len(MPDU_BUFFER_SIZES) * len(MAX_AMPDU_SIZES) * len(MAC_VARIANTS) * len(client_counts) * len(seeds)
    )
    run_idx = 0
    start = time.monotonic()

    for mpdu_buffer_size in MPDU_BUFFER_SIZES:
        for max_ampdu_size in MAX_AMPDU_SIZES:
            for mac_variant in MAC_VARIANTS:
                for n_clients in client_counts:
                    for seed in seeds:
                        run_idx += 1
                        cmd = [
                            str(binary),
                            f"--numClients={n_clients}",
                            f"--simTime={args.simTime}",
                            f"--macVariant={mac_variant}",
                            f"--mpduBufferSize={mpdu_buffer_size}",
                            f"--maxAmpduSize={max_ampdu_size}",
                            "--scenarioLabel=bufferStudy",
                            f"--outputDir={args.outputDir}",
                            f"--RngRun={seed}",
                            *sweep_common.scenario_flags(args, args.scenario),
                        ]
                        elapsed = time.monotonic() - start
                        print(
                            f"[{run_idx}/{total}] (+{elapsed:6.0f}s) mpduBufferSize={mpdu_buffer_size} "
                            f"maxAmpduSize={max_ampdu_size} macVariant={mac_variant} "
                            f"nClients={n_clients} seed={seed}"
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
