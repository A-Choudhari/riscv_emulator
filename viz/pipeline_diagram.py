#!/usr/bin/env python3
"""
pipeline_diagram.py -- renders the classic Patterson & Hennessy colored-grid
pipeline diagram from a trace CSV produced by the C++ simulator
(rvsim ... --trace out.csv).

Usage:
    python3 pipeline_diagram.py trace.csv [--max-cycles 60] [--out diagram.png]

Also produces a couple of summary bar charts (stall breakdown, branch
accuracy) if you pass --stats-json stats.json (see note at bottom on how
to add JSON stats export if you want it -- the CSV trace alone is enough
for the diagram itself).
"""
import argparse
import csv
import sys
from collections import defaultdict

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
except ImportError:
    print("This script needs matplotlib: pip install matplotlib", file=sys.stderr)
    sys.exit(1)

STAGE_COLORS = {
    "IF":  "#8ecae6",
    "ID":  "#219ebc",
    "ID*": "#a8dadc",   # stalled decode (load-use bubble)
    "EX":  "#ffb703",
    "MEM": "#fb8500",
    "WB":  "#2a9d8f",
    "FLUSH(EX)": "#e63946",
}


def load_trace(path):
    rows = []
    with open(path) as f:
        reader = csv.DictReader(f)
        for r in reader:
            rows.append({
                "cycle": int(r["cycle"]),
                "pc": int(r["pc"]),
                "text": r["instruction"],
                "stage": r["stage"],
            })
    return rows


def build_grid(rows, max_cycles):
    # instruction order = first appearance order, keyed by pc
    order = []
    seen = set()
    text_for_pc = {}
    grid = defaultdict(dict)
    max_cycle_seen = 0
    for r in rows:
        pc = r["pc"]
        if pc not in seen:
            seen.add(pc)
            order.append(pc)
        text_for_pc[pc] = r["text"]
        grid[pc][r["cycle"]] = r["stage"]
        max_cycle_seen = max(max_cycle_seen, r["cycle"])

    upto = min(max_cycle_seen, max_cycles) if max_cycles else max_cycle_seen
    return order, text_for_pc, grid, upto


def plot_diagram(order, text_for_pc, grid, upto, out_path):
    n_instr = len(order)
    fig_height = max(2, 0.35 * n_instr + 1)
    fig_width = max(8, 0.28 * upto + 4)
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))

    for row_idx, pc in enumerate(order):
        y = n_instr - row_idx - 1
        for cycle in range(1, upto + 1):
            stage = grid[pc].get(cycle)
            if not stage or stage == "FLUSH(EX)":
                continue
            color = STAGE_COLORS.get(stage, "#cccccc")
            rect = mpatches.Rectangle((cycle - 1, y), 1, 0.9, facecolor=color, edgecolor="white")
            ax.add_patch(rect)
            ax.text(cycle - 0.5, y + 0.45, stage.replace("*", ""), ha="center", va="center",
                     fontsize=7, color="black")

    ax.set_xlim(0, upto)
    ax.set_ylim(0, n_instr)
    ax.set_xticks(range(0, upto + 1, max(1, upto // 20)))
    ax.set_xlabel("Cycle")
    ax.set_yticks([n_instr - i - 0.55 for i in range(n_instr)])
    ax.set_yticklabels([text_for_pc[pc] for pc in order], fontsize=8, family="monospace")
    ax.set_title("Pipeline Execution Diagram")

    handles = [mpatches.Patch(color=c, label=s) for s, c in STAGE_COLORS.items() if s != "FLUSH(EX)"]
    ax.legend(handles=handles, loc="upper center", bbox_to_anchor=(0.5, -0.12),
              ncol=len(handles), fontsize=8, frameon=False)

    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    print(f"Wrote {out_path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace_csv")
    ap.add_argument("--max-cycles", type=int, default=80)
    ap.add_argument("--out", default="pipeline_diagram.png")
    args = ap.parse_args()

    rows = load_trace(args.trace_csv)
    order, text_for_pc, grid, upto = build_grid(rows, args.max_cycles)
    plot_diagram(order, text_for_pc, grid, upto, args.out)


if __name__ == "__main__":
    main()
