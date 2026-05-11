#!/usr/bin/env python3
"""Unit tests for validate_loop1_diagnostics.py."""

from __future__ import annotations

import json
import csv
import shutil
import sys
import unittest
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

    def _write_diagnostics(
        self,
        name: str,
        rows: list[dict],
        raw_per_pass: dict | None = None,
        summary_counts: dict | None = None,
    ) -> Path:
        path = self.base / name
        path.mkdir(parents=True, exist_ok=True)
        summary = {
            "song_id": "Fixture",
            "onset_pool_summary": {
                "raw_per_pass": raw_per_pass or {
                    "percussive": 1,
                    "harmonic": 1,
                    "full-spectrum": 1,
                }
            },
            "experimental_onset_timing": {
                "obstacle_counts_by_difficulty": summary_counts or {"easy": len(rows)},
            },
        }
        (path / "snap_diagnostics_summary.json").write_text(json.dumps(summary), encoding="utf-8")
        with (path / "onset_timing_events.csv").open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        return path

    def _write_beatmap(self, beatmap_dir: Path, song: str, counts: dict[str, int]) -> None:
        beatmap_dir.mkdir(parents=True, exist_ok=True)
        beatmap = {
            "difficulties": {
                difficulty: {"beats": [{} for _ in range(count)]}
                for difficulty, count in counts.items()
            }
        }
        (beatmap_dir / f"{song}_beatmap.json").write_text(json.dumps(beatmap), encoding="utf-8")

    def _row(self, *, onset_class: str = "percussive", subdivision: str = "downbeat", timing_source: str = "onset") -> dict:
        return {
            "difficulty": "easy",
            "event_order": "0",
            "beat_idx": "0",
            "beat_time": "0.0",
            "onset_time": "0.0",
            "residual_ms": "0.0",
            "timing_source": timing_source,
            "subdivision": subdivision,
            "source_event_idx": "0",
            "onset_class": onset_class,
        }

    def test_validate_diagnostics_flags_unknown_bins(self):
        path = self._write_diagnostics("fixture_loop1", [
            self._row(onset_class="kick", subdivision="mystery"),
        ])
        errors = diagnostics.validate_diagnostics_dir(path)
        self.assertTrue(any("unknown onset_class bins" in e for e in errors))
        self.assertTrue(any("unknown subdivision bins" in e for e in errors))

    def test_validate_diagnostics_flags_non_onset_timing_source(self):
        path = self._write_diagnostics("fixture_loop1", [
            self._row(timing_source="beat_fallback"),
        ])
        errors = diagnostics.validate_diagnostics_dir(path)
        self.assertTrue(any("non-onset timing sources" in e for e in errors))

    def test_validate_diagnostics_flags_shipped_count_drift(self):
        path = self._write_diagnostics(
            "fixture_loop1",
            [self._row()],
            summary_counts={"easy": 1},
        )
        beatmap_dir = self.base / "beatmaps"
        self._write_beatmap(beatmap_dir, "Fixture", {"easy": 2})

        errors = diagnostics.validate_diagnostics_dir(path, beatmap_dir)

        self.assertTrue(any("CSV easy rows=1 but shipped beatmap has 2" in e for e in errors))
        self.assertTrue(any("summary easy count=1 but shipped beatmap has 2" in e for e in errors))

    def test_main_returns_error_when_no_diagnostics_dirs(self):
        with mock.patch.object(diagnostics, "DEFAULT_DIR", self.base):
            with mock.patch.object(sys, "argv", ["validate_loop1_diagnostics.py"]):
                rc = diagnostics.main()
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
