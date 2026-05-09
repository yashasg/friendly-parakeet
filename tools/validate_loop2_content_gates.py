#!/usr/bin/env python3
"""Validate Loop 2 content-gate metrics for current beatmap schema.

This validator is advisory by default to avoid destabilizing baseline validation.
Use --strict to turn findings into a failing exit code.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO_ROOT / "content" / "beatmaps"

GATE_RULES = {
    "easy": {
        "min_distinct_shapes": 3,
        "max_dominant_shape_pct": 65.0,
        "max_gap_dominance_pct": 40.0,
    },
    "medium": {
        "min_distinct_shapes": 3,
        "max_dominant_shape_pct": 65.0,
        "max_gap_dominance_pct": 40.0,
    },
    "hard": {
        "min_distinct_shapes": 3,
        "max_dominant_shape_pct": 70.0,
        "max_gap_dominance_pct": 40.0,
    },
}


def _is_gap_one_readable_pair(left: dict, right: dict) -> bool:
    return (
        left.get("kind") == "shape_gate"
        and right.get("kind") == "shape_gate"
        and left.get("shape") == right.get("shape")
        and left.get("lane") == right.get("lane")
    )


def calculate_content_metrics(beats: list[dict]) -> dict[str, float | int]:
    ordered = sorted(
        [beat for beat in beats if isinstance(beat.get("beat"), int)],
        key=lambda beat: beat["beat"],
    )

    shape_gates = [beat for beat in ordered if beat.get("kind") == "shape_gate"]
    shape_counts = Counter(
        beat.get("shape", "")
        for beat in shape_gates
        if isinstance(beat.get("shape"), str) and beat.get("shape")
    )

    dominant_shape_pct = (
        100.0 * max(shape_counts.values()) / len(shape_gates)
        if shape_gates and shape_counts
        else 0.0
    )

    gap_values = [
        ordered[index]["beat"] - ordered[index - 1]["beat"]
        for index in range(1, len(ordered))
    ]
    gap_counts = Counter(gap_values)
    gap_dominance_pct = (
        100.0 * gap_counts.most_common(1)[0][1] / len(gap_values)
        if gap_values
        else 0.0
    )

    longest_same_shape_run = 0
    run_steps_ge_3 = 0
    current_shape: str | None = None
    current_len = 0
    for beat in ordered:
        if beat.get("kind") != "shape_gate":
            current_shape = None
            current_len = 0
            continue
        shape = beat.get("shape")
        if shape == current_shape:
            current_len += 1
        else:
            current_shape = shape if isinstance(shape, str) else None
            current_len = 1
        if current_len >= 3:
            run_steps_ge_3 += 1
        longest_same_shape_run = max(longest_same_shape_run, current_len)

    gap_one_pairs = 0
    unreadable_gap_one_pairs = 0
    for index in range(1, len(ordered)):
        left = ordered[index - 1]
        right = ordered[index]
        if right["beat"] - left["beat"] != 1:
            continue
        gap_one_pairs += 1
        if not _is_gap_one_readable_pair(left, right):
            unreadable_gap_one_pairs += 1

    return {
        "total_obstacles": len(ordered),
        "shape_gate_count": len(shape_gates),
        "distinct_shapes": len(shape_counts),
        "dominant_shape_pct": round(dominant_shape_pct, 3),
        "max_gap_dominance_pct": round(gap_dominance_pct, 3),
        "longest_same_shape_run": longest_same_shape_run,
        "run_steps_ge_3": run_steps_ge_3,
        "gap_one_pairs": gap_one_pairs,
        "unreadable_gap_one_pairs": unreadable_gap_one_pairs,
    }


def evaluate_content_gates(metrics: dict[str, float | int], difficulty: str) -> list[str]:
    rules = GATE_RULES.get(difficulty, {})
    findings: list[str] = []

    shape_gate_count = int(metrics["shape_gate_count"])
    if shape_gate_count <= 0:
        findings.append("shape_gate_count=0")
        return findings

    distinct_shapes = int(metrics["distinct_shapes"])
    min_shapes = int(rules.get("min_distinct_shapes", 0))
    if distinct_shapes < min_shapes:
        findings.append(f"distinct_shapes={distinct_shapes} < {min_shapes}")

    dominant_shape_pct = float(metrics["dominant_shape_pct"])
    max_dominant = float(rules.get("max_dominant_shape_pct", 100.0))
    if dominant_shape_pct > max_dominant:
        findings.append(
            f"dominant_shape_pct={dominant_shape_pct:.1f}% > {max_dominant:.1f}%"
        )

    gap_dominance_pct = float(metrics["max_gap_dominance_pct"])
    max_gap_dominance = float(rules.get("max_gap_dominance_pct", 100.0))
    if gap_dominance_pct > max_gap_dominance:
        findings.append(
            f"max_gap_dominance_pct={gap_dominance_pct:.1f}% > {max_gap_dominance:.1f}%"
        )

    unreadable_gap_one_pairs = int(metrics["unreadable_gap_one_pairs"])
    if unreadable_gap_one_pairs > 0:
        findings.append(f"unreadable_gap_one_pairs={unreadable_gap_one_pairs}")

    return findings


def validate_beatmap(path: Path) -> list[tuple[str, dict[str, float | int], list[str]]]:
    beatmap = json.loads(path.read_text())
    difficulties = beatmap.get("difficulties", {})
    results: list[tuple[str, dict[str, float | int], list[str]]] = []

    for difficulty in ("easy", "medium", "hard"):
        if difficulty not in difficulties:
            continue
        beats = difficulties[difficulty].get("beats", [])
        metrics = calculate_content_metrics(beats)
        findings = evaluate_content_gates(metrics, difficulty)
        results.append((difficulty, metrics, findings))

    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", help="Optional *_beatmap.json paths")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return non-zero exit code when content-gate findings are present.",
    )
    args = parser.parse_args(argv)

    paths = [Path(raw) for raw in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"ERROR: no beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    total_findings = 0
    for path in paths:
        for difficulty, metrics, findings in validate_beatmap(path):
            status = "OK" if not findings else f"FINDINGS ({len(findings)})"
            print(
                f"{path.name:40s} [{difficulty:6s}] {status} "
                f"shapes={metrics['distinct_shapes']} "
                f"dominant={metrics['dominant_shape_pct']:.1f}% "
                f"gap_dom={metrics['max_gap_dominance_pct']:.1f}% "
                f"gap1_unreadable={metrics['unreadable_gap_one_pairs']}"
            )
            total_findings += len(findings)
            for finding in findings:
                print(f"  - {path.name} [{difficulty}] {finding}", file=sys.stderr)

    if total_findings == 0:
        print("PASS: Loop 2 content-gate metrics satisfy configured thresholds.")
        return 0

    if args.strict:
        print(
            f"FAIL: {total_findings} Loop 2 content-gate finding(s) under --strict.",
            file=sys.stderr,
        )
        return 1

    print(
        f"WARN: {total_findings} Loop 2 content-gate finding(s) (advisory mode; use --strict to fail).",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
