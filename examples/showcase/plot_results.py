#!/usr/bin/env python3
#
# Copyright (c) 2025 UCC
# SPDX-License-Identifier: GPL-2.0-only
#
# Reads the per-run CSV triples produced by run_sweep.sh / continuum-showcase-example
# (<...>_client.csv, <...>_orchestrator.csv, <...>_stats.csv) and renders three
# PDF figures, x-axis = tier2Delay, comparing the three schedulers. Never
# touches the simulator -- re-run this any time the presentation needs to
# change (a new threshold, different styling); only re-run the simulation if
# the underlying parameters change.
#
# Usage (from the ns-3 root):
#   python3 contrib/continuum/examples/showcase/plot_results.py \
#       --results-dir contrib/continuum/examples/showcase/results \
#       --fontsize 11

import argparse
import glob
import os
import re

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

# Colorblind-friendly (Okabe-Ito) palette, one color + hatch pattern per scheduler.
SCHEDULER_STYLE = {
    "leastloaded": {"label": "LeastLoaded", "color": "#0072B2", "hatch": "//"},
    "networkaware": {"label": "NetworkAware", "color": "#E69F00", "hatch": "xx"},
    "tiereddelay": {"label": "TieredDelay", "color": "#009E73", "hatch": ".."},
}
SCHEDULER_ORDER = ["leastloaded", "networkaware", "tiereddelay"]
# Short per-scheduler code used in grouped-bar x-tick labels (figures 2 & 3),
# where color/hatch is already spoken for by the stacked category.
SCHEDULER_ABBR = {"leastloaded": "LL", "networkaware": "NA", "tiereddelay": "TD"}

# Workload-status stack (figure 2): intuitive good/warn/bad colors, plus
# hatch redundancy for colorblind accessibility.
STATUS_STYLE = {
    "Success": {"color": "#009E73", "hatch": ""},
    "Late": {"color": "#E69F00", "hatch": "//"},
    "Dropped": {"color": "#D55E00", "hatch": "xx"},
}
STATUS_ORDER = ["Success", "Late", "Dropped"]

# Backend-placement stack (figure 3).
BACKEND_STYLE = {
    "Near (edge)": {"color": "#0072B2", "hatch": ""},
    "Far (core)": {"color": "#CC79A7", "hatch": "//"},
}
BACKEND_ORDER = ["Near (edge)", "Far (core)"]

CLIENT_FILENAME_RE = re.compile(
    r"tier2Delay_(?P<delay>[0-9.]+)ms_(?P<mode>session|workload)_seed_(?P<seed>\d+)_client\.csv$"
)
STATS_FILENAME_RE = re.compile(
    r"tier2Delay_(?P<delay>[0-9.]+)ms_(?P<mode>session|workload)_seed_(?P<seed>\d+)_stats\.csv$"
)


def load_run(client_csv_path, scheduler, tier2_delay, admission_mode, seed):
    orch_csv_path = client_csv_path.replace("_client.csv", "_orchestrator.csv")
    if not os.path.exists(orch_csv_path):
        print(f"warning: missing {orch_csv_path}, skipping {client_csv_path}")
        return None

    client_df = pd.read_csv(client_csv_path)
    orch_df = pd.read_csv(orch_csv_path)
    if client_df.empty:
        return None

    merged = client_df.merge(orch_df, on="taskId", how="inner")
    merged["scheduler"] = scheduler
    merged["tier2Delay"] = tier2_delay
    merged["admissionMode"] = admission_mode
    merged["seed"] = seed
    return merged


def load_all_runs(results_dir):
    """Per-task rows (client x orchestrator, merged on taskId), one per completed task."""
    frames = []
    for scheduler in SCHEDULER_ORDER:
        pattern = os.path.join(results_dir, scheduler, "*_client.csv")
        for client_csv_path in sorted(glob.glob(pattern)):
            match = CLIENT_FILENAME_RE.search(os.path.basename(client_csv_path))
            if not match:
                continue
            tier2_delay = float(match.group("delay"))
            admission_mode = match.group("mode")
            seed = int(match.group("seed"))
            frame = load_run(client_csv_path, scheduler, tier2_delay, admission_mode, seed)
            if frame is not None:
                frames.append(frame)

    if not frames:
        raise SystemExit(f"No runs found under {results_dir}")

    return pd.concat(frames, ignore_index=True)


def load_all_stats(results_dir):
    """One row per run from each *_stats.csv (already self-describing)."""
    frames = []
    for scheduler in SCHEDULER_ORDER:
        pattern = os.path.join(results_dir, scheduler, "*_stats.csv")
        for stats_csv_path in sorted(glob.glob(pattern)):
            if not STATS_FILENAME_RE.search(os.path.basename(stats_csv_path)):
                continue
            frame = pd.read_csv(stats_csv_path)
            if not frame.empty:
                frames.append(frame)

    if not frames:
        raise SystemExit(f"No *_stats.csv files found under {results_dir}")

    stats_df = pd.concat(frames, ignore_index=True)
    stats_df = stats_df.rename(columns={"tier2DelayMs": "tier2Delay"})
    return stats_df


