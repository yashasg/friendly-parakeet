#!/usr/bin/env python3
"""Validate experimental onset-timing spike diagnostics artifacts."""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path

import level_designer as ld

DIFFICULTIES = ("easy", "medium", "hard")
MIN_IOI_MS_FLOOR = {difficulty: float(ld.MIN_IOI_MS[difficulty]) for difficulty in DIFFICULTIES}
MAX_DENSE_CLUSTER_LEN = {"easy": 2, "medium": 3, "hard": 4}
MAX_SHORT_IOI_SHARE = {"easy": 0.02, "medium": 0.20, "hard": 0.30}
MAX_RESIDUAL_MEDIAN_ABS_MS = {"easy": 65.0, "medium": 65.0, "hard": 65.0}
MAX_RESIDUAL_P90_ABS_MS = {"easy": 120.0, "medium": 120.0, "hard": 120.0}
MIN_EVENT_COUNT = {"easy": 20, "medium": 30, "hard": 40}
MAX_EVENTS_PER_MINUTE = {"easy": 120.0, "medium": 200.0, "hard": 260.0}
MIN_BEAT_LABEL_KINDS = {"easy": 1, "medium": 2, "hard": 2}
MAX_OFFGRID_SHARE = {"easy": 0.15, "medium": 0.25, "hard": 0.30}
BEAT_LABELS = {"downbeat", "eighth", "triplet"}
PUBLIC_LAYERS = {"percussive", "harmonic", "full-spectrum"}


def _share(value: int, total: int) -> float:
    return (value / total) if total > 0 else 0.0


