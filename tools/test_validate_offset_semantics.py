#!/usr/bin/env python3
"""Unit tests for validate_offset_semantics.py."""

from __future__ import annotations

import json
import shutil
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).parent))

import validate_offset_semantics as offset_semantics  # noqa: E402


class TestValidateOffsetSemantics(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_validate_offset_semantics"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write_pair(self, song: str, beatmap: dict, analysis: dict | None = None) -> None:
        (self.base / f"{song}_beatmap.json").write_text(json.dumps(beatmap), encoding="utf-8")
        if analysis is not None:
            (self.base / f"{song}_analysis.json").write_text(json.dumps(analysis), encoding="utf-8")

    def test_drift_ms_out_of_range_is_zero(self):
        err = offset_semantics.drift_ms([0.5, 1.0], offset=0.5, period=0.5, idx=5)
        self.assertEqual(err, 0.0)

    def test_validate_shipped_beatmaps_passes_with_matching_analysis(self):
        self._write_pair(
            "fixture",
            {
                "bpm": 120.0,
                "offset": 1.0,
                "difficulties": {
                    "easy": {"beats": [{"beat": 0, "kind": "shape_gate"}, {"beat": 1, "kind": "shape_gate"}]}
                },
            },
            {"beats": [1.0, 1.5, 2.0]},
        )
        with mock.patch.object(offset_semantics, "BEATMAP_DIR", self.base):
            failures = offset_semantics.validate_shipped_beatmaps(8.0)
        self.assertEqual(failures, [])

    def test_validate_shipped_beatmaps_flags_analysis_offset_mismatch(self):
        self._write_pair(
            "fixture",
            {
                "bpm": 120.0,
                "offset": 1.2,
                "difficulties": {"easy": {"beats": [{"beat": 0, "kind": "shape_gate"}]}}
            },
            {"beats": [1.0, 1.5]},
        )
        with mock.patch.object(offset_semantics, "BEATMAP_DIR", self.base):
            failures = offset_semantics.validate_shipped_beatmaps(8.0)
        self.assertTrue(any("pipeline contract violated" in failure for failure in failures))

    def test_validate_shipped_beatmaps_flags_missing_onset_fields(self):
        self._write_pair(
            "fixture",
            {
                "bpm": 120.0,
                "offset": 1.0,
                "difficulties": {"hard": {"beats": [{"beat": 2, "timing_source": "onset"}]}}
            },
            {"beats": [1.0, 1.5, 2.0]},
        )
        with mock.patch.object(offset_semantics, "BEATMAP_DIR", self.base):
            failures = offset_semantics.validate_shipped_beatmaps(8.0)
        self.assertTrue(any("missing onset_time_sec/time_sec" in failure for failure in failures))

    def test_run_all_returns_failure_when_any_suite_fails(self):
        with (
            mock.patch.object(offset_semantics, "test_uniform_beats_zero_drift", return_value=[]),
            mock.patch.object(offset_semantics, "test_jittered_beats_drift_grows", return_value=[]),
            mock.patch.object(offset_semantics, "test_offset_equals_beats_zero", return_value=[]),
            mock.patch.object(
                offset_semantics,
                "test_nonzero_first_beat_index_arrives_after_offset",
                return_value=[],
            ),
            mock.patch.object(offset_semantics, "test_offset_shift_is_global", return_value=[]),
            mock.patch.object(offset_semantics, "test_corrected_offset_reduces_drift", return_value=[]),
            mock.patch.object(offset_semantics, "validate_shipped_beatmaps", return_value=["synthetic failure"]),
        ):
            rc = offset_semantics.run_all(8.0)
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
