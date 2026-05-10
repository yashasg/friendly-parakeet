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
        self._write_beatmap(
            "fixture_beatmap.json",
            {
                "difficulties": {
                    "easy": {
                        "beats": [
                            {"kind": "shape_gate", "shape": "circle"},
                            {"kind": "shape_gate", "shape": "square"},
                            {"kind": "shape_gate", "shape": "triangle"},
                        ]
                    },
                    "medium": {"beats": [{"kind": "shape_gate", "shape": "square"}]},
                    "hard": {"beats": [{"kind": "shape_gate", "shape": "triangle"}]},
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