def style_boxplot(bp, color, hatch):
    for box in bp["boxes"]:
        box.set_facecolor(color)
        box.set_hatch(hatch)
        box.set_edgecolor("black")
    for element in ("whiskers", "caps", "medians"):
        for line in bp[element]:
            line.set_color("black")


def plot_latency_boxplot(df, delays, ylabel, out_path, fontsize):
    """Grouped boxplot per scheduler at each tier2Delay, outliers hidden, mean marked."""
    fig, ax = plt.subplots(figsize=(7, 4.5))

    n_schedulers = len(SCHEDULER_ORDER)
    group_width = 0.8
    box_width = group_width / n_schedulers

    legend_handles = []
    for s_idx, scheduler in enumerate(SCHEDULER_ORDER):
        style = SCHEDULER_STYLE[scheduler]
        positions = []
        samples = []
        for d_idx, delay in enumerate(delays):
            subset = df[(df["scheduler"] == scheduler) & (df["tier2Delay"] == delay)]
            per_workload = subset.drop_duplicates(
                subset=["scheduler", "tier2Delay", "seed", "dagId"]
            )
            values = per_workload["workloadLatencyMs"].to_numpy()
            if len(values) == 0:
                continue
            offset = (s_idx - (n_schedulers - 1) / 2) * box_width
            positions.append(d_idx + offset)
            samples.append(values)

        if not samples:
            continue

        bp = ax.boxplot(
            samples,
            positions=positions,
            widths=box_width * 0.9,
            patch_artist=True,
            showmeans=True,
            showfliers=False,
            meanprops={
                "marker": "D",
                "markerfacecolor": "white",
                "markeredgecolor": "black",
                "markersize": 5,
            },
        )
        style_boxplot(bp, style["color"], style["hatch"])
        legend_handles.append(
            plt.Rectangle(
                (0, 0),
                1,
                1,
                facecolor=style["color"],
                hatch=style["hatch"],
                edgecolor="black",
                label=style["label"],
            )
        )

    ax.set_xticks(range(len(delays)))
    ax.set_xticklabels([str(d) for d in delays])
    ax.set_xlabel("tier2Delay (ms)", fontsize=fontsize)
    ax.set_ylabel(ylabel, fontsize=fontsize)
    ax.tick_params(labelsize=fontsize)
    ax.legend(handles=legend_handles, fontsize=fontsize, loc="best")
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)
    print(f"wrote {out_path}")


