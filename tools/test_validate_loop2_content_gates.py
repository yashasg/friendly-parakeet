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


class TestShippedBeatmapInvariants(unittest.TestCase):
    """Regression coverage for issues #414, #416, #418, #420 on shipped beatmaps."""

    SHIPPED_DIR = Path(__file__).resolve().parent.parent / "content" / "beatmaps"
    INCLUSION_ORDER = {"easy": 0, "medium": 1, "hard": 2}

    def _shipped_beatmaps(self):
        import json as _json
        for path in sorted(self.SHIPPED_DIR.glob("*_beatmap.json")):
            yield path, _json.loads(path.read_text())

    def test_no_difficulty_inclusion_leaks(self):
        """#414 — every event in a difficulty array must carry an inclusion
        tier that is <= the array's tier (easy ⊂ medium ⊂ hard)."""
        for path, beatmap in self._shipped_beatmaps():
            for difficulty, payload in beatmap.get("difficulties", {}).items():
                cap = self.INCLUSION_ORDER.get(difficulty)
                if cap is None:
                    continue
                leaks = [
                    beat for beat in payload.get("beats", [])
                    if self.INCLUSION_ORDER.get(beat.get("difficulty_inclusion"), -1) > cap
                ]
                self.assertEqual(
                    leaks, [],
                    f"{path.name} [{difficulty}]: {len(leaks)} higher-tier "
                    f"inclusion leaks (#414)"
                )

    def test_strictly_increasing_beat_ordinals(self):
        """#416 — shipped beat arrays must be strictly increasing by ordinal
        (no zero-IOI artifacts from same-grid-bin snaps)."""
        for path, beatmap in self._shipped_beatmaps():
            for difficulty, payload in beatmap.get("difficulties", {}).items():
                beats = payload.get("beats", [])
                ordered = [b["beat"] for b in beats if isinstance(b.get("beat"), int)]
                violations = [
                    (ordered[i - 1], ordered[i])
                    for i in range(1, len(ordered))
                    if ordered[i] <= ordered[i - 1]
                ]
                self.assertEqual(
                    violations, [],
                    f"{path.name} [{difficulty}]: {len(violations)} "
                    f"non-strictly-increasing beat ordinal pairs (#416)"
                )

    def test_drama_medium_to_hard_ioi_step_is_perceptible(self):
        """#418 — drama hard must be perceptibly denser than drama medium.

        Per-gate spec: hard targets MEDIAN_IOI ≤ 0.540s.  Allow small slack
        (≤ 50ms over the target), and require at least a 100ms step from
        medium to hard so the ramp is felt by the player.
        """
        import json as _json
        path = self.SHIPPED_DIR / "2_drama_beatmap.json"
        if not path.exists():
            self.skipTest("drama beatmap not present")
        beatmap = _json.loads(path.read_text())
        diffs = beatmap["difficulties"]

        def _median_ioi(beats_payload):
            times = sorted(float(b["time_sec"]) for b in beats_payload["beats"])
            iois = [times[i + 1] - times[i] for i in range(len(times) - 1)]
            if not iois:
                return 0.0
            s = sorted(iois)
            mid = len(s) // 2
            return float(s[mid] if len(s) % 2 else (s[mid - 1] + s[mid]) / 2)

        medium = _median_ioi(diffs["medium"])
        hard = _median_ioi(diffs["hard"])
        self.assertGreater(
            medium - hard, 0.100,
            f"drama medium→hard median IOI step too small: "
            f"medium={medium:.3f}s hard={hard:.3f}s (need >100ms step) (#418)"
        )
        # Hard target ceiling 0.540s, allow 50ms slack so song-specific
        # density variations don't break the gate.
        self.assertLess(
            hard, 0.540 + 0.050,
            f"drama hard median IOI {hard:.3f}s above target 0.540s "
            f"(+50ms slack) (#418)"
        )

    def test_circle_and_lane2_share_nontrivial_at_medium_and_hard(self):
        """#420 — every shipped beatmap×{medium,hard} must include a
        non-trivial share of ``shape == "circle"`` AND ``lane == 2`` so the
        third broad onset layer (harmonic) and the third lane are reachable
        design space rather than dead UI.  Floor: 10% per cohort.
        """
        floor = 0.10
        for path, beatmap in self._shipped_beatmaps():
            for difficulty in ("medium", "hard"):
                payload = beatmap.get("difficulties", {}).get(difficulty)
                if not payload:
                    continue
                beats = [
                    b for b in payload.get("beats", [])
                    if b.get("kind") == "shape_gate"
                ]
                if not beats:
                    continue
                circle_share = sum(1 for b in beats if b.get("shape") == "circle") / len(beats)
                lane2_share = sum(1 for b in beats if b.get("lane") == 2) / len(beats)
                self.assertGreaterEqual(
                    circle_share, floor,
                    f"{path.name} [{difficulty}]: circle share "
                    f"{circle_share:.1%} below 10% floor (#420)"
                )
                self.assertGreaterEqual(
                    lane2_share, floor,
                    f"{path.name} [{difficulty}]: lane-2 share "
                    f"{lane2_share:.1%} below 10% floor (#420)"
                )


if __name__ == "__main__":
    unittest.main(verbosity=2)
