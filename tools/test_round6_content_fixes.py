#!/usr/bin/env python3
"""Round 6 content fixes — coverage for issues #505, #506, #507.

Targeted unit tests for:

* Issue #505 — ``build_beatmap`` emits ``offset`` anchored to the first
  authored beat (matches ``validate_beatmap_offset.py``), not to raw
  ``beats[0]``.
* Issue #506 — ``_fill_silent_gaps`` promotes real onsets from the
  fallback pool to bound mid-song silent stretches under the per-difficulty
  ``MAX_SILENT_GAP_BEATS`` cap, preserves ``segment_focus`` /
  ``segment_idx`` metadata, and never inserts a synthetic event.
* Issue #507 — ``_is_protected_cross_layer_pair`` no longer requires
  ``beat_idx`` equality, and ``_clears_min_ioi`` accepts the second of a
  cross-layer pair within 50 ms even when the snapper places them on
  opposite sides of a beat boundary.
"""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import level_designer as ld  # noqa: E402


# ─────────────────────────────────────────────────────────────────────
# Issue #507 — cross-layer protection no longer couples to beat_idx.
# ─────────────────────────────────────────────────────────────────────


class TestCrossLayerBeatBoundaryProtection(unittest.TestCase):
    """Issue #507."""

    def test_cross_layer_pair_protected_across_beat_boundary(self):
        # Demonstration from the issue body: 30 ms apart, different broad
        # layers, snapped onto opposite sides of a beat boundary.
        left = {"t": 1.000, "onset_class": "percussive", "beat_idx": 5}
        right = {"t": 1.030, "onset_class": "harmonic", "beat_idx": 6}
        self.assertTrue(ld._is_protected_cross_layer_pair(left, right))
        self.assertTrue(ld._is_protected_cross_layer_pair(right, left))

    def test_cross_layer_pair_protected_when_sharing_beat(self):
        left = {"t": 1.000, "onset_class": "percussive", "beat_idx": 5}
        right = {"t": 1.030, "onset_class": "harmonic", "beat_idx": 5}
        self.assertTrue(ld._is_protected_cross_layer_pair(left, right))

    def test_same_layer_pair_not_protected(self):
        left = {"t": 1.000, "onset_class": "percussive", "beat_idx": 5}
        right = {"t": 1.030, "onset_class": "percussive", "beat_idx": 6}
        self.assertFalse(ld._is_protected_cross_layer_pair(left, right))

    def test_cross_layer_pair_outside_window_not_protected(self):
        left = {"t": 1.000, "onset_class": "percussive", "beat_idx": 5}
        right = {"t": 1.080, "onset_class": "harmonic", "beat_idx": 6}
        self.assertFalse(ld._is_protected_cross_layer_pair(left, right))

    def test_clears_min_ioi_accepts_cross_layer_across_beat_boundary(self):
        # Across all three difficulties — 30 ms cross-layer pair must
        # survive the IOI floor regardless of beat_idx.
        left = {"t": 1.000, "onset_class": "percussive", "beat_idx": 5}
        right = {"t": 1.030, "onset_class": "harmonic", "beat_idx": 6}
        for diff in ("easy", "medium", "hard"):
            with self.subTest(difficulty=diff):
                self.assertTrue(
                    ld._clears_min_ioi(right, [left], diff),
                    f"{diff}: cross-layer event 30 ms after a percussive "
                    "anchor must not be dropped by the IOI floor",
                )

    def test_clears_min_ioi_still_drops_same_layer_within_floor(self):
        # Same-layer events within the floor must still be dropped.
        left = {"t": 1.000, "onset_class": "percussive", "beat_idx": 5}
        right = {"t": 1.030, "onset_class": "percussive", "beat_idx": 6}
        self.assertFalse(ld._clears_min_ioi(right, [left], "hard"))


# ─────────────────────────────────────────────────────────────────────
# Issue #505 — offset anchored to first authored beat.
# ─────────────────────────────────────────────────────────────────────


