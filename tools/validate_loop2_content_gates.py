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
SHAPE_CLUSTER_GAP = 3
DENSE_CLUSTER_WARN_SIZE = {"medium": 3, "hard": 3}
# Issue #532 — strict ceiling on dense same-shape clusters.  The previous
# revision silently demoted ``SAME_SHAPE_RUN_CAP`` to advisory whenever an
# oversized cluster was present (the symptom turned the gate off).  The
# replacement is a hard cap on cluster *size*: medium ≤ 4, hard ≤ 5.  See
# decisions.md for rationale.
MAX_SHAPE_CLUSTER_SIZE = {"medium": 4, "hard": 5}
GAP_ONE_MAX_RUN = {"easy": 0, "medium": 2, "hard": 3}
GAP_ONE_SHARE_CAP = {"medium": 0.20, "hard": 0.20}
# Keep strict min-IOI floors aligned with the active onset-only selector in
# level_designer.py.  Higher legacy readability floors are enforced only where
# the generator can satisfy them without inserting non-onset filler.
MIN_IOI_MS = {"easy": 500.0, "medium": 350.0, "hard": 280.0}
SHAPE_OBSTACLE_KINDS = {"shape_gate", "split_path"}
SUPPORTED_NON_BLOCKING_KINDS = {"onset_marker"}
# Issue #420 — broad-layer/lane reachability floor at medium/hard.
# Each shipped beatmap×difficulty must include at least this share of
# circle (lane-0 / harmonic-mapped) obstacles so the left shape lane stays
# reachable design space.
CIRCLE_LANE0_SHARE_FLOOR = {"medium": 0.10, "hard": 0.10}


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


def _shape_gate_clusters(ordered: list[dict], min_gap: int = SHAPE_CLUSTER_GAP) -> list[list[dict]]:
    clusters: list[list[dict]] = []
    current: list[dict] = []
    prev_beat: int | None = None
    for beat in ordered:
        if beat.get("kind") not in SHAPE_OBSTACLE_KINDS:
            continue
        beat_idx = beat.get("beat")
        if not isinstance(beat_idx, int):
            continue
        if prev_beat is None or beat_idx - prev_beat >= min_gap:
            if current:
                clusters.append(current)
            current = [beat]
        else:
            current.append(beat)
        prev_beat = beat_idx
    if current:
        clusters.append(current)
    return clusters


def _longest_same_shape_cluster_run(clusters: list[list[dict]]) -> int:
    longest = 0
    current = 0
    prev_shape = None
    for cluster in clusters:
        shape = cluster[0].get("shape")
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


CROSS_LAYER_PRESERVATION_WINDOW_MS = 50.0


def _all_onset_timed(beats: list[dict]) -> bool:
    saw_dict = False
    for beat in beats:
        if not isinstance(beat, dict):
            continue
        saw_dict = True
        if beat.get("timing_source") != "onset":
            return False
    return saw_dict


