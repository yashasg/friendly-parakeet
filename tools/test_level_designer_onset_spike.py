#!/usr/bin/env python3
"""Focused tests for segment-focus onset generation in level_designer.py.

Previous tests covered motif n-gram selection. This file is updated to cover
the segment-focused onset selection path that replaces it as the active
experimental generation route. Motif n-gram detection is kept for diagnostics
but no longer drives obstacle selection.
"""

from __future__ import annotations

import json
import shutil
import sys
import unittest
from unittest import mock
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


def _varied_fixture() -> dict:
    """Fixture with events across percussive/harmonic/full-spectrum classes."""
    beats = [round(i * 0.5, 6) for i in range(64)]
    events = []
    # Section 1: mostly percussive (kick/snare)
    for i in range(4, 16):
        passes = ["kick"] if i % 2 == 0 else ["snare"]
        events.append({
            "t": round(beats[i] + 0.01, 6),
            "flux": 0.8 + (i % 3) * 0.05,
            "intensity": "high",
            "passes": passes,
        })
    # Section 2: mostly harmonic (melody)
    for i in range(16, 28):
        events.append({
            "t": round(beats[i] + 0.01, 6),
            "flux": 0.6 + (i % 4) * 0.04,
            "intensity": "medium",
            "passes": ["melody"],
        })
    # Section 3: mixed / full-spectrum
    for i in range(28, 40):
        events.append({
            "t": round(beats[i] + 0.02, 6),
            "flux": 0.7 + (i % 2) * 0.1,
            "intensity": "high",
            "passes": ["kick", "melody"],
        })
    return {
        "song_id": "varied_fixture",
        "title": "varied_fixture",
        "bpm": 120,
        "duration": 40.0,
        "beats": beats,
        "events": events,
        "structure": [
            {"section": "verse", "start": 0.0, "end": 8.0},
            {"section": "drop", "start": 8.0, "end": 14.0},
            {"section": "bridge", "start": 14.0, "end": 20.0},
        ],
        "flux_stats": {"min": 0.5, "p25": 0.6, "p50": 0.7, "p75": 0.8, "p90": 0.9, "max": 1.0},
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
        self.assertEqual(len(onset_backed), len(beats))
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
        spike = summary.get("experimental_onset_timing", {})
        self.assertTrue(spike.get("enabled"))
        # Must now report segment_focus, not onset_motif_only.
        self.assertEqual(spike.get("generation_path"), "segment_focus")
        self.assertTrue(spike.get("legacy_rule_influence_disabled"))
        self.assertIn("comparison_by_difficulty", spike)
        self.assertTrue((out_dir / "onset_timing_events.csv").exists())
        hard = spike["comparison_by_difficulty"]["hard"]
        self.assertIn("motif_stats", hard)
        self.assertIn("event_role_distribution", hard)

    def test_experimental_mode_applies_class_lane_shape_mapping(self):
        """Segment-focus path applies ONSET_CLASS_TO_OBSTACLE mapping correctly."""
        beatmap = ld.build_beatmap(
            _varied_fixture(),
            ["hard"],
            cleanup_enabled=True,
            experimental_onset_timing=True,
        )
        beats = beatmap["difficulties"]["hard"]["beats"]
        mapped = [obs for obs in beats if obs.get("timing_source") == "onset"]
        self.assertGreater(len(mapped), 0)

        for obs in mapped:
            onset_class = obs.get("onset_class")
            if onset_class == "percussive":
                self.assertEqual(obs["lane"], 0)
                self.assertEqual(obs["shape"], "triangle")
            elif onset_class == "harmonic":
                self.assertEqual(obs["lane"], 2)
                self.assertEqual(obs["shape"], "circle")
            elif onset_class == "full-spectrum":
                self.assertEqual(obs["lane"], 1)
                self.assertEqual(obs["shape"], "square")

        # All obstacles must carry difficulty_inclusion (not motif_id).
        self.assertTrue(all("difficulty_inclusion" in obs for obs in mapped))
        # segment_focus must be set (no motif_id required).
        self.assertTrue(any(obs.get("segment_focus") for obs in mapped))

    # ─── New tests for segment-focus behaviour ────────────────────────────────

    def test_segment_focus_selector_returns_valid_structure(self):
        """select_segment_focus_beats returns the expected keys."""
        analysis = _varied_fixture()
        for diff in ("easy", "medium", "hard"):
            selected, all_snapped_map, diag = ld.select_segment_focus_beats(analysis, diff)
            self.assertIsInstance(selected, dict)
            self.assertIsInstance(all_snapped_map, dict)
            self.assertEqual(diag.get("generation_path"), "segment_focus")
            self.assertIn("segments", diag)
            self.assertGreater(len(diag["segments"]), 0)

    def test_segment_focus_difficulty_ordering(self):
        """Hard must include >= medium >= easy events (non-decreasing count)."""
        analysis = _varied_fixture()
        counts = {}
        for diff in ("easy", "medium", "hard"):
            selected, _, _ = ld.select_segment_focus_beats(analysis, diff)
            counts[diff] = len(selected)
        self.assertLessEqual(counts["easy"], counts["medium"])
        self.assertLessEqual(counts["medium"], counts["hard"])

    def test_segment_focus_deterministic(self):
        """Running the selector twice gives identical results."""
        analysis = _varied_fixture()
        sel_a, _, _ = ld.select_segment_focus_beats(analysis, "medium")
        sel_b, _, _ = ld.select_segment_focus_beats(analysis, "medium")
        self.assertEqual(sorted(sel_a.keys()), sorted(sel_b.keys()))

    def test_segment_focus_anti_repetition(self):
        """Focus class should not be identical across all segments in a varied fixture."""
        analysis = _varied_fixture()
        _, _, diag = ld.select_segment_focus_beats(analysis, "hard")
        focuses = [seg["focus_class"] for seg in diag["segments"] if seg.get("focus_events_total", 0) > 0]
        # With three structurally different sections, at least one focus class difference expected.
        # (Anti-repeat only fires after SEGMENT_FOCUS_ANTI_REPEAT_MAX repetitions, so this is
        # more of a smoke test that the field is populated correctly.)
        self.assertGreater(len(focuses), 0)
        self.assertTrue(all(f in ("percussive", "harmonic", "full-spectrum") for f in focuses))

    def test_segment_diagnostics_fields(self):
        """Each segment diagnostic entry has the required fields."""
        analysis = _varied_fixture()
        _, _, diag = ld.select_segment_focus_beats(analysis, "medium")
        required = {
            "segment_idx", "section", "start", "end",
            "focus_class", "fallback_reason",
            "class_counts", "class_flux",
            "focus_events_total", "difficulty_selected",
        }
        for seg in diag["segments"]:
            missing = required - seg.keys()
            self.assertEqual(missing, set(), f"segment {seg.get('segment_idx')} missing: {missing}")

    def test_choose_segment_focus_anti_repeat(self):
        """Anti-repeat halves score of a class repeated SEGMENT_FOCUS_ANTI_REPEAT_MAX times."""
        # All three classes present; full-spectrum would win on flux but has been repeated.
        class_stats = {
            "full-spectrum": {"count": 10, "total_flux": 10.0, "events": []},
            "percussive":    {"count": 5,  "total_flux": 5.0,  "events": []},
            "harmonic":      {"count": 3,  "total_flux": 3.0,  "events": []},
        }
        prev = ["full-spectrum"] * ld.SEGMENT_FOCUS_ANTI_REPEAT_MAX
        focus, reason = ld._choose_segment_focus(class_stats, prev)
        # After penalty, full-spectrum (5.0) vs percussive (5.0+0.5=5.5) → percussive wins.
        self.assertNotEqual(focus, "full-spectrum")
        self.assertIsNotNone(reason)

    def test_choose_segment_focus_no_penalty_for_single_class(self):
        """Anti-repeat does NOT fire when only one class exists."""
        class_stats = {
            "percussive": {"count": 8, "total_flux": 8.0, "events": []},
        }
        prev = ["percussive"] * ld.SEGMENT_FOCUS_ANTI_REPEAT_MAX
        focus, reason = ld._choose_segment_focus(class_stats, prev)
        self.assertEqual(focus, "percussive")

    # ─── Tuning / diagnostics invariant tests ────────────────────────────────

    def test_onset_pool_summary_present_in_diagnostics(self):
        """write_snap_diagnostics always emits onset_pool_summary (not gated on experimental)."""
        import shutil, json
        from pathlib import Path
        repo_root = Path(__file__).resolve().parent.parent
        out_dir = repo_root / "tools" / ".test_onset_pool_diag"
        if out_dir.exists():
            shutil.rmtree(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(out_dir, ignore_errors=True))

        # Default path (no experimental flag).
        ld.write_snap_diagnostics(_analysis_fixture(), out_dir, experimental_onset_timing=False)
        with (out_dir / "snap_diagnostics_summary.json").open() as f:
            summary = json.load(f)

        self.assertIn("onset_pool_summary", summary)
        pool = summary["onset_pool_summary"]
        required = {
            "total_events", "events_per_minute", "snapped_events",
            "snap_residual_stats", "segment_count",
            "empty_segment_count", "segment_coverage",
        }
        missing = required - pool.keys()
        self.assertEqual(missing, set(), f"onset_pool_summary missing fields: {missing}")
        self.assertGreater(pool["total_events"], 0)
        self.assertGreater(pool["events_per_minute"], 0.0)

    def test_onset_pool_summary_experimental_adds_obstacle_counts(self):
        """Experimental diagnostics include obstacle_counts_by_difficulty."""
        import shutil, json
        from pathlib import Path
        repo_root = Path(__file__).resolve().parent.parent
        out_dir = repo_root / "tools" / ".test_onset_pool_exp_diag"
        if out_dir.exists():
            shutil.rmtree(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(out_dir, ignore_errors=True))

        ld.write_snap_diagnostics(_analysis_fixture(), out_dir, experimental_onset_timing=True)
        with (out_dir / "snap_diagnostics_summary.json").open() as f:
            summary = json.load(f)

        spike = summary.get("experimental_onset_timing", {})
        self.assertIn("obstacle_counts_by_difficulty", spike)
        counts = spike["obstacle_counts_by_difficulty"]
        self.assertIn("easy", counts)
        self.assertIn("medium", counts)
        self.assertIn("hard", counts)
        # easy ≤ medium ≤ hard in the segment-focus path.
        self.assertLessEqual(counts["easy"], counts["medium"])
        self.assertLessEqual(counts["medium"], counts["hard"])

    def test_build_beatmap_counts_scale_with_difficulty(self):
        """build_beatmap obstacle counts: easy ≤ medium ≤ hard (segment-focus path)."""
        analysis = _varied_fixture()
        counts = {}
        for diff in ("easy", "medium", "hard"):
            bm = ld.build_beatmap(
                analysis, [diff],
                cleanup_enabled=True,
                experimental_onset_timing=True,
            )
            counts[diff] = bm["difficulties"][diff]["count"]
        self.assertLessEqual(
            counts["easy"], counts["medium"],
            f"easy ({counts['easy']}) > medium ({counts['medium']})",
        )
        self.assertLessEqual(
            counts["medium"], counts["hard"],
            f"medium ({counts['medium']}) > hard ({counts['hard']})",
        )

    def test_richer_onset_pool_in_varied_fixture(self):
        """Varied fixture (3 classes) should yield ≥1 event per segment in hard."""
        analysis = _varied_fixture()
        selected, _, diag = ld.select_segment_focus_beats(analysis, "hard")
        # All non-empty segments should contribute at least 1 event.
        for seg in diag["segments"]:
            if seg.get("focus_events_total", 0) > 0:
                self.assertGreater(
                    seg["difficulty_selected"], 0,
                    f"segment {seg['segment_idx']} has {seg['focus_events_total']} "
                    f"focus events but 0 selected (hard fraction should take ≥1)",
                )

    def test_segment_diagnostics_include_n_difficulty_fields(self):
        """Each non-empty segment should carry n_easy, n_medium, n_hard counts."""
        analysis = _varied_fixture()
        _, _, diag = ld.select_segment_focus_beats(analysis, "hard")
        for seg in diag["segments"]:
            if seg.get("focus_events_total", 0) == 0:
                continue
            for key in ("n_easy", "n_medium", "n_hard"):
                self.assertIn(key, seg, f"segment {seg['segment_idx']} missing {key}")
            self.assertLessEqual(seg["n_easy"], seg["n_medium"])
            self.assertLessEqual(seg["n_medium"], seg["n_hard"])

    def test_diagnostics_deterministic_across_runs(self):
        """Diagnostics summary must be identical across two independent runs."""
        import shutil, json
        from pathlib import Path
        repo_root = Path(__file__).resolve().parent.parent
        out_a = repo_root / "tools" / ".test_det_diag_a"
        out_b = repo_root / "tools" / ".test_det_diag_b"
        for d in (out_a, out_b):
            if d.exists():
                shutil.rmtree(d)
            d.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(out_a, ignore_errors=True))
        self.addCleanup(lambda: shutil.rmtree(out_b, ignore_errors=True))

        ld.write_snap_diagnostics(_analysis_fixture(), out_a, experimental_onset_timing=True)
        ld.write_snap_diagnostics(_analysis_fixture(), out_b, experimental_onset_timing=True)

        with (out_a / "snap_diagnostics_summary.json").open() as f:
            summary_a = json.load(f)
        with (out_b / "snap_diagnostics_summary.json").open() as f:
            summary_b = json.load(f)

        # Deep equality check via JSON round-trip.
        self.assertEqual(
            json.dumps(summary_a, sort_keys=True),
            json.dumps(summary_b, sort_keys=True),
            "diagnostics summary is not deterministic across runs",
        )