class TestOffsetAnchoring(unittest.TestCase):
    """Issue #505."""

    def _analysis(self) -> dict:
        # Beats spaced 0.5 s (BPM=120) but with a small positive drift
        # on beats[0] so beats[0] != back-projected anchor.  Authored
        # obstacles start at beat_idx=4.
        beats = [round(i * 0.5 + 0.030, 6) for i in range(64)]
        beats[0] = 0.080  # +50 ms drift on the first detected beat
        events = []
        for beat_idx in range(4, 60, 2):
            events.append({
                "t": round(beats[beat_idx] + 0.005, 6),
                "flux": 1.0,
                "intensity": "high",
                "passes": ["shape"],
                "layer": "harmonic",
            })
        return {
            "song_id": "anchor_fixture",
            "title": "anchor_fixture",
            "bpm": 120.0,
            "duration": 35.0,
            "beats": beats,
            "events": events,
            "structure": [{"section": "verse", "start": 0.0, "end": 35.0}],
            "flux_stats": {"min": 0.0, "p25": 0.2, "p50": 0.4, "p75": 0.6, "p90": 0.8, "max": 1.0},
        }

    def test_offset_back_projects_anchor_idx_not_beats0(self):
        analysis = self._analysis()
        bm = ld.build_beatmap(
            analysis,
            ["easy", "medium", "hard"],
            cleanup_enabled=False,
            experimental_onset_timing=True,
        )
        beat_period = 60.0 / float(analysis["bpm"])
        authored = [
            o["beat"]
            for d in bm["difficulties"].values()
            for o in d["beats"]
        ]
        self.assertTrue(authored, "fixture must produce some authored beats")
        anchor_idx = min(authored)
        expected = analysis["beats"][anchor_idx] - anchor_idx * beat_period
        self.assertAlmostEqual(bm["offset"], expected, places=3)
        # Sanity: offset must NOT be the raw beats[0] like the old code.
        self.assertNotAlmostEqual(bm["offset"], analysis["beats"][0], places=3)


# ─────────────────────────────────────────────────────────────────────
# Issue #506 — _fill_silent_gaps bounds mid-song voids using real onsets.
# ─────────────────────────────────────────────────────────────────────


class TestFillSilentGaps(unittest.TestCase):
    """Issue #506."""

    def _build_pool_and_selected(self, gap_sec: float, candidate_times):
        # Two anchor events surrounding the gap.
        left = {
            "t": 1.000, "beat_idx": 2, "subdivision": "downbeat",
            "source_event_idx": 0, "onset_class": "harmonic",
            "segment_focus": "harmonic", "segment_idx": 0,
            "flux": 0.9,
        }
        right = {
            "t": 1.000 + gap_sec, "beat_idx": 100, "subdivision": "downbeat",
            "source_event_idx": 1, "onset_class": "percussive",
            "segment_focus": "percussive", "segment_idx": 0,
            "flux": 0.9,
        }
        selected = {
            ld._event_key(left): left,
            ld._event_key(right): right,
        }
        # Real onset candidates spread across the gap; layer is set
        # (mimics rhythm_pipeline.merge_events output) but onset_class
        # is intentionally omitted so the helper's auto-classify path
        # is exercised.
        pool = []
        for i, t in enumerate(candidate_times):
            pool.append({
                "t": t,
                "beat_idx": 10 + i,
                "subdivision": "downbeat",
                "source_event_idx": 100 + i,
                "layer": "harmonic" if i % 2 == 0 else "percussive",
                "flux": 0.5,
                "beat_time": t,
            })
        # Include anchors in the fallback pool too (they must be
        # silently ignored because they're already selected).
        pool.extend([left, right])
        return selected, pool

    def test_fills_gap_with_real_onsets_only_under_cap(self):
        bpm = 120.0  # cap_sec_easy = 40 * 60 / 120 = 20 s
        gap_sec = 30.0
        candidate_times = [
            1.000 + 5.0 * (i + 1) for i in range(5)
        ]  # 6.0, 11.0, 16.0, 21.0, 26.0
        sel, pool = self._build_pool_and_selected(gap_sec, candidate_times)
        seg_ranges = [(0.0, 100.0, 0, "harmonic")]
        out = ld._fill_silent_gaps(
            sel, pool, "easy", bpm, segment_ranges=seg_ranges,
        )
        self.assertGreater(len(out), len(sel),
                           "gap-fill must add at least one real onset")
        # Resulting max gap must be <= cap_sec.
        ts = sorted(float(ev["t"]) for ev in out.values())
        max_gap = max(ts[i + 1] - ts[i] for i in range(len(ts) - 1))
        cap_sec = ld.MAX_SILENT_GAP_BEATS["easy"] * 60.0 / bpm
        self.assertLessEqual(max_gap, cap_sec + 1e-6)

    def test_no_filler_when_pool_has_no_candidates_in_gap(self):
        bpm = 120.0
        gap_sec = 30.0
        sel, pool = self._build_pool_and_selected(gap_sec, [])  # empty pool
        out = ld._fill_silent_gaps(sel, pool, "easy", bpm, segment_ranges=None)
        # No real onsets exist mid-gap → output unchanged (no synthetic event).
        self.assertEqual(set(out.keys()), set(sel.keys()))

    def test_promoted_event_inherits_segment_metadata(self):
        bpm = 120.0
        gap_sec = 30.0
        sel, pool = self._build_pool_and_selected(gap_sec, [16.0])
        seg_ranges = [
            (0.0, 10.0, 0, "harmonic"),
            (10.0, 100.0, 1, "percussive"),
        ]
        out = ld._fill_silent_gaps(
            sel, pool, "easy", bpm, segment_ranges=seg_ranges,
        )
        # Find newly promoted event (the candidate at t≈16).
        promoted = [
            ev for ev in out.values()
            if abs(float(ev["t"]) - 16.0) < 1e-6
        ]
        self.assertEqual(len(promoted), 1)
        ev = promoted[0]
        self.assertEqual(ev.get("segment_idx"), 1)
        self.assertEqual(ev.get("segment_focus"), "percussive")
        self.assertEqual(ev.get("difficulty_inclusion"), "easy")
        # Broad-layer naming preserved (no raw instrument pass leak).
        self.assertIn(ev.get("onset_class"),
                      ("percussive", "harmonic", "full-spectrum"))

    def test_max_silent_gap_beats_matches_validator(self):
        # Issue #506 — the generator constant must mirror the validator.
        from validate_max_beat_gap import MAX_GAP as VALIDATOR_MAX_GAP
        self.assertEqual(ld.MAX_SILENT_GAP_BEATS, VALIDATOR_MAX_GAP)