def calculate_content_metrics(
    beats: list[dict], beat_times: list[float] | None = None, expected_count: int | None = None
) -> dict[str, float | int | bool | None]:
    raw_beat_rows = len(beats)
    ordered = _ordered_valid_beats(beats)
    shape_clusters = _shape_gate_clusters(ordered)
    shape_obstacles = [beat for beat in ordered if beat.get("kind") in SHAPE_OBSTACLE_KINDS]
    cluster_sizes = [len(cluster) for cluster in shape_clusters]
    beat_indices = [beat["beat"] for beat in ordered]
    gaps = [beat_indices[i] - beat_indices[i - 1] for i in range(1, len(beat_indices))]
    gap_counts = Counter(gaps)

    dominant_gap_share = (
        gap_counts.most_common(1)[0][1] / len(gaps)
        if gaps
        else 0.0
    )
    dominant_gap = gap_counts.most_common(1)[0][0] if gap_counts else None

    shape_counts = Counter(beat.get("shape") for beat in shape_obstacles)
    total_shape_gates = sum(shape_counts.values())
    lane_counts = Counter(beat.get("lane") for beat in shape_obstacles)

    onset_timed = _all_onset_timed(ordered)

    # Issue #443 — onset-aware min IOI:
    #   * Under onset-only timing, drive IOI from authored time_sec/onset_time_sec
    #     so cross-layer onsets (different broad onset_class) within the
    #     50 ms preservation window can be exempted from the floor.
    #   * Under beat-grid timing, retain the legacy beat_times[] lookup.
    min_ioi_ms: float | None = None
    ioi_index_error = False
    cross_layer_preserved_pairs = 0
    if onset_timed and len(ordered) >= 2:
        prev = ordered[0]
        for current in ordered[1:]:
            prev_t = prev.get("time_sec")
            cur_t = current.get("time_sec")
            if not isinstance(prev_t, (int, float)) or not isinstance(cur_t, (int, float)):
                prev = current
                continue
            dt_ms = (float(cur_t) - float(prev_t)) * 1000.0
            cross_layer = (
                prev.get("onset_class") != current.get("onset_class")
                and prev.get("onset_class") is not None
                and current.get("onset_class") is not None
            )
            if cross_layer and 0.0 <= dt_ms < CROSS_LAYER_PRESERVATION_WINDOW_MS:
                cross_layer_preserved_pairs += 1
                prev = current
                continue
            if dt_ms >= 0.0:
                min_ioi_ms = dt_ms if min_ioi_ms is None else min(min_ioi_ms, dt_ms)
            prev = current
    elif beat_times and len(ordered) >= 2:
        for left, right in zip(beat_indices, beat_indices[1:]):
            if left < 0 or right < 0 or left >= len(beat_times) or right >= len(beat_times):
                ioi_index_error = True
                break
            dt_ms = (float(beat_times[right]) - float(beat_times[left])) * 1000.0
            min_ioi_ms = dt_ms if min_ioi_ms is None else min(min_ioi_ms, dt_ms)

    return {
        "total_obstacles": len(ordered),
        "valid_beat_count": len(ordered),
        "raw_beat_rows": raw_beat_rows,
        "expected_count": expected_count,
        "count_matches": expected_count is None or expected_count == len(ordered),
        "strictly_increasing": all(
            beat_indices[i] > beat_indices[i - 1] for i in range(1, len(beat_indices))
        ),
        "all_shape_gate": all(
            beat.get("kind") in SHAPE_OBSTACLE_KINDS | SUPPORTED_NON_BLOCKING_KINDS
            for beat in ordered
        ),
        "lane_range_ok": all(beat.get("lane") in (0, 1, 2) for beat in ordered),
        "dominant_gap": dominant_gap,
        "dominant_gap_share": dominant_gap_share,
        "gap_one_share": (gap_counts.get(1, 0) / len(gaps)) if gaps else 0.0,
        "gap_one_run": _longest_gap_one_run(gaps),
        "longest_same_shape_run": _longest_same_shape_run(shape_obstacles) if shape_obstacles else 0,
        "longest_same_shape_cluster_run": _longest_same_shape_cluster_run(shape_clusters) if shape_clusters else 0,
        "shape_cluster_count": len(shape_clusters),
        "max_shape_cluster_size": max(cluster_sizes) if cluster_sizes else 0,
        "shape_clusters_over_warn": 0,
        "triangle_share": (shape_counts.get("triangle", 0) / total_shape_gates) if total_shape_gates else 0.0,
        "circle_share": (shape_counts.get("circle", 0) / total_shape_gates) if total_shape_gates else 0.0,
        "lane0_share": (lane_counts.get(0, 0) / total_shape_gates) if total_shape_gates else 0.0,
        "lane2_share": (lane_counts.get(2, 0) / total_shape_gates) if total_shape_gates else 0.0,
        "min_ioi_ms": min_ioi_ms,
        "ioi_index_error": ioi_index_error,
        "onset_timed": onset_timed,
        "cross_layer_preserved_pairs": cross_layer_preserved_pairs,
    }


def evaluate_content_gates(metrics: dict[str, float | int | bool | None], difficulty: str) -> list[str]:
    findings: list[str] = []
    onset_timed = bool(metrics.get("onset_timed"))

    if not bool(metrics["count_matches"]):
        expected_count = metrics.get("expected_count")
        valid_beat_count = int(metrics.get("valid_beat_count", metrics.get("total_obstacles", 0)))
        raw_beat_rows = int(metrics.get("raw_beat_rows", valid_beat_count))
        findings.append(
            f"count mismatch: count={expected_count} valid_beats={valid_beat_count} raw_rows={raw_beat_rows}"
        )
    if not bool(metrics["strictly_increasing"]):
        findings.append("beat indices are not strictly increasing")
    if not bool(metrics["lane_range_ok"]):
        findings.append("lane outside {0,1,2}")
    if not bool(metrics["all_shape_gate"]):
        findings.append("unsupported obstacle kind present")

    # Issue #443 — beat-ordinal gap monotony / gap=1 share / gap=1 run gates
    # were designed for beat-grid timing where `beat[i+1]-beat[i]` reflects
    # musical-beat distance. Under onset-only timing (PR #427) `beat` is a
    # sequential ordinal across selected onsets, so gap==1 is the natural
    # shape and these gates emit structural false positives. Demote them to
    # advisories in onset-only mode; time-IOI is the strict gate via min_ioi.
    if not onset_timed:
        monotony_cap = GAP_MONOTONY_CAP.get(difficulty)
        if monotony_cap is not None and float(metrics["dominant_gap_share"]) >= monotony_cap:
            findings.append(
                f"gap monotony: dominant gap={metrics['dominant_gap']} share={float(metrics['dominant_gap_share']):.1%} (must be < {monotony_cap:.0%})"
            )

    run_cap = SAME_SHAPE_RUN_CAP.get(difficulty)
    cluster_run = int(metrics.get("longest_same_shape_cluster_run", metrics.get("longest_same_shape_run", 0)))
    protected_onset_pairs = onset_timed and int(metrics.get("cross_layer_preserved_pairs", 0) or 0) > 0
    # Issue #532 — the previous ``not oversized_cluster_present`` escape
    # was self-disabling: any oversized cluster turned the cluster-chain
    # run cap off, even though the cluster *was* the violation.  Removed.
    if run_cap is not None and cluster_run > run_cap and not protected_onset_pairs:
        findings.append(
            f"same-shape cluster-chain run {cluster_run} exceeds cap {run_cap}"
        )
    cluster_size_cap = MAX_SHAPE_CLUSTER_SIZE.get(difficulty)
    max_cluster = int(metrics.get("max_shape_cluster_size", 0))
    if cluster_size_cap is not None and max_cluster > cluster_size_cap and not protected_onset_pairs:
        findings.append(
            f"max shape cluster size {max_cluster} exceeds cap {cluster_size_cap} (#532)"
        )

    if not onset_timed:
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

    # Issue #420 — circle/lane-0 reachability floors at medium/hard.
    share_floor = CIRCLE_LANE0_SHARE_FLOOR.get(difficulty)
    if share_floor is not None:
        circle_share = metrics.get("circle_share")
        lane0_share = metrics.get("lane0_share")
        if circle_share is not None and float(circle_share) < share_floor:
            findings.append(
                f"circle share {float(circle_share):.1%} below floor "
                f"{share_floor:.0%} (issue #420)"
            )
        if lane0_share is not None and float(lane0_share) < share_floor:
            findings.append(
                f"lane-0 share {float(lane0_share):.1%} below floor "
                f"{share_floor:.0%} (issue #420)"
            )

    min_ioi_ms = metrics.get("min_ioi_ms")
    if min_ioi_ms is not None:
        floor = MIN_IOI_MS.get(difficulty)
        if floor is not None and float(min_ioi_ms) < floor:
            findings.append(f"min IOI {float(min_ioi_ms):.1f}ms below floor {floor:.0f}ms")

    if bool(metrics.get("ioi_index_error")):
        findings.append("beat index out of range for beat_times array")

    return findings


