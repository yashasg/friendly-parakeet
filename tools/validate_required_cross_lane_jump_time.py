#!/usr/bin/env python3
"""Validate time-authored required gates for reachable 0<->2 lane jumps.

Issue #819: a player cannot reliably clear adjacent required shape gates that
also require a full left/right lane swap when they are authored too close
together in real time. Non-blocking onset_marker rows preserve audio metadata
and are ignored by this required-action check.

Exit codes:
  0 - all beatmaps pass
  1 - one or more violations found

Usage:
    python3 tools/validate_required_cross_lane_jump_time.py
    python3 tools/validate_required_cross_lane_jump_time.py content/beatmaps/2_drama_beatmap.json
    python3 tools/validate_required_cross_lane_jump_time.py --min-sec 1.2
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO_ROOT / "content" / "beatmaps"

REQUIRED_KIND = "shape_gate"
FULL_CROSS_LANE_JUMP = {0, 2}
DEFAULT_MIN_CROSS_LANE_JUMP_SEC = 1.2


def _numeric_time(row: dict) -> float | None:
    value = row.get("time_sec")
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return float(value)
    return None


def _required_timed_shape_gates(beats: list[object]) -> list[dict]:
    rows: list[dict] = []
    for row in beats:
        if not isinstance(row, dict):
            continue
        if row.get("kind") != REQUIRED_KIND:
            continue
        if _numeric_time(row) is None:
            continue
        rows.append(row)
    rows.sort(key=lambda row: _numeric_time(row) or 0.0)
    return rows


def find_short_cross_lane_shape_jumps(
    beats: list[object],
    min_sec: float = DEFAULT_MIN_CROSS_LANE_JUMP_SEC,
) -> list[tuple[dict, dict, float]]:
    """Return adjacent required shape gates with unreachable 0<->2 swaps."""
    violations: list[tuple[dict, dict, float]] = []
    rows = _required_timed_shape_gates(beats)
    for previous, current in zip(rows, rows[1:]):
        previous_lane = previous.get("lane")
        current_lane = current.get("lane")
        if {previous_lane, current_lane} != FULL_CROSS_LANE_JUMP:
            continue
        if previous.get("shape") == current.get("shape"):
            continue
        dt = (_numeric_time(current) or 0.0) - (_numeric_time(previous) or 0.0)
        if 0.0 < dt and dt < min_sec - 1e-9:
            violations.append((previous, current, dt))
    return violations


def validate_beatmap(path: Path, min_sec: float = DEFAULT_MIN_CROSS_LANE_JUMP_SEC) -> list[str]:
    beatmap = json.loads(path.read_text())
    name = path.stem.replace("_beatmap", "")
    findings: list[str] = []
    for difficulty, payload in beatmap.get("difficulties", {}).items():
        beats = payload.get("beats", []) or []
        for previous, current, dt in find_short_cross_lane_shape_jumps(beats, min_sec):
            findings.append(
                f"{name} [{difficulty}]: beat {previous.get('beat')} "
                f"(lane={previous.get('lane')}, shape={previous.get('shape')}, "
                f"t={_numeric_time(previous):.3f}s) -> beat {current.get('beat')} "
                f"(lane={current.get('lane')}, shape={current.get('shape')}, "
                f"t={_numeric_time(current):.3f}s) is {dt:.3f}s < {min_sec:.3f}s (#819)"
            )
    return findings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", help="Optional *_beatmap.json paths")
    parser.add_argument("--min-sec", type=float, default=DEFAULT_MIN_CROSS_LANE_JUMP_SEC)
    args = parser.parse_args(argv)

    paths = [Path(raw) for raw in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"ERROR: no beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    all_findings: list[str] = []
    for path in paths:
        all_findings.extend(validate_beatmap(path, args.min_sec))

    if all_findings:
        print("REQUIRED CROSS-LANE JUMP VIOLATIONS (#819):", file=sys.stderr)
        for finding in all_findings:
            print(f"  x {finding}", file=sys.stderr)
        return 1

    print(f"OK: no required 0<->2 lane shape jumps under {args.min_sec:.3f}s in {len(paths)} beatmap(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
