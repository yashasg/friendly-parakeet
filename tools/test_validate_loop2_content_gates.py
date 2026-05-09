#!/usr/bin/env python3
"""Unit tests for validate_loop2_content_gates.py."""

from __future__ import annotations

import sys
import unittest
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
            {"beat": 9, "kind": "combo_gate", "shape": "triangle", "lane": 2},
            {"beat": 10, "kind": "shape_gate", "shape": "triangle", "lane": 2},
        ]

        metrics = gates.calculate_content_metrics(beats)

        self.assertEqual(metrics["total_obstacles"], 7)
        self.assertEqual(metrics["shape_gate_count"], 6)
        self.assertEqual(metrics["distinct_shapes"], 3)
        self.assertAlmostEqual(metrics["dominant_shape_pct"], 50.0)
        self.assertAlmostEqual(metrics["max_gap_dominance_pct"], 50.0)
        self.assertEqual(metrics["longest_same_shape_run"], 3)
        self.assertEqual(metrics["run_steps_ge_3"], 1)
        self.assertEqual(metrics["gap_one_pairs"], 3)
        self.assertEqual(metrics["unreadable_gap_one_pairs"], 2)


class TestLoop2GateEvaluation(unittest.TestCase):
    def test_evaluate_content_gates_flags_expected_issues(self):
        metrics = {
            "shape_gate_count": 10,
            "distinct_shapes": 2,
            "dominant_shape_pct": 76.0,
            "max_gap_dominance_pct": 55.0,
            "unreadable_gap_one_pairs": 1,
        }

        findings = gates.evaluate_content_gates(metrics, "hard")

        self.assertEqual(len(findings), 4)
        self.assertTrue(any("distinct_shapes" in finding for finding in findings))
        self.assertTrue(any("dominant_shape_pct" in finding for finding in findings))
        self.assertTrue(any("max_gap_dominance_pct" in finding for finding in findings))
        self.assertTrue(any("unreadable_gap_one_pairs" in finding for finding in findings))

    def test_non_strict_mode_is_non_blocking(self):
        rc = gates.main([
            "content/beatmaps/1_stomper_beatmap.json",
        ])
        self.assertEqual(rc, 0)

    def test_strict_mode_fails_on_findings(self):
        rc = gates.main([
            "--strict",
            "content/beatmaps/1_stomper_beatmap.json",
        ])
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
