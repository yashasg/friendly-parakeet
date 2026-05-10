#!/usr/bin/env python3
"""Unit tests for validate_difficulty_ramp.py."""

from __future__ import annotations

import contextlib
import io
import json
import shutil
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).parent))

import validate_difficulty_ramp as ramp  # noqa: E402


class TestValidateDifficultyRamp(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_validate_difficulty_ramp"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write_beatmap(self, name: str, payload: dict) -> Path:
        path = self.base / name
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def test_easy_shape_gate_only_flags_forbidden_kind(self):
        violations = ramp.check_easy_shape_gate_only("fixture", [{"kind": "shape_gate"}, {"kind": "wall"}])
        self.assertEqual(len(violations), 1)
        self.assertIn("forbidden obstacle", violations[0])

    def test_easy_variety_flags_dominant_and_low_variety(self):
        beats = [{"kind": "shape_gate", "shape": "circle"} for _ in range(5)]
        violations = ramp.check_easy_variety("fixture", beats)
        self.assertTrue(any("distinct shape" in v for v in violations))
        self.assertTrue(any("dominates" in v for v in violations))

    def test_main_passes_for_valid_fixture(self):
        # Counts and IOIs must satisfy strict ramps now (#529): easy 3 < medium 5 < hard 7,
        # and IOIs must be non-inverting.
        self._write_beatmap(
            "fixture_beatmap.json",
            {
                "difficulties": {
                    "easy": {
                        "beats": [
                            {"kind": "shape_gate", "shape": "circle",   "time_sec": 0.0},
                            {"kind": "shape_gate", "shape": "square",   "time_sec": 4.0},
                            {"kind": "shape_gate", "shape": "triangle", "time_sec": 8.0},
                        ]
                    },
                    "medium": {
                        "beats": [
                            {"kind": "shape_gate", "shape": "square",   "time_sec": i * 2.0}
                            for i in range(5)
                        ]
                    },
                    "hard": {
                        "beats": [
                            {"kind": "shape_gate", "shape": "triangle", "time_sec": i * 1.0}
                            for i in range(7)
                        ]
                    },
                }
            },
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        with mock.patch.object(ramp, "BEATMAP_DIR", self.base):
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                rc = ramp.main()
        self.assertEqual(rc, 0, msg=stderr.getvalue())
        self.assertIn("OK: difficulty ramp checks passed", stdout.getvalue())

    def test_main_fails_on_deprecated_lane_block(self):
        self._write_beatmap(
            "fixture_beatmap.json",
            {
                "difficulties": {
                    "easy": {"beats": [{"kind": "shape_gate", "shape": "circle"}]},
                    "medium": {"beats": [{"kind": "lane_block"}]},
                    "hard": {"beats": [{"kind": "shape_gate", "shape": "triangle"}]},
                }
            },
        )
        stderr = io.StringIO()
        with mock.patch.object(ramp, "BEATMAP_DIR", self.base):
            with contextlib.redirect_stderr(stderr):
                rc = ramp.main()
        self.assertEqual(rc, 1)
        self.assertIn("deprecated obstacle", stderr.getvalue())


    def test_count_ramp_inversion_fails(self):
        easy = [{"kind": "shape_gate"} for _ in range(50)]
        medium = [{"kind": "shape_gate"} for _ in range(40)]  # fewer than easy
        hard = [{"kind": "shape_gate"} for _ in range(60)]
        v = ramp.check_count_ramp("fixture", easy, medium, hard)
        self.assertTrue(any("count ramp" in s.lower() for s in v),
                        f"expected count-ramp violation, got {v}")

    def test_ioi_inversion_significant_fails(self):
        # Easy: ten events at 1.0s spacing → all IOIs = 1.0
        # Medium: ten events at 2.0s spacing → all IOIs = 2.0 (slower!)
        easy = [{"kind": "shape_gate", "time_sec": i * 1.0} for i in range(10)]
        medium = [{"kind": "shape_gate", "time_sec": i * 2.0} for i in range(10)]
        hard = [{"kind": "shape_gate", "time_sec": i * 0.5} for i in range(10)]
        v = ramp.check_median_ioi_ramp("fixture", easy, medium, hard)
        self.assertTrue(any("INVERSION" in s for s in v),
                        f"expected IOI inversion, got {v}")

    def test_ioi_uses_lower_quartile(self):
        """Issue #529 — bimodal IOI distributions must not produce false
        inversions when the dense-region pace ramps correctly."""
        # Easy: 10 events with IOIs ~ 1.0s tight + 5.0s sparse fills
        easy_ts = [0.0, 1.0, 2.0, 3.0, 4.0, 9.0, 14.0, 19.0, 20.0, 21.0]
        # Medium: tighter dense bursts (0.5s) + sparse fills
        med_ts = [0.0, 0.5, 1.0, 1.5, 2.0, 7.0, 12.0, 17.0, 17.5, 18.0]
        # Hard: even tighter
        hard_ts = [0.0, 0.25, 0.5, 0.75, 1.0, 6.0, 11.0, 16.0, 16.25, 16.5]
        easy = [{"kind": "shape_gate", "time_sec": t} for t in easy_ts]
        medium = [{"kind": "shape_gate", "time_sec": t} for t in med_ts]
        hard = [{"kind": "shape_gate", "time_sec": t} for t in hard_ts]
        v = ramp.check_median_ioi_ramp("fixture", easy, medium, hard)
        self.assertEqual(v, [], f"expected no inversion, got {v}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
