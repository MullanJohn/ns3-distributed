"""
Shared workload-parameter CLI surface for run_sweep.py and run_buffer_study.py.

Everything about the depth-estimation workload (resolution, uplink/downlink
compression ratios, frame rate) is exposed here as CLI-overridable arguments
with defaults, rather than hardcoded -- so any of these can be tuned from the
command line to explore further scenarios without editing either script.
"""

import argparse


def add_workload_args(parser: argparse.ArgumentParser) -> None:
    # Base (uncompressed) resolution. 640x360 is deliberately chosen to
    # stress the system at 8 clients without compression.
    parser.add_argument("--baseWidth", type=int, default=640)
    parser.add_argument("--baseHeight", type=int, default=360)
    parser.add_argument(
        "--inputBytesPerPixel", type=float, default=3.0, help="Uplink (RGB camera frame)"
    )
    parser.add_argument(
        "--outputBytesPerPixel", type=float, default=2.0, help="Downlink (16-bit depth map)"
    )

    # Uplink and downlink compress differently (different content: natural
    # camera video vs. smoother/lower-entropy depth data) -- independent
    # knobs, not a single shared ratio. Depth maps compress *less* well than
    # RGB video in practice, hence the lower default ratio.
    parser.add_argument(
        "--uplinkCompressionFactor",
        type=float,
        default=10.0,
        help="Compressed uplink mean = uncompressed mean / this factor",
    )
    parser.add_argument(
        "--downlinkCompressionFactor",
        type=float,
        default=5.0,
        help=(
            "Compressed downlink mean = uncompressed mean / this factor. "
            "Depth maps compress less well than RGB video in practice; "
            "reasonable range to explore is 2-5."
        ),
    )
    parser.add_argument(
        "--rangeFraction",
        type=float,
        default=0.10,
        help="Uniform half-width (compressed scenario) as a fraction of the mean",
    )

    parser.add_argument("--frameRateHz", type=float, default=30.0)


def uncompressed_means(args) -> tuple[float, float]:
    input_mean = args.baseWidth * args.baseHeight * args.inputBytesPerPixel
    output_mean = args.baseWidth * args.baseHeight * args.outputBytesPerPixel
    return input_mean, output_mean


def scenario_flags(args, scenario: str) -> list[str]:
    input_mean, output_mean = uncompressed_means(args)
    flags = [f"--frameRateHz={args.frameRateHz}"]
    if scenario == "uncompressed":
        flags += [
            "--inputSizeMode=fixed",
            f"--inputSizeMean={input_mean}",
            "--outputSizeMode=fixed",
            f"--outputSizeMean={output_mean}",
        ]
    elif scenario == "compressed":
        flags += [
            "--inputSizeMode=uniform",
            f"--inputSizeMean={input_mean / args.uplinkCompressionFactor}",
            f"--inputSizeRangeFraction={args.rangeFraction}",
            "--outputSizeMode=uniform",
            f"--outputSizeMean={output_mean / args.downlinkCompressionFactor}",
            f"--outputSizeRangeFraction={args.rangeFraction}",
        ]
    else:
        raise ValueError(f"unknown scenario {scenario!r}")
    return flags
