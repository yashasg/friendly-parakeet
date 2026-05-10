#!/usr/bin/env python3
"""Unit tests for validate_loop2_content_gates.py."""

from __future__ import annotations

import sys
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import validate_loop2_content_gates as gates  # noqa: E402


class TestLoop2MetricCalculations(unittest.TestCase):
    def test_calculate_content_metrics(self):
        beats = [
            {"beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 0},
            {"beat": 1, "kind": "shape_gate", "shape": "circle", "lane": 0},
            {"beat": 2, "kind": "shape_gate", "shape": "square", "lane": 1},
            {"beat": 5, "kind": "shape_gate", "shape": "square", "lane": 1},
            {"beat": 7, "kind": "shape_gate", "shape": "square", "lane": 1},
            {"beat": 10, "kind": "shape_gate", "shape": "triangle", "lane": 2},
        ]
        beat_times = [i * 0.5 for i in range(20)]

        metrics = gates.calculate_content_metrics(beats, beat_times=beat_times, expected_count=6)

        self.assertEqual(metrics["total_obstacles"], 6)
        self.assertTrue(metrics["count_matches"])
        self.assertTrue(metrics["strictly_increasing"])
        self.assertAlmostEqual(metrics["dominant_gap_share"], 2 / 5)
        self.assertAlmostEqual(metrics["gap_one_share"], 2 / 5)
        self.assertEqual(metrics["gap_one_run"], 2)
        self.assertEqual(metrics["longest_same_shape_run"], 3)
        self.assertEqual(metrics["longest_same_shape_cluster_run"], 1)
        self.assertEqual(metrics["shape_cluster_count"], 3)
        self.assertEqual(metrics["max_shape_cluster_size"], 3)
        self.assertAlmostEqual(metrics["triangle_share"], 1 / 6)
        self.assertAlmostEqual(metrics["circle_share"], 2 / 6)
        self.assertIsNotNone(metrics["min_ioi_ms"])

    def test_cluster_aware_run_metric_ignores_dense_cluster_size(self):
        beats = [
            {"beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 0},
            {"beat": 1, "kind": "shape_gate", "shape": "circle", "lane": 0},
            {"beat": 2, "kind": "shape_gate", "shape": "circle", "lane": 0},
            {"beat": 3, "kind": "shape_gate", "shape": "circle", "lane": 0},
            {"beat": 7, "kind": "shape_gate", "shape": "square", "lane": 1},
        ]
        metrics = gates.calculate_content_metrics(beats, expected_count=5)

        self.assertEqual(metrics["longest_same_shape_run"], 4)
        self.assertEqual(metrics["longest_same_shape_cluster_run"], 1)
        self.assertEqual(metrics["max_shape_cluster_size"], 4)

    def test_count_check_uses_valid_beats_not_raw_rows(self):
        beats = [
            {"beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 0},
            {"beat": 2, "kind": "shape_gate", "shape": "square", "lane": 1},
            {"beat": "bad", "kind": "shape_gate", "shape": "triangle", "lane": 2},
        ]
        metrics = gates.calculate_content_metrics(beats, expected_count=2)

        self.assertEqual(metrics["raw_beat_rows"], 3)
        self.assertEqual(metrics["valid_beat_count"], 2)
        self.assertTrue(metrics["count_matches"])


class TestLoop2GateEvaluation(unittest.TestCase):
    def test_evaluate_content_gates_flags_expected_issues(self):
        metrics = {
            "expected_count": 5,
            "total_obstacles": 4,
            "valid_beat_count": 4,
            "raw_beat_rows": 5,
            "count_matches": False,
            "strictly_increasing": False,
            "all_shape_gate": False,
            "lane_range_ok": False,
            "dominant_gap": 2,
            "dominant_gap_share": 0.50,
            "gap_one_share": 0.30,
            "gap_one_run": 4,
            "longest_same_shape_run": 6,
            "longest_same_shape_cluster_run": 6,
            "max_shape_cluster_size": 3,
            "triangle_share": 0.1,
            "circle_share": 0.5,
            "min_ioi_ms": 250.0,
            "ioi_index_error": True,
        }

        findings = gates.evaluate_content_gates(metrics, "hard")

        self.assertGreaterEqual(len(findings), 9)
        self.assertTrue(any("count mismatch" in finding for finding in findings))
        self.assertTrue(any("valid_beats=4 raw_rows=5" in finding for finding in findings))
        self.assertTrue(any("not strictly increasing" in finding for finding in findings))
        self.assertTrue(any("dominant gap" in finding for finding in findings))
        self.assertTrue(any("same-shape cluster-chain run" in finding for finding in findings))
        self.assertTrue(any("hard triangle share" in finding for finding in findings))
        self.assertTrue(any("hard circle share" in finding for finding in findings))
        self.assertTrue(any("min IOI" in finding for finding in findings))

    def test_same_shape_gate_is_advisory_with_oversized_clusters(self):
        metrics = {
            "count_matches": True,
            "strictly_increasing": True,
            "all_shape_gate": True,
            "lane_range_ok": True,
            "dominant_gap_share": 0.10,
            "dominant_gap": 2,
            "gap_one_run": 0,
            "gap_one_share": 0.0,
            "longest_same_shape_cluster_run": 8,
            "max_shape_cluster_size": 5,
            "triangle_share": 0.3,
            "circle_share": 0.2,
            "min_ioi_ms": 450.0,
            "ioi_index_error": False,
        }
        findings = gates.evaluate_content_gates(metrics, "hard")
        self.assertFalse(any("same-shape cluster-chain run" in finding for finding in findings))

    def test_cluster_advisory_emitted_for_dense_cluster(self):
        metrics = {
            "longest_same_shape_run": 9,
            "max_shape_cluster_size": 4,
        }
        advisories = gates.evaluate_cluster_advisories(metrics, "hard")
        self.assertEqual(len(advisories), 2)
        self.assertIn("dense readability cluster", advisories[0])

    def test_non_strict_mode_is_non_blocking(self):
        rc = gates.main([
            "content/beatmaps/1_stomper_beatmap.json",
        ])
        self.assertEqual(rc, 0)

    def test_strict_mode_fails_on_findings(self):
        fake_metrics = {
            "total_obstacles": 3,
            "dominant_gap_share": 0.5,
            "gap_one_share": 0.0,
            "gap_one_run": 0,
            "longest_same_shape_run": 1,
            "longest_same_shape_cluster_run": 1,
            "max_shape_cluster_size": 1,
            "shape_clusters_over_warn": 0,
            "triangle_share": 0.3,
            "circle_share": 0.2,
        }
        fake_results = [("hard", fake_metrics, ["synthetic finding"], [])]
        with mock.patch.object(gates, "validate_beatmap", return_value=fake_results):
            rc = gates.main([
                "--strict",
                "content/beatmaps/1_stomper_beatmap.json",
            ])
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
