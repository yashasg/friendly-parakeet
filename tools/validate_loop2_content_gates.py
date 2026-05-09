#!/usr/bin/env python3
"""Validate Loop 2 hard content gates for current beatmap schema.

Default mode is advisory (exit 0 even with findings) so baseline failures can be
reported without breaking existing validation flows. Use --strict to fail.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO_ROOT / "content" / "beatmaps"

GAP_MONOTONY_CAP = {"medium": 0.40, "hard": 0.35}
SAME_SHAPE_RUN_CAP = {"medium": 3, "hard": 3}
GAP_ONE_MAX_RUN = {"easy": 0, "medium": 2, "hard": 3}
GAP_ONE_SHARE_CAP = {"medium": 0.20, "hard": 0.20}
MIN_IOI_MS = {"easy": 700.0, "medium": 380.0, "hard": 300.0}


def _ordered_valid_beats(beats: list[dict]) -> list[dict]:
    return sorted(
        [beat for beat in beats if isinstance(beat.get("beat"), int)],
        key=lambda beat: beat["beat"],
    )


def _longest_same_shape_run(ordered: list[dict]) -> int:
    longest = 0
    current = 0
    prev_shape = None
    for beat in ordered:
        shape = beat.get("shape")
        if shape == prev_shape:
            current += 1
        else:
            current = 1
            prev_shape = shape
        longest = max(longest, current)
    return longest


def _longest_gap_one_run(gaps: list[int]) -> int:
    longest = 0
    current = 0
    for gap in gaps:
        if gap == 1:
            current += 1
            longest = max(longest, current)
        else:
            current = 0
    return longest


def calculate_content_metrics(
    beats: list[dict], beat_times: list[float] | None = None, expected_count: int | None = None
) -> dict[str, float | int | bool | None]:
    ordered = _ordered_valid_beats(beats)
    beat_indices = [beat["beat"] for beat in ordered]
    gaps = [beat_indices[i] - beat_indices[i - 1] for i in range(1, len(beat_indices))]
    gap_counts = Counter(gaps)

    dominant_gap_share = (
        gap_counts.most_common(1)[0][1] / len(gaps)
        if gaps
        else 0.0
    )
    dominant_gap = gap_counts.most_common(1)[0][0] if gap_counts else None

    shape_counts = Counter(beat.get("shape") for beat in ordered if beat.get("kind") == "shape_gate")
    total_shape_gates = sum(shape_counts.values())

    min_ioi_ms: float | None = None
    ioi_index_error = False
    if beat_times and len(ordered) >= 2:
        for left, right in zip(beat_indices, beat_indices[1:]):
            if left < 0 or right < 0 or left >= len(beat_times) or right >= len(beat_times):
                ioi_index_error = True
                break
            dt_ms = (float(beat_times[right]) - float(beat_times[left])) * 1000.0
            min_ioi_ms = dt_ms if min_ioi_ms is None else min(min_ioi_ms, dt_ms)

    return {
        "total_obstacles": len(ordered),
        "valid_beat_count": len(ordered),
        "expected_count": expected_count,
        "count_matches": expected_count is None or expected_count == len(beats),
        "strictly_increasing": all(
            beat_indices[i] > beat_indices[i - 1] for i in range(1, len(beat_indices))
        ),
        "all_shape_gate": all(beat.get("kind") == "shape_gate" for beat in ordered),
        "lane_range_ok": all(beat.get("lane") in (0, 1, 2) for beat in ordered),
        "dominant_gap": dominant_gap,
        "dominant_gap_share": dominant_gap_share,
        "gap_one_share": (gap_counts.get(1, 0) / len(gaps)) if gaps else 0.0,
        "gap_one_run": _longest_gap_one_run(gaps),
        "longest_same_shape_run": _longest_same_shape_run(ordered) if ordered else 0,
        "triangle_share": (shape_counts.get("triangle", 0) / total_shape_gates) if total_shape_gates else 0.0,
        "circle_share": (shape_counts.get("circle", 0) / total_shape_gates) if total_shape_gates else 0.0,
        "min_ioi_ms": min_ioi_ms,
        "ioi_index_error": ioi_index_error,
    }


def evaluate_content_gates(metrics: dict[str, float | int | bool | None], difficulty: str) -> list[str]:
    findings: list[str] = []

    if not bool(metrics["count_matches"]):
        findings.append(
            f"count mismatch: count={metrics['expected_count']} len(beats)={metrics['total_obstacles']}"
        )
    if not bool(metrics["strictly_increasing"]):
        findings.append("beat indices are not strictly increasing")
    if not bool(metrics["lane_range_ok"]):
        findings.append("lane outside {0,1,2}")
    if not bool(metrics["all_shape_gate"]):
        findings.append("non-shape_gate obstacle present")

    monotony_cap = GAP_MONOTONY_CAP.get(difficulty)
    if monotony_cap is not None and float(metrics["dominant_gap_share"]) >= monotony_cap:
        findings.append(
            f"gap monotony: dominant gap={metrics['dominant_gap']} share={float(metrics['dominant_gap_share']):.1%} (must be < {monotony_cap:.0%})"
        )

    run_cap = SAME_SHAPE_RUN_CAP.get(difficulty)
    if run_cap is not None and int(metrics["longest_same_shape_run"]) > run_cap:
        findings.append(
            f"same-shape contiguous run {int(metrics['longest_same_shape_run'])} exceeds cap {run_cap}"
        )

    gap_one_run_cap = GAP_ONE_MAX_RUN.get(difficulty)
    if gap_one_run_cap is not None and int(metrics["gap_one_run"]) > gap_one_run_cap:
        findings.append(
            f"gap=1 burst run={int(metrics['gap_one_run'])} exceeds cap {gap_one_run_cap}"
        )

    gap_one_share_cap = GAP_ONE_SHARE_CAP.get(difficulty)
    if gap_one_share_cap is not None and float(metrics["gap_one_share"]) > gap_one_share_cap:
        findings.append(
            f"gap=1 share {float(metrics['gap_one_share']):.1%} exceeds cap {gap_one_share_cap:.0%}"
        )

    if difficulty == "hard":
        if float(metrics["triangle_share"]) < 0.25:
            findings.append(f"hard triangle share {float(metrics['triangle_share']):.1%} below floor 25%")
        if float(metrics["circle_share"]) > 0.40:
            findings.append(f"hard circle share {float(metrics['circle_share']):.1%} above ceiling 40%")

    min_ioi_ms = metrics.get("min_ioi_ms")
    if min_ioi_ms is not None:
        floor = MIN_IOI_MS.get(difficulty)
        if floor is not None and float(min_ioi_ms) < floor:
            findings.append(f"min IOI {float(min_ioi_ms):.1f}ms below floor {floor:.0f}ms")

    if bool(metrics.get("ioi_index_error")):
        findings.append("beat index out of range for beat_times array")

    return findings


def validate_beatmap(path: Path) -> list[tuple[str, dict[str, float | int | bool | None], list[str]]]:
    beatmap = json.loads(path.read_text())
    difficulties = beatmap.get("difficulties", {})
    beat_times = beatmap.get("beat_times", [])
    results: list[tuple[str, dict[str, float | int | bool | None], list[str]]] = []

    for difficulty in ("easy", "medium", "hard"):
        if difficulty not in difficulties:
            continue
        payload = difficulties[difficulty]
        beats = payload.get("beats", [])
        expected_count = payload.get("count")
        metrics = calculate_content_metrics(beats, beat_times=beat_times, expected_count=expected_count)
        findings = evaluate_content_gates(metrics, difficulty)
        results.append((difficulty, metrics, findings))

    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", help="Optional *_beatmap.json paths")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return non-zero exit code when gate findings are present.",
    )
    args = parser.parse_args(argv)

    paths = [Path(raw) for raw in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"ERROR: no beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    total_findings = 0
    print_mode = "STRICT" if args.strict else "REPORT"
    print(f"Loop 2 content-gate mode: {print_mode} (use --strict to gate).")

    for path in paths:
        print(f"\n== {path.name} ==")
        for difficulty, metrics, findings in validate_beatmap(path):
            status = "PASS" if not findings else f"FAIL ({len(findings)})"
            print(
                f"  [{difficulty:6s}] {status} "
                f"obs={metrics['total_obstacles']} "
                f"dom_gap={float(metrics['dominant_gap_share']):.1%} "
                f"gap1={float(metrics['gap_one_share']):.1%} "
                f"gap1_run={int(metrics['gap_one_run'])} "
                f"shape_run={int(metrics['longest_same_shape_run'])} "
                f"tri={float(metrics['triangle_share']):.1%} "
                f"cir={float(metrics['circle_share']):.1%}"
            )
            for finding in findings:
                print(f"      - {finding}")
            total_findings += len(findings)

    if total_findings == 0:
        print("\nPASS: all checked beatmaps satisfy Loop 2 hard gates.")
        return 0

    if args.strict:
        print(f"\nFAIL: {total_findings} hard-gate finding(s) under --strict.", file=sys.stderr)
        return 1

    print(
        f"\nWARN: {total_findings} hard-gate finding(s) (report-only mode, exit 0).",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
