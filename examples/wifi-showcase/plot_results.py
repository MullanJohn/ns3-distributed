#!/usr/bin/env python3
"""
Aggregates and plots continuum-wifi-showcase-example results (run_sweep.py's
and run_buffer_study.py's _stats.csv/_frames.csv outputs).

Four merged figures for the primary sweep (scenario x macVariant fully
crossed): latency components (stacked bars), frame outcomes (stacked bars),
energy (stacked bars), and an end-to-end latency boxplot (completed frames
only, mean marked, 5th/95th-percentile whiskers, no outlier fliers, log
y-scale if the whisker range exceeds 100ms). Each merged figure carries two
legends: a component legend (color = stacked segment) and a scenario legend
(hatch = scenario x macVariant group, using Raw/Cmp for
uncompressed/compressed and EDCA/OFDMA for baseline/ulofdma) placed above
the figure.

All figures are exported as PDF.

Run from the ns-3 root, after run_sweep.py / run_buffer_study.py have
populated contrib/continuum/examples/wifi-showcase/results/<macVariant>/:

    python3 contrib/continuum/examples/wifi-showcase/plot_results.py

Requires matplotlib and pandas (pip install matplotlib pandas).
"""

import argparse
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

FONT_SIZE = 13  # 12-14pt range for all labels/legends/titles
LIGHT_COLORS = ["#AED6F1", "#F9E79F", "#ABEBC6", "#F5B7B1", "#D7BDE2"]
HATCHES = [".", "*", "o", "+"]  # one per scenario x macVariant group

CLIENT_COUNTS = [1, 2, 4, 8]
GROUPS = [
    ("uncompressed", "baseline"),
    ("uncompressed", "ulofdma"),
    ("compressed", "baseline"),
    ("compressed", "ulofdma"),
]
GROUP_FILL_COLORS = LIGHT_COLORS[: len(GROUPS)]  # boxplot fill: one distinct color per group
SCENARIO_LABEL = {"uncompressed": "Raw", "compressed": "Cmp"}
MAC_VARIANT_LABEL = {"baseline": "EDCA", "ulofdma": "OFDMA"}
GROUP_LABELS = [f"{SCENARIO_LABEL[s]}, {MAC_VARIANT_LABEL[m]}" for s, m in GROUPS]

FRAMES_RE = re.compile(r"^n(\d+)_(\w+?)_(\w+)_seed_(\d+)_frames\.csv$")

plt.rcParams.update(
    {
        "font.size": FONT_SIZE,
        "axes.labelsize": FONT_SIZE,
        "axes.titlesize": FONT_SIZE,
        "legend.fontsize": FONT_SIZE,
        "xtick.labelsize": FONT_SIZE,
        "ytick.labelsize": FONT_SIZE,
        "hatch.linewidth": 0.5,  # ".","*","O","+" render very dense at the 1.0 default
    }
)


def load_stats(results_dir: Path) -> pd.DataFrame:
    rows = [pd.read_csv(p) for p in results_dir.glob("**/*_stats.csv") if p.stat().st_size > 0]
    if not rows:
        print(f"error: no *_stats.csv found under {results_dir}", file=sys.stderr)
        sys.exit(1)
    df = pd.concat(rows, ignore_index=True)
    df["totalAttempts"] = df.nSuccess + df.nLate + df.nDropped + df.nRejected
    for col in ["nSuccess", "nLate", "nDropped", "nRejected"]:
        df[col + "Rate"] = df[col] / df["totalAttempts"]
    return df


def load_frames(results_dir: Path) -> pd.DataFrame:
    rows = []
    for p in results_dir.glob("**/*_frames.csv"):
        if p.stat().st_size == 0:
            continue
        m = FRAMES_RE.match(p.name)
        if not m:
            continue
        try:
            df = pd.read_csv(p)
        except pd.errors.EmptyDataError:
            continue
        if df.empty:
            continue
        df["numClients"] = int(m.group(1))
        df["macVariant"] = m.group(2)
        df["scenario"] = m.group(3)
        rows.append(df)
    return pd.concat(rows, ignore_index=True) if rows else pd.DataFrame()


def component_legend_handles(colors, labels):
    return [plt.Rectangle((0, 0), 1, 1, facecolor=c, edgecolor="black") for c in colors], labels


