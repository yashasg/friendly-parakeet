#!/usr/bin/env python3
"""Unit tests for validate_medium_balance.py."""

from __future__ import annotations

import json
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import validate_medium_balance as medium_balance  # noqa: E402


class TestValidateMediumBalance(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_validate_medium_balance"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write_beatmap(self, name: str, beats: list[dict]) -> Path:
        path = self.base / name
        payload = {"title": "Fixture", "difficulties": {"medium": {"beats": beats}}}
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def test_validate_beatmap_passes_within_target_ranges(self):
        beats = (
            [{"kind": "shape_gate", "shape": "circle", "lane": 2} for _ in range(3)]
            + [{"kind": "shape_gate", "shape": "square", "lane": 1} for _ in range(10)]
            + [{"kind": "shape_gate", "shape": "triangle", "lane": 0} for _ in range(7)]
        )
        path = self._write_beatmap("fixture_pass_beatmap.json", beats)
        issues = medium_balance.validate_beatmap(path)
        self.assertEqual(issues, [])

    def test_validate_beatmap_flags_shape_and_lane_distribution(self):
        beats = [{"kind": "shape_gate", "shape": "circle", "lane": 2} for _ in range(10)]
        path = self._write_beatmap("fixture_fail_beatmap.json", beats)
        issues = medium_balance.validate_beatmap(path)
        self.assertGreaterEqual(len(issues), 4)
        self.assertTrue(any("circle" in issue for issue in issues))
        self.assertTrue(any("lane 2" in issue for issue in issues))


if __name__ == "__main__":
    unittest.main(verbosity=2)
