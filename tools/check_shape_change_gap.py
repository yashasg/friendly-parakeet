#!/usr/bin/env python3
"""Deterministic check: every shipped beatmap must satisfy the C++ loader's
min_shape_change_gap rule (issue #134).

Mirrors `validate_beat_map` Rule 6 in app/systems/beat_map_loader.cpp:
shape-bearing obstacles (shape_gate, split_path, combo_gate) that change
shape must be at least MIN_GAP beats apart.

Exits 0 on success, 1 on failure. Prints a per-difficulty summary.

Usage:
    python3 tools/check_shape_change_gap.py
    python3 tools/check_shape_change_gap.py content/beatmaps/1_stomper_beatmap.json
    python3 tools/check_shape_change_gap.py --min-gap 3
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO / "content" / "beatmaps"
SHAPE_KINDS = {"shape_gate", "split_path", "combo_gate"}
DEFAULT_MIN_GAP = 3


def violations(beats: list, min_gap: int) -> list[tuple[int, str, int, str, int]]:
    out: list[tuple[int, str, int, str, int]] = []
    prev_shape = None
    prev_beat = None
    for o in beats:
        if o.get("kind") not in SHAPE_KINDS or "shape" not in o:
            continue
        if prev_shape is not None and o["shape"] != prev_shape:
            gap = o["beat"] - prev_beat
            if gap < min_gap:
                out.append((prev_beat, prev_shape, o["beat"], o["shape"], gap))
        prev_shape = o["shape"]
        prev_beat = o["beat"]
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Validate min_shape_change_gap in beatmaps")
    ap.add_argument("files", nargs="*")
    ap.add_argument("--min-gap", type=int, default=DEFAULT_MIN_GAP)
    args = ap.parse_args()

    paths = [Path(f) for f in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"No beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    total = 0
    for path in paths:
        bm = json.loads(path.read_text())
        diffs = bm.get("difficulties", {})
        for diff, dd in diffs.items():
            v = violations(dd.get("beats", []), args.min_gap)
            status = "OK" if not v else f"FAIL ({len(v)} violations)"
            print(f"  {path.name:40s} [{diff:6s}]  {status}")
            for prev_b, prev_s, b, s, g in v[:3]:
                print(f"      beat {prev_b}({prev_s}) -> beat {b}({s})  gap={g} < {args.min_gap}",
                      file=sys.stderr)
            total += len(v)

    print()
    if total:
        print(f"FAIL: {total} min_shape_change_gap violation(s)", file=sys.stderr)
        return 1
    print(f"PASS: all beatmaps satisfy min_shape_change_gap >= {args.min_gap}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