def group_legend_handles(labels, facecolors=None):
    if facecolors is None:
        facecolors = ["white"] * len(labels)
    return [
        plt.Rectangle((0, 0), 1, 1, facecolor=fc, edgecolor="black", hatch=HATCHES[i % len(HATCHES)])
        for i, fc in enumerate(facecolors)
    ], labels


def add_two_legends(fig, ax, comp_colors, comp_labels, group_labels, comp_loc="upper left"):
    handles, labels = component_legend_handles(comp_colors, comp_labels)
    leg1 = ax.legend(handles, labels, loc=comp_loc)
    ax.add_artist(leg1)
    ghandles, glabels = group_legend_handles(group_labels)
    fig.legend(
        ghandles,
        glabels,
        loc="upper center",
        bbox_to_anchor=(0.5, 1.10),
        ncol=len(group_labels),
        frameon=False,
    )


def grouped_stacked_bar(ax, data: pd.DataFrame, mean_cols: list[str]):
    """Groups=nClients on the x-axis; within each group, one bar per
    scenario x macVariant (GROUPS), stacked by mean_cols; hatch encodes the
    group, color encodes the stacked component."""
    n_groups = len(GROUPS)
    n_clients = len(CLIENT_COUNTS)
    bar_width = 0.8 / n_groups
    x = np.arange(n_clients)

    for gi, (scenario, mac_variant) in enumerate(GROUPS):
        subset = data[(data.scenario == scenario) & (data.macVariant == mac_variant)]
        means = subset.groupby("numClients")[mean_cols].mean().reindex(CLIENT_COUNTS)
        xpos = x + (gi - (n_groups - 1) / 2) * bar_width
        bottom = np.zeros(n_clients)
        for ci, col in enumerate(mean_cols):
            vals = means[col].fillna(0).to_numpy()
            color = LIGHT_COLORS[ci % len(LIGHT_COLORS)]
            hatch = HATCHES[gi % len(HATCHES)]
            ax.bar(
                xpos,
                vals,
                bar_width,
                bottom=bottom,
                color=color,
                hatch=hatch,
                edgecolor="black",
                linewidth=0.6,
            )
            bottom += vals

    ax.set_xticks(x)
    ax.set_xticklabels(CLIENT_COUNTS)
    ax.set_xlabel("Number of clients")


def plot_latency(data: pd.DataFrame, out_dir: Path):
    mean_cols = ["uplinkMeanMs", "backendMeanMs", "downlinkMeanMs"]
    comp_labels = ["uplink", "inference (backend)", "downlink"]
    fig, ax = plt.subplots(figsize=(9, 5.5))
    grouped_stacked_bar(ax, data, mean_cols)
    ax.set_ylabel("Latency (ms)")
    add_two_legends(fig, ax, LIGHT_COLORS[: len(mean_cols)], comp_labels, GROUP_LABELS)
    ax.grid(alpha=0.3, axis="y")
    fig.tight_layout()
    fig.savefig(out_dir / "latency_components.pdf", bbox_inches="tight")
    plt.close(fig)


def plot_outcomes(data: pd.DataFrame, out_dir: Path):
    # "rejected" is excluded: with no AdmissionPolicy attached in this
    # showcase (always-admit), rejection can never occur -- only 3 outcomes
    # are actually reachable.
    mean_cols = ["nSuccessRate", "nLateRate", "nDroppedRate"]
    comp_labels = ["success", "late", "dropped"]
    fig, ax = plt.subplots(figsize=(9, 5.5))
    grouped_stacked_bar(ax, data, mean_cols)
    ax.set_ylabel("Fraction of attempts")
    add_two_legends(
        fig, ax, LIGHT_COLORS[: len(mean_cols)], comp_labels, GROUP_LABELS, comp_loc="lower left"
    )
    ax.grid(alpha=0.3, axis="y")
    fig.tight_layout()
    fig.savefig(out_dir / "outcomes.pdf", bbox_inches="tight")
    plt.close(fig)