# ─────────────────────────────────────────────────────────────────────────────
# DIRECTIVE 2026-05-10 — enforcement tests
# ─────────────────────────────────────────────────────────────────────────────

def _cross_layer_fixture() -> dict:
    """Analysis fixture with explicitly-layered events.

    Pair A: percussive at t=1.000 and harmonic at t=1.040 (40ms apart, <50ms).
    Pair B: two percussive events at t=2.000 and t=2.030 (30ms, same layer).
    Single C: full-spectrum at t=3.000.
    """
    beats = [round(i * 0.5, 6) for i in range(20)]
    events = [
        # Pair A — different layers, close in time
        {"t": 1.000, "passes": ["kick"],   "layer": "percussive",    "flux": 0.8, "intensity": "high"},
        {"t": 1.040, "passes": ["melody"], "layer": "harmonic",      "flux": 0.7, "intensity": "medium"},
        # Pair B — same layer, close in time (should merge to 1)
        {"t": 2.000, "passes": ["kick"],   "layer": "percussive",    "flux": 0.6, "intensity": "high"},
        {"t": 2.030, "passes": ["snare"],  "layer": "percussive",    "flux": 0.5, "intensity": "high"},
        # Single C
        {"t": 3.000, "passes": ["flux"],   "layer": "full-spectrum", "flux": 0.4, "intensity": "medium"},
    ]
    return {
        "song_id": "cross_layer_fixture",
        "title":   "cross_layer_fixture",
        "bpm": 120,
        "duration": 12.0,
        "beats": beats,
        "events": events,
        "structure": [{"section": "verse", "start": 0.0, "end": 12.0}],
        "flux_stats": {"min": 0.0, "p25": 0.4, "p50": 0.6, "p75": 0.8, "p90": 0.9, "max": 1.0},
    }


