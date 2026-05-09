#!/usr/bin/env python3
"""Focused tests for experimental onset-timing spike in level_designer.py."""

from __future__ import annotations

import json
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import level_designer as ld  # noqa: E402


def _analysis_fixture() -> dict:
    beats = [round(i * 0.5, 6) for i in range(48)]
    events = []
    for beat_idx in range(8, 40):
        events.append({
            "t": round(beats[beat_idx] + 0.04, 6),
            "flux": 1.0,
            "intensity": "high",
            "passes": ["shape"],
        })
    return {
        "song_id": "fixture_song",
        "title": "fixture_song",
        "bpm": 120,
        "duration": 30.0,
        "beats": beats,
        "events": events,
        "structure": [
            {"section": "verse", "start": 0.0, "end": 6.0},
            {"section": "drop", "start": 6.0, "end": 12.0},
            {"section": "verse", "start": 12.0, "end": 18.0},
            {"section": "bridge", "start": 18.0, "end": 24.0},
        ],
        "flux_stats": {"min": 0.0, "p25": 0.2, "p50": 0.4, "p75": 0.6, "p90": 0.8, "max": 1.0},
    }


class TestExperimentalOnsetTiming(unittest.TestCase):
    def test_defaults_keep_beat_snapped_time(self):
        beatmap = ld.build_beatmap(_analysis_fixture(), ["easy"], cleanup_enabled=True)
        beats = beatmap["difficulties"]["easy"]["beats"]
        self.assertGreater(len(beats), 0)
        for obs in beats:
            beat_idx = obs["beat"]
            self.assertAlmostEqual(obs["time_sec"], beatmap["beat_times"][beat_idx], places=6)
            self.assertNotIn("timing_source", obs)

    def test_experimental_mode_uses_onset_time_when_available(self):
        beatmap = ld.build_beatmap(
            _analysis_fixture(),
            ["easy"],
            cleanup_enabled=True,
            experimental_onset_timing=True,
        )
        beats = beatmap["difficulties"]["easy"]["beats"]
        onset_backed = [obs for obs in beats if obs.get("timing_source") == "onset"]
        self.assertGreater(len(onset_backed), 0)
        self.assertTrue(
            any(abs(obs["time_sec"] - obs["beat_time_sec"]) > 0.0 for obs in onset_backed),
            "expected at least one onset-backed obstacle to differ from beat time",
        )

    def test_experimental_diagnostics_emit_comparison_artifacts(self):
        repo_root = Path(__file__).resolve().parent.parent
        out_dir = repo_root / "tools" / ".test_onset_spike_diagnostics"
        if out_dir.exists():
            shutil.rmtree(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(out_dir, ignore_errors=True))

        ld.write_snap_diagnostics(
            _analysis_fixture(),
            out_dir,
            experimental_onset_timing=True,
        )

        summary_path = out_dir / "snap_diagnostics_summary.json"
        self.assertTrue(summary_path.exists())
        with summary_path.open() as handle:
            summary = json.load(handle)
        self.assertTrue(summary.get("experimental_onset_timing", {}).get("enabled"))
        self.assertIn("comparison_by_difficulty", summary["experimental_onset_timing"])
        self.assertTrue((out_dir / "onset_timing_events.csv").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