def load_summary(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def load_onset_rows(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def _validate_onset_only_timing(summary: dict, rows: list[dict]) -> list[str]:
    """Enforce the onset-only timing invariant.

    Fails when any CSV row carries a non-onset ``timing_source`` and when any
    difficulty's ``timing_source_histogram`` has a nonzero entry whose key is
    not ``onset``.
    """
    findings: list[str] = []
    for idx, row in enumerate(rows):
        ts = row.get("timing_source")
        if ts is None or ts == "":
            findings.append(f"row {idx} ({row.get('difficulty', '?')}) missing timing_source")
            continue
        if ts != "onset":
            findings.append(
                f"row {idx} ({row.get('difficulty', '?')}) has non-onset "
                f"timing_source={ts!r} (only 'onset' is permitted)"
            )

    experimental = summary.get("experimental_onset_timing")
    if not isinstance(experimental, dict):
        return findings
    comparison = experimental.get("comparison_by_difficulty")
    if not isinstance(comparison, dict):
        return findings
    for difficulty in DIFFICULTIES:
        payload = comparison.get(difficulty)
        if not isinstance(payload, dict):
            continue
        event_counts = payload.get("event_counts")
        if not isinstance(event_counts, dict):
            continue
        hist = event_counts.get("timing_source_histogram")
        if not isinstance(hist, dict):
            findings.append(f"[{difficulty}] event_counts.timing_source_histogram must be an object")
            continue
        for key, value in hist.items():
            try:
                count = int(value)
            except (TypeError, ValueError):
                findings.append(
                    f"[{difficulty}] timing_source_histogram entry {key!r} is non-integer: {value!r}"
                )
                continue
            if key != "onset" and count != 0:
                findings.append(
                    f"[{difficulty}] timing_source_histogram has non-onset entry "
                    f"{key!r}={count} (only 'onset' is permitted to be nonzero)"
                )
    return findings


def validate_artifact_shape(summary: dict, rows: list[dict]) -> list[str]:
    findings: list[str] = []
    raw_per_pass = summary.get("onset_pool_summary", {}).get("raw_per_pass", {})
    if isinstance(raw_per_pass, dict):
        leaked = sorted(set(raw_per_pass) - PUBLIC_LAYERS)
        if leaked:
            findings.append(
                "onset_pool_summary.raw_per_pass leaks non-public pass names: "
                f"{leaked}"
            )

    experimental = summary.get("experimental_onset_timing")
    if not isinstance(experimental, dict):
        return ["summary missing experimental_onset_timing block"]
    if not bool(experimental.get("enabled")):
        findings.append("experimental_onset_timing.enabled must be true for spike diagnostics")

    comparison = experimental.get("comparison_by_difficulty")
    if not isinstance(comparison, dict):
        findings.append("experimental_onset_timing.comparison_by_difficulty must be an object")
        return findings

    required_outer = {
        "event_counts",
        "residuals_onset_minus_beat_ms",
        "ioi_stats_ms",
        "dense_cluster_stats",
        "subdivision_label_distribution",
        "onset_class_distribution",
        "event_role_distribution",
    }
    required_event_counts = {"obstacles", "timing_source_histogram"}
    required_stats = {"beat_snap", "onset_timing"}
    required_ioi_fields = {"count", "min_ms", "median_ms", "p90_ms", "mean_ms", "max_ms"}
    required_dense_fields = {"threshold_ms", "short_ioi_count", "short_ioi_share", "cluster_count", "max_cluster_len"}
    required_residual_fields = {"count", "median_abs_ms", "p90_abs_ms", "mean_abs_ms", "max_abs_ms"}

    rows_by_diff = {difficulty: [] for difficulty in DIFFICULTIES}
    for row in rows:
        difficulty = row.get("difficulty")
        if difficulty in rows_by_diff:
            rows_by_diff[difficulty].append(row)

    required_csv_fields = {
        "difficulty",
        "event_order",
        "beat_idx",
        "beat_time",
        "onset_time",
        "residual_ms",
        "timing_source",
        "subdivision",
        "source_event_idx",
        "onset_class",
        "event_role",
        "difficulty_inclusion",
    }
    if rows:
        missing_csv = sorted(required_csv_fields - set(rows[0]))
        if missing_csv:
            findings.append(f"onset_timing_events.csv missing fields: {missing_csv}")
    else:
        findings.append("onset_timing_events.csv is empty")

    for difficulty in DIFFICULTIES:
        payload = comparison.get(difficulty)
        if not isinstance(payload, dict):
            findings.append(f"comparison_by_difficulty missing '{difficulty}'")
            continue

        missing_outer = sorted(required_outer - set(payload))
        if missing_outer:
            findings.append(f"[{difficulty}] missing summary keys: {missing_outer}")
            continue

        event_counts = payload["event_counts"]
        if not isinstance(event_counts, dict):
            findings.append(f"[{difficulty}] event_counts must be an object")
            continue
        missing_event = sorted(required_event_counts - set(event_counts))
        if missing_event:
            findings.append(f"[{difficulty}] event_counts missing keys: {missing_event}")

        residuals = payload["residuals_onset_minus_beat_ms"]
        if not isinstance(residuals, dict):
            findings.append(f"[{difficulty}] residuals_onset_minus_beat_ms must be an object")
        else:
            missing_residual = sorted(required_residual_fields - set(residuals))
            if missing_residual:
                findings.append(f"[{difficulty}] residual summary missing keys: {missing_residual}")

        ioi_stats = payload["ioi_stats_ms"]
        dense_stats = payload["dense_cluster_stats"]
        for block_name, block_payload, required_fields in (
            ("ioi_stats_ms", ioi_stats, required_ioi_fields),
            ("dense_cluster_stats", dense_stats, required_dense_fields),
        ):
            if not isinstance(block_payload, dict):
                findings.append(f"[{difficulty}] {block_name} must be an object")
                continue
            missing_diff_stats = sorted(required_stats - set(block_payload))
            if missing_diff_stats:
                findings.append(
                    f"[{difficulty}] {block_name} missing timing variants: {missing_diff_stats}"
                )
                continue
            for variant in required_stats:
                variant_payload = block_payload[variant]
                if not isinstance(variant_payload, dict):
                    findings.append(f"[{difficulty}] {block_name}.{variant} must be an object")
                    continue
                missing_variant = sorted(required_fields - set(variant_payload))
                if missing_variant:
                    findings.append(
                        f"[{difficulty}] {block_name}.{variant} missing keys: {missing_variant}"
                    )

        expected_rows = int(event_counts.get("obstacles", -1))
        actual_rows = len(rows_by_diff[difficulty])
        if expected_rows >= 0 and expected_rows != actual_rows:
            findings.append(
                f"[{difficulty}] onset row count mismatch: summary={expected_rows} csv={actual_rows}"
            )

    findings.extend(_validate_onset_only_timing(summary, rows))
    return findings


def _events_per_minute(rows: list[dict]) -> float:
    if len(rows) < 2:
        return 0.0
    times = sorted(float(row["onset_time"]) for row in rows)
    duration_s = times[-1] - times[0]
    if duration_s <= 0:
        return 0.0
    return (len(times) / duration_s) * 60.0


def _layer_ioi_metrics(rows: list[dict], threshold_ms: float) -> dict[str, float]:
    interval_count = 0
    short_count = 0
    max_cluster_len = 0
    min_ioi = float("inf")
    by_layer: dict[str, list[float]] = {}
    for row in rows:
        layer = row.get("onset_class") or "unknown"
        by_layer.setdefault(layer, []).append(float(row["onset_time"]))

    for times in by_layer.values():
        times = sorted(times)
        current_cluster = 1
        for prev, cur in zip(times, times[1:]):
            interval = (cur - prev) * 1000.0
            min_ioi = min(min_ioi, interval)
            interval_count += 1
            if interval < threshold_ms:
                short_count += 1
                current_cluster += 1
                max_cluster_len = max(max_cluster_len, current_cluster)
            else:
                current_cluster = 1

    return {
        "min_ioi_ms": 0.0 if min_ioi == float("inf") else min_ioi,
        "short_ioi_share": _share(short_count, interval_count),
        "max_dense_cluster_len": float(max_cluster_len),
    }


def evaluate_spike_gates(summary: dict, rows: list[dict]) -> tuple[dict[str, dict[str, float]], list[str]]:
    findings: list[str] = []
    metrics: dict[str, dict[str, float]] = {}
    comparison = summary["experimental_onset_timing"]["comparison_by_difficulty"]
    rows_by_diff = {difficulty: [] for difficulty in DIFFICULTIES}
    for row in rows:
        difficulty = row.get("difficulty")
        if difficulty in rows_by_diff:
            rows_by_diff[difficulty].append(row)

    for difficulty in DIFFICULTIES:
        payload = comparison[difficulty]
        onset_ioi = payload["ioi_stats_ms"]["onset_timing"]
        dense = payload["dense_cluster_stats"]["onset_timing"]
        residuals = payload["residuals_onset_minus_beat_ms"]
        subdivision_hist = payload["subdivision_label_distribution"]
        event_counts = payload["event_counts"]

        event_count = int(event_counts.get("obstacles", 0))
        epm = _events_per_minute(rows_by_diff[difficulty])
        layer_ioi = _layer_ioi_metrics(rows_by_diff[difficulty], MIN_IOI_MS_FLOOR[difficulty])
        total_labels = sum(int(v) for v in subdivision_hist.values())
        beat_label_kinds = sum(
            1 for label in BEAT_LABELS if int(subdivision_hist.get(label, 0)) > 0
        )
        offgrid_share = _share(int(subdivision_hist.get("offgrid", 0)), total_labels)

        metrics[difficulty] = {
            "events": float(event_count),
            "epm": epm,
            "min_ioi_ms": layer_ioi["min_ioi_ms"],
            "short_ioi_share": layer_ioi["short_ioi_share"],
            "max_dense_cluster_len": layer_ioi["max_dense_cluster_len"],
            "residual_median_abs_ms": float(residuals["median_abs_ms"]),
            "residual_p90_abs_ms": float(residuals["p90_abs_ms"]),
            "beat_label_kinds": float(beat_label_kinds),
            "offgrid_share": offgrid_share,
        }

        if event_count < MIN_EVENT_COUNT[difficulty]:
            findings.append(
                f"[{difficulty}] event count {event_count} below floor {MIN_EVENT_COUNT[difficulty]}"
            )
        if epm > MAX_EVENTS_PER_MINUTE[difficulty]:
            findings.append(
                f"[{difficulty}] event density {epm:.1f}/min above cap {MAX_EVENTS_PER_MINUTE[difficulty]:.1f}/min"
            )
        if layer_ioi["min_ioi_ms"] < MIN_IOI_MS_FLOOR[difficulty]:
            findings.append(
                f"[{difficulty}] same-layer min IOI {layer_ioi['min_ioi_ms']:.1f}ms below floor {MIN_IOI_MS_FLOOR[difficulty]:.1f}ms"
            )
        if int(layer_ioi["max_dense_cluster_len"]) > MAX_DENSE_CLUSTER_LEN[difficulty]:
            findings.append(
                f"[{difficulty}] same-layer dense cluster len {int(layer_ioi['max_dense_cluster_len'])} above cap {MAX_DENSE_CLUSTER_LEN[difficulty]}"
            )
        if layer_ioi["short_ioi_share"] > MAX_SHORT_IOI_SHARE[difficulty]:
            findings.append(
                f"[{difficulty}] same-layer short IOI share {layer_ioi['short_ioi_share']:.1%} above cap {MAX_SHORT_IOI_SHARE[difficulty]:.0%}"
            )
        if float(residuals["median_abs_ms"]) > MAX_RESIDUAL_MEDIAN_ABS_MS[difficulty]:
            findings.append(
                f"[{difficulty}] residual median {float(residuals['median_abs_ms']):.1f}ms above cap {MAX_RESIDUAL_MEDIAN_ABS_MS[difficulty]:.1f}ms"
            )
        if float(residuals["p90_abs_ms"]) > MAX_RESIDUAL_P90_ABS_MS[difficulty]:
            findings.append(
                f"[{difficulty}] residual p90 {float(residuals['p90_abs_ms']):.1f}ms above cap {MAX_RESIDUAL_P90_ABS_MS[difficulty]:.1f}ms"
            )
        if beat_label_kinds < MIN_BEAT_LABEL_KINDS[difficulty]:
            findings.append(
                f"[{difficulty}] beat/subdivision label coverage={beat_label_kinds} below floor {MIN_BEAT_LABEL_KINDS[difficulty]}"
            )
        if offgrid_share > MAX_OFFGRID_SHARE[difficulty]:
            findings.append(
                f"[{difficulty}] offgrid share {offgrid_share:.1%} above cap {MAX_OFFGRID_SHARE[difficulty]:.0%}"
            )

    return metrics, findings


def _diagnostic_dirs(root: Path) -> list[Path]:
    if (root / "snap_diagnostics_summary.json").exists() or (root / "onset_timing_events.csv").exists():
        return [root]
    return sorted(
        path.parent
        for path in root.rglob("snap_diagnostics_summary.json")
        if (path.parent / "onset_timing_events.csv").exists()
    )


def _validate_dir(diagnostics_dir: Path, strict: bool) -> int:
    summary_path = diagnostics_dir / "snap_diagnostics_summary.json"
    events_path = diagnostics_dir / "onset_timing_events.csv"
    if not summary_path.exists():
        print(f"ERROR: missing {summary_path}", file=sys.stderr)
        return 1

    summary = load_summary(summary_path)
    experimental = summary.get("experimental_onset_timing")
    experimental_enabled = isinstance(experimental, dict) and bool(experimental.get("enabled"))
    if not experimental_enabled:
        print(
            "ERROR: diagnostics summary is not experimental_onset_timing enabled; "
            "regenerate diagnostics in experimental mode to validate onset spike artifacts.",
            file=sys.stderr,
        )
        return 1
    if not events_path.exists():
        print(f"ERROR: missing {events_path}", file=sys.stderr)
        return 1
    rows = load_onset_rows(events_path)

    findings = validate_artifact_shape(summary, rows)
    metrics: dict[str, dict[str, float]] = {}
    if not findings:
        metrics, findings = evaluate_spike_gates(summary, rows)

    mode = "STRICT" if strict else "REPORT"
    print(f"Onset spike gate mode: {mode} ({diagnostics_dir})")
    for difficulty in DIFFICULTIES:
        if difficulty not in metrics:
            continue
        metric = metrics[difficulty]
        print(
            f"  [{difficulty:6s}] "
            f"events={int(metric['events'])} "
            f"epm={metric['epm']:.1f} "
            f"min_ioi={metric['min_ioi_ms']:.1f}ms "
            f"dense={int(metric['max_dense_cluster_len'])} "
            f"short_ioi={metric['short_ioi_share']:.1%} "
            f"res_med={metric['residual_median_abs_ms']:.1f}ms "
            f"res_p90={metric['residual_p90_abs_ms']:.1f}ms "
            f"labels={int(metric['beat_label_kinds'])}/3 "
            f"offgrid={metric['offgrid_share']:.1%}"
        )

    if findings:
        for finding in findings:
            print(f"  - {finding}")
        if strict:
            print(f"FAIL: {len(findings)} onset-spike finding(s).", file=sys.stderr)
            return 1
        print(f"WARN: {len(findings)} onset-spike finding(s) (report-only mode).", file=sys.stderr)
        return 0

    print("PASS: onset-spike diagnostics artifacts satisfy gates.")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--diagnostics-dir", required=True, help="Directory containing spike diagnostics artifacts.")
    parser.add_argument("--strict", action="store_true", help="Return non-zero when findings exist.")
    args = parser.parse_args(argv)

    diagnostics_dir = Path(args.diagnostics_dir)
    dirs = _diagnostic_dirs(diagnostics_dir)
    if not dirs:
        print(f"ERROR: no diagnostics artifacts found under {diagnostics_dir}", file=sys.stderr)
        return 1
    return 1 if any(_validate_dir(path, args.strict) != 0 for path in dirs) else 0


if __name__ == "__main__":
    sys.exit(main())
