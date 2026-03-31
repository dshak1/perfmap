from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Rectangle


ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets"
ASSETS.mkdir(parents=True, exist_ok=True)

plt.rcParams.update(
    {
        "figure.facecolor": "#f7f4ee",
        "axes.facecolor": "#f7f4ee",
        "savefig.facecolor": "#f7f4ee",
        "font.family": "DejaVu Sans",
    }
)


def save_memory_layout() -> None:
    fig, ax = plt.subplots(figsize=(14, 8))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 10)
    ax.axis("off")

    ax.text(
        0.5,
        9.2,
        "Two Mental Models",
        fontsize=28,
        fontweight="bold",
        color="#0f172a",
    )
    ax.text(
        0.5,
        8.45,
        "The workshop is really about memory layout, not just hash maps.",
        fontsize=15,
        color="#334155",
    )

    ax.text(1.1, 7.5, "std::unordered_map", fontsize=22, fontweight="bold", color="#7c2d12")
    ax.text(7.7, 7.5, "PerfMap", fontsize=22, fontweight="bold", color="#0f766e")

    # Chained buckets.
    bucket_x = 1.0
    for row in range(3):
        y = 6.2 - row * 1.7
        ax.add_patch(
            FancyBboxPatch(
                (bucket_x, y),
                1.8,
                0.9,
                boxstyle="round,pad=0.03,rounding_size=0.08",
                linewidth=1.8,
                edgecolor="#c2410c",
                facecolor="#ffedd5",
            )
        )
        ax.text(bucket_x + 0.22, y + 0.33, f"bucket {row}", fontsize=13, color="#7c2d12")
        prev_x = bucket_x + 2.4
        for node in range(2 + (row % 2)):
            ax.add_patch(
                FancyBboxPatch(
                    (prev_x, y),
                    1.45,
                    0.9,
                    boxstyle="round,pad=0.03,rounding_size=0.08",
                    linewidth=1.5,
                    edgecolor="#9a3412",
                    facecolor="#fff7ed",
                )
            )
            ax.text(prev_x + 0.27, y + 0.33, f"node {node}", fontsize=12, color="#7c2d12")
            ax.annotate(
                "",
                xy=(prev_x - 0.12, y + 0.45),
                xytext=(prev_x - 0.52, y + 0.45),
                arrowprops=dict(arrowstyle="->", lw=1.8, color="#ea580c"),
            )
            prev_x += 1.95

    ax.text(
        1.1,
        0.9,
        "Pointer chasing, per-node allocation, worse cache locality",
        fontsize=15,
        color="#7c2d12",
    )

    # Flat slots.
    start_x = 7.6
    y = 4.2
    labels = ["slot 0", "slot 1", "slot 2", "slot 3", "slot 4", "slot 5"]
    fills = ["#ccfbf1", "#99f6e4", "#5eead4", "#2dd4bf", "#14b8a6", "#0d9488"]
    for i, (label, fill) in enumerate(zip(labels, fills)):
        ax.add_patch(
            Rectangle(
                (start_x + i * 0.92, y),
                0.84,
                1.5,
                linewidth=1.5,
                edgecolor="#115e59",
                facecolor=fill,
            )
        )
        ax.text(
            start_x + i * 0.92 + 0.14,
            y + 0.62,
            label,
            fontsize=11,
            color="#082f49",
            rotation=90,
        )

    for i in range(5):
        ax.annotate(
            "",
            xy=(start_x + i * 0.92 + 1.0, y + 2.0),
            xytext=(start_x + i * 0.92 + 0.74, y + 2.0),
            arrowprops=dict(arrowstyle="->", lw=1.8, color="#0f766e"),
        )

    ax.text(
        7.6,
        6.2,
        "Contiguous slots + linear probing",
        fontsize=16,
        color="#0f766e",
        fontweight="bold",
    )
    ax.text(
        7.6,
        0.9,
        "Sequential access, fewer allocations, easy to benchmark and reason about",
        fontsize=15,
        color="#115e59",
    )

    fig.tight_layout()
    fig.savefig(ASSETS / "memory_layout.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def save_benchmark_summary() -> None:
    labels = ["Insert", "Find hit", "Find miss", "Erase", "Mixed"]
    perfmap = np.array([2.4, 1.18, 0.72, 7.7, 1.77])
    colors = ["#0f766e" if value >= 1 else "#b45309" for value in perfmap]

    fig, ax = plt.subplots(figsize=(13, 7.5))
    y = np.arange(len(labels))
    ax.barh(y, perfmap, color=colors, height=0.58)
    ax.axvline(1.0, color="#1e293b", linewidth=2, linestyle="--")

    ax.set_yticks(y, labels, fontsize=15)
    ax.set_xlim(0, 8.5)
    ax.set_xlabel("PerfMap speedup vs std::unordered_map", fontsize=15, color="#0f172a")
    ax.set_title(
        "Largest benchmark sizes from docs/RESULTS.md",
        fontsize=24,
        fontweight="bold",
        color="#0f172a",
        loc="left",
        pad=18,
    )
    ax.text(
        0.02,
        1.02,
        "Above 1.0 means PerfMap wins. Miss lookups remain the main trade-off.",
        transform=ax.transAxes,
        fontsize=13,
        color="#475569",
    )

    for i, value in enumerate(perfmap):
        label = f"{value:.2f}x"
        ax.text(
            value + 0.08,
            i,
            label,
            va="center",
            fontsize=14,
            color="#0f172a",
            fontweight="bold",
        )

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_visible(False)
    ax.spines["bottom"].set_color("#94a3b8")
    ax.tick_params(axis="x", labelsize=13, colors="#334155")
    ax.tick_params(axis="y", colors="#0f172a")
    ax.invert_yaxis()

    fig.tight_layout()
    fig.savefig(ASSETS / "benchmark_summary.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


def save_extension_tracks() -> None:
    fig, ax = plt.subplots(figsize=(14, 8))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 8)
    ax.axis("off")

    title_color = "#0f172a"
    ax.text(0.5, 7.2, "Three Strong Extension Tracks", fontsize=28, fontweight="bold", color=title_color)
    ax.text(0.5, 6.55, "Each one turns the same repo into a different career story.", fontsize=15, color="#475569")

    cards = [
        (0.7, "#eff6ff", "#1d4ed8", "AWS / DevOps", ["Perf regression CI", "EC2 or Graviton runs", "S3 result archive", "Dashboards + alerts"]),
        (4.8, "#ecfeff", "#0f766e", "Low-Level AI / ML", ["Feature-store hot path", "Embedding metadata cache", "Inference request dedup", "Latency-aware serving"]),
        (8.9, "#fefce8", "#a16207", "Performance C++", ["Robin Hood hashing", "SIMD probing", "SwissTable metadata", "Custom allocators"]),
    ]

    for x, fill, edge, heading, items in cards:
        ax.add_patch(
            FancyBboxPatch(
                (x, 1.2),
                3.8,
                4.7,
                boxstyle="round,pad=0.05,rounding_size=0.12",
                linewidth=2,
                edgecolor=edge,
                facecolor=fill,
            )
        )
        ax.text(x + 0.28, 5.3, heading, fontsize=19, fontweight="bold", color=edge)
        for idx, item in enumerate(items):
            ax.text(x + 0.28, 4.45 - idx * 0.8, f"• {item}", fontsize=14, color="#0f172a")

    fig.tight_layout()
    fig.savefig(ASSETS / "extension_tracks.png", dpi=220, bbox_inches="tight")
    plt.close(fig)


if __name__ == "__main__":
    save_memory_layout()
    save_benchmark_summary()
    save_extension_tracks()
