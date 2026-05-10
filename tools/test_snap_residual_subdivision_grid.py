#!/usr/bin/env python3
"""Issue #471 — ``snap_residual_stats`` must report event residuals against
the subdivision grid the event was labelled with (downbeat / eighth /
triplet), not against the bare beat anchor.  Previously eighth-/triplet-
labelled events accumulated ~half-beat / ~third-beat residuals from the
downbeat, which made p90 and within_Xms numbers structurally meaningless
for off-beat subdivisions.
"""

from __future__ import annotations

import json
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import level_designer as ld  # noqa: E402


def _fixture_with_eighth_events() -> dict:
    """Beats 0.5 s apart; events placed exactly on eighth-note grid points
    (50 % between adjacent beats).  True residual against the eighth grid
    is ~0 ms; residual against the surrounding downbeat is 250 ms."""
    beats = [round(i * 0.5, 6) for i in range(40)]
    events = []
    for beat_idx in range(2, 30):
        # Place the event on the eighth between beat_idx and beat_idx+1.
        events.append({
            "t": round(beats[beat_idx] + 0.25, 6),
            "flux": 0.9,
            "intensity": "high",
            "passes": ["full_spectrum_flux"],
            "layer": "full-spectrum",
        })
    return {
        "song_id": "eighth_grid_fixture",
        "title": "eighth_grid_fixture",
        "bpm": 120,
        "duration": 22.0,
        "beats": beats,
        "events": events,
        "structure": [{"section": "verse", "start": 0.0, "end": 22.0}],
        "flux_stats": {"min": 0.0, "p25": 0.5, "p50": 0.7, "p75": 0.85, "p90": 0.9, "max": 1.0},
    }


class TestSnapResidualUsesSubdivisionGrid(unittest.TestCase):
    def setUp(self):
        repo_root = Path(__file__).resolve().parent.parent
        self.out_dir = repo_root / "tools" / ".test_snap_residual_subdiv"
        if self.out_dir.exists():
            shutil.rmtree(self.out_dir)
        self.out_dir.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.out_dir, ignore_errors=True))

    def _write_and_load(self, analysis):
        ld.write_snap_diagnostics(analysis, self.out_dir, experimental_onset_timing=False)
        with (self.out_dir / "snap_diagnostics_summary.json").open() as f:
            return json.load(f)

    def test_eighth_events_have_near_zero_subdivision_residual(self):
        """Events placed exactly on the eighth grid should report
        ``snap_residual_stats.median_abs_ms`` near 0 (subdivision-grid
        semantics).  Pre-#471 this would have been ~250 ms because the
        residual was measured from the downbeat anchor."""
        summary = self._write_and_load(_fixture_with_eighth_events())
        stats = summary["onset_pool_summary"]["snap_residual_stats"]
        self.assertEqual(stats["count"], 28)
        # Subdivision-grid residual: events are exactly on the eighth, so
        # the median absolute residual must be small (< 5 ms).
        self.assertLess(
            stats["median_abs_ms"], 5.0,
            f"snap_residual_stats reports {stats['median_abs_ms']} ms median; "
            "expected near-zero against the eighth subdivision grid",
        )
        self.assertLess(stats["p90_abs_ms"], 5.0)
        # Honesty check: every event lands within the 20 ms band.
        self.assertEqual(stats["within_20ms"], 28)

    def test_csv_grid_time_matches_subdivision_not_beat_anchor(self):
        """The per-event CSV must record ``grid_time`` at the subdivision
        location (eighth at +0.25 s after each beat), not the beat
        downbeat itself."""
        ld.write_snap_diagnostics(_fixture_with_eighth_events(), self.out_dir,
                                  experimental_onset_timing=False)
        import csv
        rows = []
        with (self.out_dir / "snap_candidate_events.csv").open() as f:
            for row in csv.DictReader(f):
                if row["candidate"] == "current_quarter_snap":
                    rows.append(row)
        self.assertGreater(len(rows), 0)
        for row in rows:
            event_t = float(row["event_time"])
            grid_t = float(row["grid_time"])
            # event_t = beat + 0.25; grid_t (subdivision) should also sit
            # at beat + 0.25, NOT at the bare beat.  Difference should
            # therefore be ~0, not ~0.25.
            self.assertLess(
                abs(event_t - grid_t), 0.005,
                f"row event_time={event_t} grid_time={grid_t} — grid_time "
                "must be the subdivision point, not the downbeat anchor",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
