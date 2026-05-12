#!/usr/bin/env python3
"""Unit tests for validate_beatmap_offset.py."""

from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).parent))

import validate_beatmap_offset as offset_validator  # noqa: E402


class _WorkingDirectory:
    def __init__(self, destination: Path):
        self.destination = destination
        self.previous: str | None = None

    def __enter__(self):
        self.previous = os.getcwd()
        os.chdir(self.destination)
        return self

    def __exit__(self, exc_type, exc, tb):
        if self.previous is not None:
            os.chdir(self.previous)


class TestValidateBeatmapOffset(unittest.TestCase):
    def test_beatmap_dir_is_repo_relative(self):
        expected = Path(offset_validator.__file__).resolve().parent.parent / "content" / "beatmaps"
        self.assertEqual(offset_validator.BEATMAP_DIR, expected)

    def test_main_runs_outside_repo_root(self):
        with mock.patch.object(offset_validator, "validate_beatmap_pair", return_value=[]):
            with _WorkingDirectory(Path(__file__).resolve().parent):
                rc = offset_validator.main([])
        self.assertEqual(rc, 0)

    def test_rejects_non_anchor_beat_index_outside_analysis_range(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            beatmap_path = tmp_path / "fixture_beatmap.json"
            analysis_path = tmp_path / "fixture_analysis.json"
            beatmap_path.write_text(
                """{
                    "bpm": 120.0,
                    "offset": 0.0,
                    "difficulties": {
                        "easy": {
                            "beats": [
                                {"beat": 2},
                                {"beat": 4},
                                {"beat": 100}
                            ]
                        }
                    }
                }"""
            )
            analysis_path.write_text(
                """{
                    "beats": [
                        0.0, 0.5, 1.0, 1.5, 2.0,
                        2.5, 3.0, 3.5, 4.0, 4.5
                    ]
                }"""
            )

            errors = offset_validator.validate_beatmap_pair(
                beatmap_path,
                analysis_path,
                offset_validator.TOLERANCE_MS_DEFAULT,
            )

        self.assertEqual(len(errors), 1)
        self.assertIn("authored beat index out of range", errors[0])
        self.assertIn("100", errors[0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