def evaluate_cluster_advisories(
    metrics: dict[str, float | int | bool | None], difficulty: str
) -> list[str]:
    advisories: list[str] = []
    warn_size = DENSE_CLUSTER_WARN_SIZE.get(difficulty)
    if warn_size is None:
        return advisories
    max_cluster = int(metrics.get("max_shape_cluster_size", 0))
    cluster_size_cap = MAX_SHAPE_CLUSTER_SIZE.get(difficulty)
    # Only emit the diagnostic note when the cluster is in the
    # "warn but not yet hard-fail" band (warn_size < size <= cap).
    # Sizes above the cap are handled by ``evaluate_content_gates``
    # as a hard finding (#532); no advisory needed.
    upper_bound = cluster_size_cap if cluster_size_cap is not None else float("inf")
    if max_cluster > warn_size and max_cluster <= upper_bound:
        advisories.append(
            f"dense readability cluster size={max_cluster} (> {warn_size}) may inflate raw shape_run={int(metrics.get('longest_same_shape_run', 0))}"
        )
    return advisories


def validate_beatmap(
    path: Path,
) -> list[tuple[str, dict[str, float | int | bool | None], list[str], list[str]]]:
    beatmap = json.loads(path.read_text())
    difficulties = beatmap.get("difficulties", {})
    beat_times = beatmap.get("beat_times", [])
    results: list[tuple[str, dict[str, float | int | bool | None], list[str], list[str]]] = []

    for difficulty in ("easy", "medium", "hard"):
        if difficulty not in difficulties:
            continue
        payload = difficulties[difficulty]
        beats = payload.get("beats", [])
        expected_count = payload.get("count")
        metrics = calculate_content_metrics(beats, beat_times=beat_times, expected_count=expected_count)
        warn_size = DENSE_CLUSTER_WARN_SIZE.get(difficulty)
        if warn_size is not None:
            metrics["shape_clusters_over_warn"] = sum(
                1 for cluster in _shape_gate_clusters(_ordered_valid_beats(beats)) if len(cluster) > warn_size
            )
        findings = evaluate_content_gates(metrics, difficulty)
        advisories = evaluate_cluster_advisories(metrics, difficulty)
        results.append((difficulty, metrics, findings, advisories))

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
        for difficulty, metrics, findings, advisories in validate_beatmap(path):
            status = "PASS" if not findings else f"FAIL ({len(findings)})"
            print(
                f"  [{difficulty:6s}] {status} "
                f"obs={metrics['total_obstacles']} "
                f"dom_gap={float(metrics['dominant_gap_share']):.1%} "
                f"gap1={float(metrics['gap_one_share']):.1%} "
                f"gap1_run={int(metrics['gap_one_run'])} "
                f"shape_run={int(metrics['longest_same_shape_run'])} "
                f"shape_cluster_run={int(metrics['longest_same_shape_cluster_run'])} "
                f"max_shape_cluster={int(metrics['max_shape_cluster_size'])} "
                f"dense_clusters={int(metrics['shape_clusters_over_warn'])} "
                f"tri={float(metrics['triangle_share']):.1%} "
                f"cir={float(metrics['circle_share']):.1%}"
            )
            for finding in findings:
                print(f"      - {finding}")
            for advisory in advisories:
                print(f"      ~ {advisory}")
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
