#!/usr/bin/env python3
"""Unit tests for validate_max_beat_gap.py."""

from __future__ import annotations

import contextlib
import io
import json
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import validate_max_beat_gap as max_gap  # noqa: E402


class TestValidateMaxBeatGap(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_validate_max_beat_gap"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write_beatmap(self, filename: str, payload: dict) -> Path:
        path = self.base / filename
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def test_main_skips_invalid_rows_and_reports_diagnostics(self):
        path = self._write_beatmap(
            "fixture_beatmap.json",
            {
                "difficulties": {
                    "easy": {
                        "beats": [
                            {"beat": 0, "kind": "shape_gate"},
                            {"beat": "bad"},
                            {"lane": 1},
                            {"beat": 5, "kind": "shape_gate"},
                        ]
                    }
                }
            },
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            rc = max_gap.main([str(path)])

        self.assertEqual(rc, 0)
        self.assertIn("skipped 2 invalid beat row(s)", stderr.getvalue())
        self.assertIn("OK: max beat gap checks passed", stdout.getvalue())

    def test_main_still_fails_on_valid_gap_violations(self):
        path = self._write_beatmap(
            "fixture_violation_beatmap.json",
            {
                "difficulties": {
                    "medium": {
                        "beats": [
                            {"beat": 0, "kind": "shape_gate"},
                            {"beat": "bad"},
                            {"beat": 40, "kind": "shape_gate"},
                        ]
                    }
                }
            },
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            rc = max_gap.main([str(path)])

        self.assertEqual(rc, 1)
        self.assertIn("skipped 1 invalid beat row(s)", stderr.getvalue())
        self.assertIn("MAX BEAT GAP VIOLATIONS", stderr.getvalue())

    def test_onset_mode_uses_time_based_gap(self):
        """When timing_source=onset on every row, gap is measured in seconds.

        Beat-ordinal gap is large (39) but real time gap is tiny (0.78s) at
        120 BPM. The beat-ordinal cap of 32 must be ignored in onset-only
        mode (#447 / #452) so this case is not flagged.
        """
        path = self._write_beatmap(
            "fixture_onset_dense_beatmap.json",
            {
                "bpm": 120.0,
                "difficulties": {
                    "medium": {
                        "beats": [
                            {"beat": 0,  "kind": "shape_gate", "time_sec": 0.0,
                             "timing_source": "onset"},
                            {"beat": 40, "kind": "shape_gate", "time_sec": 0.78,
                             "timing_source": "onset"},
                        ]
                    }
                },
            },
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            rc = max_gap.main([str(path)])
        self.assertEqual(rc, 0, msg=stderr.getvalue())

    def test_onset_mode_flags_real_time_silence(self):
        """A genuine multi-second silence is flagged in onset-only mode."""
        path = self._write_beatmap(
            "fixture_onset_silence_beatmap.json",
            {
                "bpm": 120.0,
                "difficulties": {
                    "medium": {
                        "beats": [
                            {"beat": 0, "kind": "shape_gate", "time_sec": 0.0,
                             "timing_source": "onset"},
                            {"beat": 1, "kind": "shape_gate", "time_sec": 30.0,
                             "timing_source": "onset"},
                        ]
                    }
                },
            },
        )
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            rc = max_gap.main([str(path)])
        self.assertEqual(rc, 1)
        self.assertIn("onset-mode limit", stderr.getvalue())


if __name__ == "__main__":
    unittest.main(verbosity=2)