# ─────────────────────────────────────────────────────────────────────
# End-to-end shipped invariants — issues #505, #506.
# ─────────────────────────────────────────────────────────────────────


class TestShippedBeatmapsRound6Invariants(unittest.TestCase):
    """End-to-end guarantees the validators encode."""

    @classmethod
    def setUpClass(cls):
        cls.beatmap_dir = Path(__file__).resolve().parent.parent / "content" / "beatmaps"
        cls.beatmaps = list(cls.beatmap_dir.glob("*_beatmap.json"))

    def test_shipped_offsets_are_anchor_back_projected(self):
        # Issue #505 — same formula as validate_beatmap_offset.py.
        for path in self.beatmaps:
            with self.subTest(beatmap=path.name):
                bm = json.loads(path.read_text())
                stem = path.stem.replace("_beatmap", "")
                analysis_path = path.parent / f"{stem}_analysis.json"
                if not analysis_path.exists():
                    self.skipTest("analysis file missing")
                an = json.loads(analysis_path.read_text())
                bpm = float(bm["bpm"])
                period = 60.0 / bpm
                authored = [
                    o["beat"]
                    for d in bm["difficulties"].values()
                    for o in d["beats"]
                ]
                anchor_idx = min(authored)
                expected = an["beats"][anchor_idx] - anchor_idx * period
                self.assertAlmostEqual(bm["offset"], expected, places=3)

    def test_shipped_silent_gap_bounded_per_difficulty(self):
        # Issue #506 — every shipped tier-file must respect the
        # per-difficulty silent-gap cap (in seconds) under onset timing.
        for path in self.beatmaps:
            bm = json.loads(path.read_text())
            bpm = float(bm["bpm"])
            for diff, cap_beats in ld.MAX_SILENT_GAP_BEATS.items():
                cap_sec = cap_beats * 60.0 / bpm
                ts = sorted(
                    float(o["time_sec"]) for o in bm["difficulties"][diff]["beats"]
                )
                if len(ts) < 2:
                    continue
                max_gap = max(ts[i + 1] - ts[i] for i in range(len(ts) - 1))
                with self.subTest(beatmap=path.name, difficulty=diff):
                    self.assertLessEqual(
                        max_gap, cap_sec + 1e-6,
                        f"{path.name} {diff}: max gap {max_gap:.2f}s "
                        f"exceeds {cap_sec:.2f}s cap",
                    )


if __name__ == "__main__":
    unittest.main()