class TestDirective20260510(unittest.TestCase):
    """Prove the four invariants required by the 2026-05-10 design directive."""

    # ── 1. Cross-layer events are NOT merged away ────────────────────────────

    def test_snap_cross_layer_events_preserved_per_beat(self):
        """Percussive + harmonic onsets on the same beat survive as two snapped events."""
        analysis = _cross_layer_fixture()
        beats = analysis["beats"]
        events = analysis["events"]

        snapped = ld.snap_events_to_beats(events, beats)

        # Pair A: t=1.000 and t=1.040 both snap to beat 2 (=1.0 s).
        # They have different onset_classes so BOTH must survive.
        beat_2_events = [ev for ev in snapped if ev["beat_idx"] == 2]
        classes_at_beat_2 = {ld.classify_onset_class(ev) for ev in beat_2_events}
        self.assertIn(
            "percussive", classes_at_beat_2,
            "percussive onset at t=1.000 was discarded at beat 2",
        )
        self.assertIn(
            "harmonic", classes_at_beat_2,
            "harmonic onset at t=1.040 was discarded at beat 2",
        )
        self.assertEqual(
            len(beat_2_events), 2,
            f"expected 2 cross-layer events at beat 2, got {len(beat_2_events)}",
        )

    def test_snap_same_layer_events_deduplicated_per_beat(self):
        """Same-class onsets on the same beat collapse to one (no duplicates)."""
        analysis = _cross_layer_fixture()
        beats = analysis["beats"]
        events = analysis["events"]

        snapped = ld.snap_events_to_beats(events, beats)

        # Pair B: t=2.000 and t=2.030 both snap to beat 4 (=2.0 s).
        # Both are percussive → only the first should survive.
        beat_4_events = [ev for ev in snapped if ev["beat_idx"] == 4]
        self.assertEqual(
            len(beat_4_events), 1,
            f"expected 1 same-class event at beat 4, got {len(beat_4_events)}",
        )
        self.assertEqual(ld.classify_onset_class(beat_4_events[0]), "percussive")

    def test_segment_class_stats_sees_both_layers(self):
        """_segment_class_stats receives both percussive and harmonic from the same beat."""
        analysis = _cross_layer_fixture()
        beats = analysis["beats"]
        events = analysis["events"]
        snapped = ld.snap_events_to_beats(events, beats)

        # Only the segment that covers beat 2 (t=1.0s) matters here.
        seg_events = [ev for ev in snapped if float(ev["beat_time"]) < 2.0]
        class_stats = ld._segment_class_stats(seg_events)

        self.assertIn("percussive", class_stats, "percussive missing from class_stats")
        self.assertIn("harmonic",   class_stats, "harmonic missing from class_stats")

    # ── 2. Onset-only path — no beat_fallback timing source ──────────────────

    def test_onset_only_path_no_beat_fallback(self):
        """All obstacles from design_level_segment_focus carry timing_source='onset'."""
        analysis = _varied_fixture()
        for diff in ("easy", "medium", "hard"):
            beatmap = ld.build_beatmap(
                analysis, [diff],
                cleanup_enabled=False,
                experimental_onset_timing=True,
            )
            obstacles = beatmap["difficulties"][diff]["beats"]
            bad = [
                obs for obs in obstacles
                if obs.get("timing_source") != "onset"
            ]
            self.assertEqual(
                bad, [],
                f"{diff}: obstacles with non-onset timing_source: {bad}",
            )

    def test_onset_only_path_timing_source_field_always_set(self):
        """Every obstacle in experimental mode has a timing_source field."""
        analysis = _varied_fixture()
        beatmap = ld.build_beatmap(
            _varied_fixture(), ["hard"],
            experimental_onset_timing=True,
        )
        for obs in beatmap["difficulties"]["hard"]["beats"]:
            self.assertIn(
                "timing_source", obs,
                f"obstacle missing timing_source field: {obs}",
            )

    # ── 3. Cleanup is not invoked for the onset-only path ────────────────────

    def test_cleanup_not_invoked_in_segment_focus(self):
        """design_level_segment_focus must not call any clean_* function.

        We verify this indirectly: the segment-focus path produces obstacles
        whose beat sequence matches the raw onset-selected beats exactly
        (no obstacles removed or shifted by cleanup).
        """
        analysis = _varied_fixture()
        # Get raw selection beats from selected event payloads.
        selected, _, _ = ld.select_segment_focus_beats(analysis, "hard")
        raw_beats = sorted(event["beat_idx"] for event in selected.values())

        # Get beatmap obstacles.
        obstacles, _, _ = ld.design_level_segment_focus(analysis, "hard")
        obs_beats = sorted(obs["beat"] for obs in obstacles)

        self.assertEqual(
            raw_beats, obs_beats,
            "cleanup altered the obstacle beat list — "
            "cleanup must not run on the onset-only path",
        )

    def test_cleanup_parameter_ignored_in_segment_focus(self):
        """Passing cleanup_enabled=True to design_level_segment_focus has no effect."""
        analysis = _varied_fixture()
        obs_false, _, _ = ld.design_level_segment_focus(analysis, "medium", cleanup_enabled=False)
        obs_true,  _, _ = ld.design_level_segment_focus(analysis, "medium", cleanup_enabled=True)

        beats_false = sorted(obs["beat"] for obs in obs_false)
        beats_true  = sorted(obs["beat"] for obs in obs_true)
        self.assertEqual(
            beats_false, beats_true,
            "cleanup_enabled=True changed the output — cleanup must be a no-op here",
        )

    # ── 4. Difficulty counts are deterministic and ordered ───────────────────

    def test_difficulty_counts_deterministic(self):
        """Two independent calls with the same analysis yield identical counts."""
        analysis = _varied_fixture()
        counts_a, counts_b = {}, {}
        for diff in ("easy", "medium", "hard"):
            bm_a = ld.build_beatmap(analysis, [diff], experimental_onset_timing=True)
            bm_b = ld.build_beatmap(analysis, [diff], experimental_onset_timing=True)
            counts_a[diff] = bm_a["difficulties"][diff]["count"]
            counts_b[diff] = bm_b["difficulties"][diff]["count"]
        self.assertEqual(counts_a, counts_b, "obstacle counts are not deterministic")

    def test_difficulty_counts_ordered(self):
        """easy ≤ medium ≤ hard obstacle counts (onset-only path)."""
        analysis = _varied_fixture()
        counts = {}
        for diff in ("easy", "medium", "hard"):
            bm = ld.build_beatmap(analysis, [diff], experimental_onset_timing=True)
            counts[diff] = bm["difficulties"][diff]["count"]
        self.assertLessEqual(
            counts["easy"], counts["medium"],
            f"easy ({counts['easy']}) > medium ({counts['medium']})",
        )
        self.assertLessEqual(
            counts["medium"], counts["hard"],
            f"medium ({counts['medium']}) > hard ({counts['hard']})",
        )

    def test_intro_rest_not_applied_in_segment_focus(self):
        """Onsets before DIFFICULTY_INTRO_REST beats are not filtered out."""
        # Build an analysis where events start at beat 0 (earlier than any intro_rest).
        beats = [round(i * 0.5, 6) for i in range(30)]
        early_events = [
            {"t": round(beats[1] + 0.01, 6), "passes": ["kick"], "layer": "percussive",
             "flux": 0.9, "intensity": "high"},
            {"t": round(beats[2] + 0.01, 6), "passes": ["melody"], "layer": "harmonic",
             "flux": 0.8, "intensity": "high"},
        ]
        analysis = {
            "song_id": "early_onset_fixture",
            "title": "early_onset_fixture",
            "bpm": 120,
            "duration": 20.0,
            "beats": beats,
            "events": early_events,
            "structure": [{"section": "verse", "start": 0.0, "end": 20.0}],
            "flux_stats": {"min": 0.0, "p25": 0.5, "p50": 0.7, "p75": 0.85, "p90": 0.9, "max": 1.0},
        }
        # Hard intro_rest = 2, so with the old filter beats 1 and 2 would be skipped.
        # With the fix they must be included.
        selected, _, _ = ld.select_segment_focus_beats(analysis, "easy")
        selected_beats = sorted(event["beat_idx"] for event in selected.values())
        # At least one of the early beats (1 or 2) should be present.
        self.assertTrue(
            any(b in selected_beats for b in (1, 2)),
            f"early onset beats were filtered out — intro_rest must not apply. "
            f"selected beats: {selected_beats}",
        )

    def test_segment_focus_preserves_cross_layer_same_beat_when_selected(self):
        analysis = _cross_layer_fixture()
        with mock.patch.object(ld, "_choose_segment_focus", return_value=("ghost", "forced_for_test")):
            selected, _, _ = ld.select_segment_focus_beats(analysis, "hard")

        beat_2_events = [event for event in selected.values() if event.get("beat_idx") == 2]
        classes = {event.get("onset_class") for event in beat_2_events}
        self.assertIn("percussive", classes)
        self.assertIn("harmonic", classes)
        self.assertGreaterEqual(len(beat_2_events), 2)

    def test_write_snap_diagnostics_clears_stale_onset_csv_when_non_experimental(self):
        repo_root = Path(__file__).resolve().parent.parent
        out_dir = repo_root / "tools" / ".test_onset_stale_cleanup"
        if out_dir.exists():
            shutil.rmtree(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(out_dir, ignore_errors=True))

        ld.write_snap_diagnostics(_analysis_fixture(), out_dir, experimental_onset_timing=True)
        self.assertTrue((out_dir / "onset_timing_events.csv").exists())

        ld.write_snap_diagnostics(_analysis_fixture(), out_dir, experimental_onset_timing=False)
        self.assertFalse((out_dir / "onset_timing_events.csv").exists())

    def test_classify_onset_class_prefers_layer_field(self):
        """classify_onset_class uses the 'layer' field when present (not passes)."""
        # An event that has passes suggesting 'full-spectrum' but layer='percussive'.
        event = {
            "passes": ["kick", "melody", "flux"],
            "layer": "percussive",
        }
        self.assertEqual(ld.classify_onset_class(event), "percussive")

    def test_classify_onset_class_fallback_to_passes(self):
        """classify_onset_class falls back to passes when layer field is absent."""
        event_kick   = {"passes": ["kick"],   "flux": 1.0}
        event_melody = {"passes": ["melody"], "flux": 1.0}
        event_both   = {"passes": ["kick", "melody"], "flux": 1.0}
        event_flux   = {"passes": ["flux"],   "flux": 1.0}

        self.assertEqual(ld.classify_onset_class(event_kick),   "percussive")
        self.assertEqual(ld.classify_onset_class(event_melody), "harmonic")
        self.assertEqual(ld.classify_onset_class(event_both),   "full-spectrum")
        self.assertEqual(ld.classify_onset_class(event_flux),   "full-spectrum")


if __name__ == "__main__":
    unittest.main(verbosity=2)
