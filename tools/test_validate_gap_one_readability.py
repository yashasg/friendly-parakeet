#!/usr/bin/env python3
"""Unit tests for validate_gap_one_readability.py."""

from __future__ import annotations

import contextlib
import io
import json
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import validate_gap_one_readability as gap_one  # noqa: E402


class TestValidateGapOneReadability(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_validate_gap_one_readability"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write_beatmap(self, filename: str, payload: dict) -> Path:
        path = self.base / filename
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def test_main_skips_invalid_rows_and_keeps_evaluating_valid_rows(self):
        path = self._write_beatmap(
            "fixture_beatmap.json",
            {
                "difficulties": {
                    "easy": {
                        "beats": [
                            {"beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 0},
                            {"beat": "bad"},
                            {"lane": 1},
                            {"beat": 1, "kind": "shape_gate", "shape": "circle", "lane": 0},
                        ]
                    }
                }
            },
        )

        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            rc = gap_one.main([str(path)])

        self.assertEqual(rc, 1)
        self.assertIn("FAIL (1 violations)", stdout.getvalue())
        self.assertIn("skipped 2 invalid beat row(s)", stderr.getvalue())

    def test_main_passes_when_only_invalid_rows_remain(self):
        path = self._write_beatmap(
            "fixture_pass_beatmap.json",
            {
                "difficulties": {
                    "hard": {
                        "beats": [
                            {"beat": "bad"},
                            {"beat": None},
                            {"shape": "circle"},
                        ]
                    }
                }
            },
        )

        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            rc = gap_one.main([str(path)])

        self.assertEqual(rc, 0)
        self.assertIn("PASS: all beatmaps satisfy gap=1 readability rules.", stdout.getvalue())
        self.assertIn("skipped 3 invalid beat row(s)", stderr.getvalue())


if __name__ == "__main__":
    unittest.main(verbosity=2)
