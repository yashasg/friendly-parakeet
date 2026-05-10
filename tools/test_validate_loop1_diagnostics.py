#!/usr/bin/env python3
"""Unit tests for validate_loop1_diagnostics.py."""

from __future__ import annotations

import json
import shutil
import sys
import unittest
from collections import Counter
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).parent))

import validate_loop1_diagnostics as diagnostics  # noqa: E402


class TestValidateLoop1Diagnostics(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_validate_loop1_diagnostics"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write_analysis(self, name: str, payload: dict | None = None) -> Path:
        path = self.base / name
        path.write_text(json.dumps(payload or {"title": "Fixture"}), encoding="utf-8")
        return path

    def test_validate_analysis_flags_unknown_bins(self):
        path = self._write_analysis("fixture_analysis.json")
        with mock.patch.object(
            diagnostics,
            "summarize_difficulty",
            return_value=(Counter({"hexagon": 2}), Counter({"mystery": 1})),
        ):
            errors = diagnostics.validate_analysis(path)
        self.assertTrue(any("unknown shape bins" in e for e in errors))
        self.assertTrue(any("unknown subdivision bins" in e for e in errors))

    def test_validate_analysis_handles_generation_exception(self):
        path = self._write_analysis("fixture_analysis.json")
        with mock.patch.object(diagnostics, "summarize_difficulty", side_effect=RuntimeError("boom")):
            errors = diagnostics.validate_analysis(path)
        self.assertEqual(len(errors), 3)
        self.assertTrue(all("generation failed" in e for e in errors))

    def test_main_returns_error_when_no_analysis_files(self):
        with mock.patch.object(diagnostics, "DEFAULT_DIR", self.base):
            with mock.patch.object(sys, "argv", ["validate_loop1_diagnostics.py"]):
                rc = diagnostics.main()
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
