#!/usr/bin/env python3
"""Unit tests for validate_required_cross_lane_jump_time.py (issue #819)."""

from __future__ import annotations

import contextlib
import io
import json
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import validate_required_cross_lane_jump_time as v  # noqa: E402


def _gate(t, lane, shape):
    return {"kind": "shape_gate", "time_sec": t, "lane": lane, "shape": shape}


def _marker(t, lane, shape):
    return {"kind": "onset_marker", "time_sec": t, "lane": lane, "shape": shape}


class TestRequiredCrossLaneJumpTime(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_required_cross_lane_jump_time"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write_beatmap(self, filename: str, beats: list[dict]) -> Path:
        path = self.base / filename
        path.write_text(json.dumps({"difficulties": {"easy": {"beats": beats}}}), encoding="utf-8")
        return path

    def test_short_zero_to_two_shape_change_is_flagged(self):
        beats = [_gate(10.0, 0, "circle"), _gate(11.0, 2, "triangle")]
        findings = v.find_short_cross_lane_shape_jumps(beats)
        self.assertEqual(len(findings), 1)

    def test_short_two_to_zero_shape_change_is_flagged(self):
        beats = [_gate(10.0, 2, "triangle"), _gate(10.75, 0, "circle")]
        findings = v.find_short_cross_lane_shape_jumps(beats)
        self.assertEqual(len(findings), 1)

    def test_non_blocking_marker_does_not_create_required_pair(self):
        beats = [_gate(10.0, 0, "circle"), _marker(10.5, 2, "triangle"), _gate(11.5, 2, "triangle")]
        findings = v.find_short_cross_lane_shape_jumps(beats)
        self.assertEqual(findings, [])

    def test_adjacent_same_shape_or_nearby_lane_passes(self):
        self.assertEqual(v.find_short_cross_lane_shape_jumps([_gate(0.0, 0, "circle"), _gate(0.5, 2, "circle")]), [])
        self.assertEqual(v.find_short_cross_lane_shape_jumps([_gate(0.0, 0, "circle"), _gate(0.5, 1, "square")]), [])

    def test_pair_at_floor_passes(self):
        beats = [_gate(10.0, 0, "circle"), _gate(11.2, 2, "triangle")]
        findings = v.find_short_cross_lane_shape_jumps(beats)
        self.assertEqual(findings, [])

    def test_main_reports_violation(self):
        path = self._write_beatmap("fixture_beatmap.json", [_gate(0.0, 0, "circle"), _gate(1.0, 2, "triangle")])
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            rc = v.main([str(path)])
        self.assertEqual(rc, 1)
        self.assertIn("REQUIRED CROSS-LANE JUMP VIOLATIONS", stderr.getvalue())


if __name__ == "__main__":
    unittest.main(verbosity=2)
