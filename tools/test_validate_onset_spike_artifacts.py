#!/usr/bin/env python3
"""Unit tests for validate_onset_spike_artifacts.py."""

from __future__ import annotations

import csv
import json
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import validate_onset_spike_artifacts as spike  # noqa: E402


def _summary_fixture(
    *,
    min_ioi_ms: float = 820.0,
    short_ioi_share: float = 0.0,
    max_cluster_len: int = 1,
    residual_median: float = 30.0,
    residual_p90: float = 50.0,
    label_hist: dict[str, int] | None = None,
) -> dict:
    labels = label_hist or {"downbeat": 18, "eighth": 10, "triplet": 8}

    def payload(diff: str, count: int) -> dict:
        diff_min_ioi = min_ioi_ms
        if diff == "medium":
            diff_min_ioi = max(min_ioi_ms, 420.0)
        if diff == "hard":
            diff_min_ioi = max(min_ioi_ms, 340.0)
        return {
            "event_counts": {
                "obstacles": count,
                "timing_source_histogram": {"onset": count, "beat_fallback": 0},
            },
            "residuals_onset_minus_beat_ms": {
                "count": count,
                "median_abs_ms": residual_median,
                "p90_abs_ms": residual_p90,
                "mean_abs_ms": residual_median,
                "max_abs_ms": residual_p90,
            },
            "ioi_stats_ms": {
                "beat_snap": {"count": max(count - 1, 0), "min_ms": diff_min_ioi + 50.0, "median_ms": diff_min_ioi + 90.0, "p90_ms": diff_min_ioi + 120.0, "mean_ms": diff_min_ioi + 100.0, "max_ms": diff_min_ioi + 180.0},
                "onset_timing": {"count": max(count - 1, 0), "min_ms": diff_min_ioi, "median_ms": diff_min_ioi + 40.0, "p90_ms": diff_min_ioi + 80.0, "mean_ms": diff_min_ioi + 50.0, "max_ms": diff_min_ioi + 120.0},
            },
            "dense_cluster_stats": {
                "beat_snap": {"threshold_ms": spike.MIN_IOI_MS_FLOOR[diff], "short_ioi_count": 0, "short_ioi_share": 0.0, "cluster_count": 0, "max_cluster_len": 1},
                "onset_timing": {"threshold_ms": spike.MIN_IOI_MS_FLOOR[diff], "short_ioi_count": 0, "short_ioi_share": short_ioi_share, "cluster_count": 0, "max_cluster_len": max_cluster_len},
            },
            "subdivision_label_distribution": labels,
        }

    return {
        "song_id": "fixture_song",
        "experimental_onset_timing": {
            "enabled": True,
            "comparison_by_difficulty": {
                "easy": payload("easy", 36),
                "medium": payload("medium", 48),
                "hard": payload("hard", 64),
            },
        },
    }


def _rows_fixture() -> list[dict]:
    rows: list[dict] = []
    for diff, count, spacing in (("easy", 36, 0.85), ("medium", 48, 0.50), ("hard", 64, 0.35)):
        for idx in range(count):
            beat_time = idx * spacing
            rows.append({
                "difficulty": diff,
                "event_order": str(idx),
                "beat_idx": str(idx),
                "beat_time": f"{beat_time:.3f}",
                "onset_time": f"{beat_time + 0.03:.3f}",
                "residual_ms": "30.0",
                "timing_source": "onset",
                "subdivision": "downbeat" if idx % 3 == 0 else ("eighth" if idx % 3 == 1 else "triplet"),
                "source_event_idx": str(idx),
            })
    return rows


class TestOnsetSpikeArtifactShape(unittest.TestCase):
    def test_shape_validator_requires_experimental_block(self):
        findings = spike.validate_artifact_shape({"song_id": "x"}, rows=[])
        self.assertIn("summary missing experimental_onset_timing block", findings)

    def test_shape_validator_detects_row_count_mismatch(self):
        summary = _summary_fixture()
        rows = _rows_fixture()[:-1]
        findings = spike.validate_artifact_shape(summary, rows)
        self.assertTrue(any("row count mismatch" in finding for finding in findings))


class TestOnsetSpikeGates(unittest.TestCase):
    def test_evaluate_spike_gates_passes_fixture(self):
        summary = _summary_fixture()
        rows = _rows_fixture()
        _, findings = spike.evaluate_spike_gates(summary, rows)
        self.assertEqual(findings, [])

    def test_evaluate_spike_gates_flags_expected_failures(self):
        summary = _summary_fixture(
            min_ioi_ms=200.0,
            short_ioi_share=0.9,
            max_cluster_len=10,
            residual_median=90.0,
            residual_p90=160.0,
            label_hist={"offgrid": 40, "downbeat": 2},
        )
        rows = _rows_fixture()
        _, findings = spike.evaluate_spike_gates(summary, rows)
        self.assertGreaterEqual(len(findings), 8)
        self.assertTrue(any("min IOI" in finding for finding in findings))
        self.assertTrue(any("dense cluster len" in finding for finding in findings))
        self.assertTrue(any("residual p90" in finding for finding in findings))
        self.assertTrue(any("label coverage" in finding for finding in findings))


class TestOnsetSpikeCli(unittest.TestCase):
    def test_main_report_mode_returns_zero_for_findings(self):
        base = Path(__file__).resolve().parent / ".test_onset_spike_validator"
        if base.exists():
            shutil.rmtree(base)
        base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(base, ignore_errors=True))

        summary = _summary_fixture(min_ioi_ms=200.0, label_hist={"downbeat": 3, "offgrid": 25})
        (base / "snap_diagnostics_summary.json").write_text(json.dumps(summary), encoding="utf-8")
        with (base / "onset_timing_events.csv").open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(
                handle,
                fieldnames=[
                    "difficulty",
                    "event_order",
                    "beat_idx",
                    "beat_time",
                    "onset_time",
                    "residual_ms",
                    "timing_source",
                    "subdivision",
                    "source_event_idx",
                ],
            )
            writer.writeheader()
            writer.writerows(_rows_fixture())

        rc = spike.main(["--diagnostics-dir", str(base)])
        self.assertEqual(rc, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
