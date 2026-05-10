#!/usr/bin/env python3
"""Unit tests for validate_beatmap_offset.py."""

from __future__ import annotations

import os
import sys
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