def plot_stacked_bars_vs_tier2delay(
    values_fn, delays, categories, style, ylabel, out_path, fontsize
):
    """
    Grouped-and-stacked bars: one dodged bar per scheduler at each tier2Delay,
    each bar stacked into `categories` segments (color+hatch from `style`).
    Scheduler identity is carried by two-line x-tick labels ("<delay>\\n<abbr>")
    since color/hatch is already spoken for by the stacked category.
    values_fn(scheduler, delay) -> dict category -> value.
    """
    fig, ax = plt.subplots(figsize=(8.5, 4.5))

    n_schedulers = len(SCHEDULER_ORDER)
    group_width = 0.8
    bar_width = group_width / n_schedulers

    tick_positions = []
    tick_labels = []

    for s_idx, scheduler in enumerate(SCHEDULER_ORDER):
        offset = (s_idx - (n_schedulers - 1) / 2) * bar_width
        positions = [d_idx + offset for d_idx in range(len(delays))]
        bottoms = [0.0] * len(delays)

        for delay, pos in zip(delays, positions):
            tick_positions.append(pos)
            tick_labels.append(f"{delay:g}\n{SCHEDULER_ABBR[scheduler]}")

        for cat in categories:
            values = [values_fn(scheduler, delay).get(cat, 0.0) for delay in delays]
            cat_style = style[cat]
            ax.bar(
                positions,
                values,
                width=bar_width * 0.9,
                bottom=bottoms,
                facecolor=cat_style["color"],
                hatch=cat_style["hatch"],
                edgecolor="black",
                label=cat if s_idx == 0 else None,
            )
            bottoms = [b + v for b, v in zip(bottoms, values)]

    ax.set_xticks(tick_positions)
    ax.set_xticklabels(tick_labels)
    ax.set_xlabel("tier2Delay (ms) / scheduler", fontsize=fontsize)
    ax.set_ylabel(ylabel, fontsize=fontsize)
    ax.tick_params(labelsize=fontsize)
    ax.legend(fontsize=fontsize, loc="best")
    fig.tight_layout()
    fig.savefig(out_path)
    plt.close(fig)
    print(f"wrote {out_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--results-dir",
        default="contrib/continuum/examples/showcase/results",
        help="Root directory containing <scheduler>/*_client.csv, *_orchestrator.csv, *_stats.csv",
    )
    parser.add_argument(
        "--fontsize", type=float, default=11, help="Base font size for figure text"
    )
    parser.add_argument(
        "--deadline-violation-ms",
        type=float,
        default=None,
        help="Override the deadline threshold (ms) used to classify Late workloads; "
        "defaults to each row's own deadlineBudgetMs column",
    )
    parser.add_argument(
        "--admission-mode",
        default="session",
        choices=["session", "workload"],
        help="Which OffloadClient admission mode's runs to plot (default: session). "
        "Session- and workload-mode runs are never pooled together, since they "
        "represent different experiments.",
    )
    args = parser.parse_args()

    df = load_all_runs(args.results_dir)
    df = df[df["admissionMode"] == args.admission_mode]
    if df.empty:
        raise SystemExit(
            f"No runs found with admissionMode={args.admission_mode!r} under {args.results_dir}"
        )

    stats_df = load_all_stats(args.results_dir)
    stats_df = stats_df[stats_df["admissionMode"] == args.admission_mode]
    if stats_df.empty:
        raise SystemExit(
            f"No *_stats.csv runs found with admissionMode={args.admission_mode!r} "
            f"under {args.results_dir}"
        )

    delays = sorted(df["tier2Delay"].unique())

    figures_dir = os.path.join(args.results_dir, "figures")
    os.makedirs(figures_dir, exist_ok=True)
    suffix = f"_{args.admission_mode}"

    # --- Figure 1: end-to-end workload latency (outliers hidden, mean marked) ---
    plot_latency_boxplot(
        df,
        delays,
        "End-to-end workload latency (ms)",
        os.path.join(figures_dir, f"latency_boxplot{suffix}.pdf"),
        args.fontsize,
    )

    # --- Figure 2: workload status (Success / Late / Dropped), as a share of ---
    # --- every workload the client attempted to generate (sent + dropped).  ---
    per_workload = df.drop_duplicates(subset=["scheduler", "tier2Delay", "seed", "dagId"]).copy()
    threshold = (
        args.deadline_violation_ms
        if args.deadline_violation_ms is not None
        else per_workload["deadlineBudgetMs"]
    )
    per_workload["status"] = pd.Series(
        pd.Categorical(["Late"] * len(per_workload), categories=STATUS_ORDER)
    ).to_numpy()
    per_workload.loc[per_workload["workloadLatencyMs"] <= threshold, "status"] = "Success"
    per_workload.loc[per_workload["workloadLatencyMs"] > threshold, "status"] = "Late"

    responded_counts = (
        per_workload.groupby(["scheduler", "tier2Delay", "status"])
        .size()
        .unstack(fill_value=0)
    )
    dropped_counts = stats_df.groupby(["scheduler", "tier2Delay"])["tasksDropped"].sum()

    def status_values(scheduler, delay):
        counts = {"Success": 0.0, "Late": 0.0, "Dropped": 0.0}
        if (scheduler, delay) in responded_counts.index:
            row = responded_counts.loc[(scheduler, delay)]
            counts["Success"] = float(row.get("Success", 0))
            counts["Late"] = float(row.get("Late", 0))
        counts["Dropped"] = float(dropped_counts.get((scheduler, delay), 0))
        total = sum(counts.values())
        if total == 0:
            return {k: 0.0 for k in counts}
        return {k: 100.0 * v / total for k, v in counts.items()}

    plot_stacked_bars_vs_tier2delay(
        status_values,
        delays,
        STATUS_ORDER,
        STATUS_STYLE,
        "Share of generated workloads (%)",
        os.path.join(figures_dir, f"workload_status{suffix}.pdf"),
        args.fontsize,
    )

    # --- Figure 3: jobs (tasks) processed on the near (edge) vs. far (core) backend ---
    backend_counts = df.groupby(["scheduler", "tier2Delay", "backendIdx"]).size().unstack(
        fill_value=0
    )

    def backend_values(scheduler, delay):
        counts = {"Near (edge)": 0.0, "Far (core)": 0.0}
        if (scheduler, delay) in backend_counts.index:
            row = backend_counts.loc[(scheduler, delay)]
            counts["Near (edge)"] = float(row.get(0, 0))
            counts["Far (core)"] = float(row.get(1, 0))
        return counts

    plot_stacked_bars_vs_tier2delay(
        backend_values,
        delays,
        BACKEND_ORDER,
        BACKEND_STYLE,
        "Number of jobs processed",
        os.path.join(figures_dir, f"backend_placement{suffix}.pdf"),
        args.fontsize,
    )


if __name__ == "__main__":
    main()
