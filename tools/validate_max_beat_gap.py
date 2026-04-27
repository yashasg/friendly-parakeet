#!/usr/bin/env python3
"""Validate maximum beat gap per difficulty level.

Issue #138: Shipped songs contain 56-64 beat silent gaps.

This validator checks the longest mid-song gap between authored obstacles,
measured as consecutive beat indices with no obstacle. Leading intro rest and
trailing outro rest are not counted as failures.

Per-difficulty limits:
  EASY:   max gap <= 40 beats
  MEDIUM: max gap <= 32 beats
  HARD:   max gap <= 30 beats

A "gap" is the number of consecutive beats with no obstacles.

Exit codes:
  0 - all beatmaps pass
  1 - one or more violations found

Usage:
    python3 tools/validate_max_beat_gap.py
    python3 tools/validate_max_beat_gap.py content/beatmaps/1_stomper_beatmap.json
"""

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO / "content" / "beatmaps"

# Maximum silent gap (in beats) per difficulty.
# These thresholds allow intentional breathing room (section transitions) while
# catching excessive gaps that indicate generation issues.
#
# Issue #138: "shipped songs contain 56-64 beat silent gaps" - we flag issues
# at these high levels to catch regressions, but allow somewhat higher thresholds
# than absolute minimum to account for song structure (intros, outros, transitions).
#
# Guidelines per BPM:
#   At 120 BPM: 32 beats = 16sec, 40 beats = 20sec
#   At 160 BPM: 32 beats = 12sec, 40 beats = 15sec
#
# This is more about catching *unintentional* gaps from generation bugs rather
# than strictly limiting longest comfortable silence.
MAX_GAP = {
    "easy":   40,    # allow breathing room for learning
    "medium": 32,    # slightly less for ramp difficulty
    "hard":   30,    # slightly more density expected
}


def find_max_gap(beats_in_beatmap: set[int]) -> int:
    """Return the longest mid-song run of consecutive beats with no obstacle."""
    if len(beats_in_beatmap) <= 1:
        return 0

    sorted_beats = sorted(beats_in_beatmap)
    max_gap = 0
    for prev_beat, next_beat in zip(sorted_beats, sorted_beats[1:]):
        max_gap = max(max_gap, next_beat - prev_beat - 1)
    return max_gap


def main() -> int:
    import argparse
    ap = argparse.ArgumentParser(description="Validate max beat gap per difficulty")
    ap.add_argument("files", nargs="*")
    ap.add_argument("--max-gap-easy", type=int, default=MAX_GAP["easy"])
    ap.add_argument("--max-gap-medium", type=int, default=MAX_GAP["medium"])
    ap.add_argument("--max-gap-hard", type=int, default=MAX_GAP["hard"])
    args = ap.parse_args()

    max_gaps = {
        "easy": args.max_gap_easy,
        "medium": args.max_gap_medium,
        "hard": args.max_gap_hard,
    }

    paths = [Path(f) for f in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"ERROR: no beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    all_violations: list[str] = []

    for path in paths:
        with open(path) as f:
            beatmap = json.load(f)

        name = path.stem.replace("_beatmap", "")

        for difficulty in ["easy", "medium", "hard"]:
            if difficulty not in beatmap.get("difficulties", {}):
                continue

            diff_data = beatmap["difficulties"][difficulty]
            beats_in_beatmap = set(b["beat"] for b in diff_data.get("beats", []))

            if not beats_in_beatmap:
                continue
            max_gap = find_max_gap(beats_in_beatmap)

            max_allowed = max_gaps[difficulty]
            if max_gap > max_allowed:
                all_violations.append(
                    f"{name} [{difficulty}]: max gap {max_gap} beats "
                    f"exceeds limit {max_allowed} beats"
                )

    if all_violations:
        print("MAX BEAT GAP VIOLATIONS (#138):", file=sys.stderr)
        for v in all_violations:
            print(f"  x {v}", file=sys.stderr)
        return 1

    print(f"OK: max beat gap checks passed for {len(paths)} beatmap(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