def plot_energy(data: pd.DataFrame, out_dir: Path):
    mean_cols = ["computeEnergyJ", "apRadioEnergyJ", "staRadioEnergyTotalJ"]
    comp_labels = ["compute", "AP radio", "STA radios (total)"]
    fig, ax = plt.subplots(figsize=(9, 5.5))
    grouped_stacked_bar(ax, data, mean_cols)
    ax.set_ylabel("Energy (J)")
    add_two_legends(fig, ax, LIGHT_COLORS[: len(mean_cols)], comp_labels, GROUP_LABELS)
    ax.grid(alpha=0.3, axis="y")
    fig.tight_layout()
    fig.savefig(out_dir / "energy.pdf", bbox_inches="tight")
    plt.close(fig)


def plot_latency_boxplot(frames: pd.DataFrame, out_dir: Path):
    completed = frames[frames.status.isin(["success", "late"])]
    n_groups = len(GROUPS)
    box_width = 0.8 / n_groups

    positions = []
    box_data = []
    for ci, n_clients in enumerate(CLIENT_COUNTS):
        for gi, (scenario, mac_variant) in enumerate(GROUPS):
            subset = completed[
                (completed.numClients == n_clients)
                & (completed.scenario == scenario)
                & (completed.macVariant == mac_variant)
            ]
            box_data.append(subset["totalLatencyMs"].dropna().to_numpy())
            positions.append(ci + (gi - (n_groups - 1) / 2) * box_width)

    fig, ax = plt.subplots(figsize=(9, 5.5))

    # 5th/95th-percentile whiskers, no outlier fliers plotted.
    bp = ax.boxplot(
        box_data,
        positions=positions,
        widths=box_width * 0.9,
        patch_artist=True,
        showmeans=True,
        showfliers=False,
        whis=[5, 95],
        meanprops=dict(marker="^", markerfacecolor="darkgreen", markeredgecolor="black", markersize=7),
        medianprops=dict(color="black"),
    )
    # Each of the 4 scenario x macVariant groups gets its own distinct fill
    # color (and its own hatch, matching the other merged figures).
    for i, patch in enumerate(bp["boxes"]):
        gi = i % n_groups
        patch.set_facecolor(GROUP_FILL_COLORS[gi])
        patch.set_hatch(HATCHES[gi % len(HATCHES)])
        patch.set_edgecolor("black")

    ax.set_yscale("log")

    ax.set_xticks(range(len(CLIENT_COUNTS)))
    ax.set_xticklabels(CLIENT_COUNTS)
    ax.set_xlim(-0.5, len(CLIENT_COUNTS) - 0.5)
    ax.set_xlabel("Number of clients")
    ax.set_ylabel("End-to-end latency (ms)")
    ax.grid(alpha=0.3, axis="y")

    ghandles, glabels = group_legend_handles(GROUP_LABELS, facecolors=GROUP_FILL_COLORS)
    fig.legend(
        ghandles, glabels, loc="upper center", bbox_to_anchor=(0.5, 1.10), ncol=n_groups, frameon=False
    )
    mean_handle = plt.Line2D(
        [0], [0], marker="^", color="darkgreen", linestyle="None", markersize=7, label="mean"
    )
    ax.legend(handles=[mean_handle], loc="upper left")

    fig.tight_layout()
    fig.savefig(out_dir / "latency_boxplot.pdf", bbox_inches="tight")
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--resultsDir", default="contrib/continuum/examples/wifi-showcase/results"
    )
    parser.add_argument(
        "--outDir", default="contrib/continuum/examples/wifi-showcase/results/plots"
    )
    args = parser.parse_args()

    results_dir = Path(args.resultsDir)
    out_dir = Path(args.outDir)
    out_dir.mkdir(parents=True, exist_ok=True)

    stats = load_stats(results_dir)
    is_buffer_study = stats["scenario"] == "bufferStudy"
    primary = stats[~is_buffer_study]

    if not primary.empty:
        plot_latency(primary, out_dir)
        plot_outcomes(primary, out_dir)
        plot_energy(primary, out_dir)

        frames = load_frames(results_dir)
        frames = frames[frames["scenario"] != "bufferStudy"]
        if not frames.empty:
            plot_latency_boxplot(frames, out_dir)
    else:
        print("note: no primary-sweep rows found (all rows are scenarioLabel=bufferStudy)")

    buffer_study = stats[is_buffer_study]
    if not buffer_study.empty:
        print(
            f"note: {len(buffer_study)} bufferStudy rows present but not plotted by this script "
            "-- buffer-study figures are not part of this merged-figure set"
        )

    print(f"Wrote plots to {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
