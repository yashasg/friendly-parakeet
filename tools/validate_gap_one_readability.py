#!/usr/bin/env python3
"""Validate gap=1 readability rules for issue #141.

Policy:
  EASY   - gap=1 is never allowed.
  MEDIUM - gap=1 is allowed only after 30% authored beat progress, only
           between identical shape gates, and never in back-to-back runs.
  HARD   - gap=1 is allowed after beat 10 under the same readability guard,
           with at most two consecutive one-beat gaps.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO / "content" / "beatmaps"

GAP_ONE_MEDIUM_START_PROGRESS = 0.30
GAP_ONE_HARD_MIN_BEAT = 11
GAP_ONE_MAX_RUN = {"medium": 1, "hard": 2}
def is_readable_family(left: dict, right: dict) -> bool:
    return (
        left.get("kind") == "shape_gate"
        and right.get("kind") == "shape_gate"
        and left.get("shape") == right.get("shape")
        and left.get("lane") == right.get("lane")
    )



def validate_gap_one(beats: list[dict], difficulty: str) -> list[tuple[int, str]]:
    violations: list[tuple[int, str]] = []
    if len(beats) <= 1:
        return violations

    last_authored_beat = max(1, max(beat["beat"] for beat in beats))
    gap_one_run = 0

    for index in range(1, len(beats)):
        left = beats[index - 1]
        right = beats[index]
        gap = right["beat"] - left["beat"]

        if gap != 1:
            gap_one_run = 0
            continue

        if difficulty == "easy":
            violations.append((left["beat"], "gap=1 is not allowed on easy"))
            gap_one_run += 1
            continue

        if difficulty == "medium":
            progress = left["beat"] / last_authored_beat
            if progress <= GAP_ONE_MEDIUM_START_PROGRESS:
                violations.append(
                    (left["beat"], f"gap=1 before medium teaching threshold ({progress:.2f})")
                )
            if gap_one_run >= GAP_ONE_MAX_RUN["medium"]:
                violations.append((left["beat"], "gap=1 run exceeds medium limit 1"))
        elif difficulty == "hard":
            if left["beat"] < GAP_ONE_HARD_MIN_BEAT:
                violations.append((left["beat"], "gap=1 appears before hard intro guard"))
            if gap_one_run >= GAP_ONE_MAX_RUN["hard"]:
                violations.append((left["beat"], "gap=1 run exceeds hard limit 2"))
        else:
            violations.append((left["beat"], f"unknown difficulty '{difficulty}'"))

        if not is_readable_family(left, right):
            violations.append((left["beat"], "gap=1 pair is not identical shape_gate family"))

        gap_one_run += 1

    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate gap=1 readability rules")
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()

    paths = [Path(path) for path in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"ERROR: no beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    total_violations = 0
    for path in paths:
        beatmap = json.loads(path.read_text())
        difficulties = beatmap.get("difficulties", {})

        for difficulty in ("easy", "medium", "hard"):
            if difficulty not in difficulties:
                continue

            beats = difficulties[difficulty].get("beats", [])
            violations = validate_gap_one(beats, difficulty)
            status = "OK" if not violations else f"FAIL ({len(violations)} violations)"
            print(f"  {path.name:40s} [{difficulty:6s}]  {status}")

            for beat_index, reason in violations[:5]:
                print(f"      beat {beat_index}: {reason}", file=sys.stderr)
            total_violations += len(violations)

    if total_violations:
        print(f"FAIL: {total_violations} gap=1 readability violation(s)", file=sys.stderr)
        return 1

    print("PASS: all beatmaps satisfy gap=1 readability rules.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
