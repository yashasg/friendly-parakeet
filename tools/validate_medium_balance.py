#!/usr/bin/env python3
"""Validate medium difficulty shape/lane distribution for issue #142.

Medium shape-gate targets:
  Circle:   10-20%
  Square:   45-60%
  Triangle: 25-45%

Shape gates map directly to lanes, so lane distribution uses the same ranges.
"""

import json
import sys
from collections import Counter
from pathlib import Path


TARGET_RANGES = {
    "circle": (0.10, 0.20),
    "square": (0.45, 0.60),
    "triangle": (0.25, 0.45),
}

LANE_NAMES = {
    0: "circle",
    1: "square",
    2: "triangle",
}


def validate_beatmap(beatmap_path: Path) -> list[str]:
    with open(beatmap_path) as f:
        beatmap = json.load(f)

    beats = beatmap.get("difficulties", {}).get("medium", {}).get("beats", [])
    shapes = Counter()
    lanes = Counter()
    total_shape_gates = 0

    for beat in beats:
        if beat.get("kind") != "shape_gate":
            continue
        total_shape_gates += 1
        shapes[beat.get("shape", "")] += 1
        lanes[beat.get("lane", -1)] += 1

    if total_shape_gates == 0:
        return [f"{beatmap_path.name} medium: no shape_gate obstacles"]

    issues: list[str] = []
    title = beatmap.get("title", beatmap_path.stem)

    for shape, (min_pct, max_pct) in TARGET_RANGES.items():
        pct = shapes.get(shape, 0) / total_shape_gates
        if not (min_pct <= pct <= max_pct):
            issues.append(
                f"{title} medium: {shape} at {pct:.1%} "
                f"(target {min_pct:.0%}-{max_pct:.0%})"
            )

    for lane, shape in LANE_NAMES.items():
        min_pct, max_pct = TARGET_RANGES[shape]
        pct = lanes.get(lane, 0) / total_shape_gates
        if not (min_pct <= pct <= max_pct):
            issues.append(
                f"{title} medium: lane {lane} ({shape}) at {pct:.1%} "
                f"(target {min_pct:.0%}-{max_pct:.0%})"
            )

    return issues


def main() -> int:
    beatmap_dir = Path(__file__).parent.parent / "content" / "beatmaps"
    beatmaps = sorted(beatmap_dir.glob("*_beatmap.json"))
    if not beatmaps:
        print(f"ERROR: no beatmaps found in {beatmap_dir}", file=sys.stderr)
        return 1

    all_issues: list[str] = []
    for beatmap_path in beatmaps:
        all_issues.extend(validate_beatmap(beatmap_path))

    if all_issues:
        print("MEDIUM DISTRIBUTION VALIDATION FAILURES:", file=sys.stderr)
        for issue in all_issues:
            print(f"  x {issue}", file=sys.stderr)
        return 1

    print("PASS: all shipped medium beatmaps satisfy shape/lane balance (#142).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
