#!/usr/bin/env python3
"""Unit tests for layer-aware merge_events in rhythm_pipeline.py.

Proves that:
  - Events from different broad layer classes are never merged even when
    their timestamps are within the 50 ms merge window.
  - Events from the same layer class ARE merged when within the window.
  - Every merged event carries the 'layer' field.
  - The PASS_TO_LAYER mapping covers all named passes.
  - _event_layer falls back to 'full-spectrum' for unknown pass names.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import rhythm_pipeline as rp  # noqa: E402


def _make_event(pass_name: str, t: float, flux: float = 1.0) -> dict:
    return {"pass": pass_name, "t": t, "flux": flux, "method": "test"}


class TestEventLayer(unittest.TestCase):
    def test_all_named_passes_have_a_layer(self):
        for p in rp.ONSET_PASSES:
            name = p["name"]
            layer = rp.PASS_TO_LAYER.get(name)
            self.assertIsNotNone(layer, f"PASS_TO_LAYER missing entry for '{name}'")
            self.assertIn(
                layer, ("percussive", "harmonic", "full-spectrum"),
                f"invalid layer '{layer}' for pass '{name}'",
            )

    def test_event_layer_returns_percussive_for_kick(self):
        self.assertEqual(rp._event_layer({"pass": "kick"}), "percussive")

    def test_event_layer_returns_percussive_for_snare(self):
        self.assertEqual(rp._event_layer({"pass": "snare"}), "percussive")

    def test_event_layer_returns_percussive_for_hihat(self):
        self.assertEqual(rp._event_layer({"pass": "hihat"}), "percussive")

    def test_event_layer_returns_harmonic_for_melody(self):
        self.assertEqual(rp._event_layer({"pass": "melody"}), "harmonic")

    def test_event_layer_returns_full_spectrum_for_flux(self):
        self.assertEqual(rp._event_layer({"pass": "flux"}), "full-spectrum")

    def test_event_layer_falls_back_for_unknown_pass(self):
        self.assertEqual(rp._event_layer({"pass": "unknown_instrument"}), "full-spectrum")

    def test_event_layer_handles_missing_pass_key(self):
        self.assertEqual(rp._event_layer({}), "full-spectrum")


class TestMergeEventsCrossLayerPreservation(unittest.TestCase):
    """Cross-layer events within 50 ms MUST remain as separate events."""

    def test_percussive_and_harmonic_within_50ms_not_merged(self):
        events = [
            _make_event("kick",   t=1.000, flux=0.8),
            _make_event("melody", t=1.040, flux=0.7),  # 40 ms later, different layer
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(len(merged), 2, "cross-layer events were incorrectly merged")
        layers = {e["layer"] for e in merged}
        self.assertIn("percussive", layers)
        self.assertIn("harmonic", layers)

    def test_percussive_and_full_spectrum_within_50ms_not_merged(self):
        events = [
            _make_event("kick", t=2.000, flux=0.9),
            _make_event("flux", t=2.030, flux=0.6),  # 30 ms later, different layer
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(len(merged), 2, "percussive+full-spectrum within 50ms were merged")
        layers = {e["layer"] for e in merged}
        self.assertIn("percussive",    layers)
        self.assertIn("full-spectrum", layers)

    def test_harmonic_and_full_spectrum_within_50ms_not_merged(self):
        events = [
            _make_event("melody", t=0.500, flux=0.5),
            _make_event("flux",   t=0.530, flux=0.4),  # 30 ms later, different layer
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(len(merged), 2, "harmonic+full-spectrum within 50ms were merged")

    def test_all_three_layers_within_50ms_produce_three_events(self):
        events = [
            _make_event("kick",   t=1.000, flux=0.9),
            _make_event("melody", t=1.020, flux=0.8),
            _make_event("flux",   t=1.040, flux=0.7),
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(len(merged), 3, "three cross-layer events should remain three")
        layers = {e["layer"] for e in merged}
        self.assertEqual(layers, {"percussive", "harmonic", "full-spectrum"})


class TestMergeEventsSameLayerCollapse(unittest.TestCase):
    """Same-layer events within 50 ms SHOULD be merged into one."""

    def test_two_percussive_within_50ms_collapse_to_one(self):
        events = [
            _make_event("kick",  t=1.000, flux=0.8),
            _make_event("snare", t=1.030, flux=0.6),  # 30 ms later, same layer
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(len(merged), 1, "same-layer percussive events should merge")
        self.assertEqual(merged[0]["layer"], "percussive")
        # Flux should be the max of the group.
        self.assertAlmostEqual(merged[0]["flux"], 0.8, places=4)
        # Passes should list both contributors.
        self.assertIn("kick",  merged[0]["passes"])
        self.assertIn("snare", merged[0]["passes"])

    def test_two_harmonic_within_50ms_collapse_to_one(self):
        events = [
            _make_event("melody", t=2.000, flux=0.7),
            _make_event("melody", t=2.040, flux=0.5),  # 40 ms later, same pass
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(len(merged), 1, "same-layer harmonic events should merge")
        self.assertEqual(merged[0]["layer"], "harmonic")

    def test_same_layer_beyond_window_stays_separate(self):
        events = [
            _make_event("kick", t=1.000, flux=0.8),
            _make_event("kick", t=1.060, flux=0.7),  # 60 ms — outside 50 ms window
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(len(merged), 2, "same-layer events outside window must not merge")

    def test_same_layer_adjacency_floor_after_anchor_window(self):
        """Issue #467 — anchor-window grouping uses the *mean* of each
        group as the rep timestamp, so consecutive groups can still end
        up < ``merge_window`` apart on the rep stream.  The merger must
        enforce a same-layer adjacency floor so no two same-layer reps
        ship within 50 ms of each other.

        Trio at [0, 49 ms, 70 ms]:
          * Pure anchor-window: groups [0, 49] -> rep 24.5 ms; [70] ->
            rep 70 ms.  Distance is 45.5 ms (< 50 ms).  REGRESSION.
          * Fixed: second pass collapses 24.5 ms and 70 ms into one rep.
        """
        events = [
            _make_event("kick",  t=0.000, flux=0.6),
            _make_event("snare", t=0.049, flux=0.7),  # within 50 ms of 0.000
            _make_event("kick",  t=0.070, flux=0.8),  # 21 ms past anchor; mean(0,49)=24.5
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        self.assertEqual(
            len(merged), 1,
            f"all three percussive events must collapse into one; got {[e['t'] for e in merged]}",
        )
        self.assertEqual(merged[0]["layer"], "percussive")
        self.assertAlmostEqual(merged[0]["flux"], 0.8, places=4)
        # Passes from every contributor are preserved in the merged rep.
        self.assertIn("kick",  merged[0]["passes"])
        self.assertIn("snare", merged[0]["passes"])

    def test_same_layer_adjacency_floor_preserves_cross_layer(self):
        """The adjacency-floor sweep is per-layer — cross-layer events
        within 50 ms must remain distinct (directive 2026-05-10)."""
        events = [
            _make_event("kick",   t=0.000, flux=0.6),  # percussive
            _make_event("snare",  t=0.049, flux=0.7),  # percussive (collapses with kick)
            _make_event("kick",   t=0.070, flux=0.8),  # percussive (collapses via floor)
            _make_event("melody", t=0.030, flux=0.5),  # harmonic — must survive
            _make_event("flux",   t=0.040, flux=0.4),  # full-spectrum — must survive
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        layers = sorted(e["layer"] for e in merged)
        self.assertEqual(
            layers, ["full-spectrum", "harmonic", "percussive"],
            "cross-layer events within 50 ms must NOT be collapsed by the same-layer floor",
        )

    def test_no_same_layer_reps_within_merge_window(self):
        """Stress: random pre-merge events must always yield same-layer
        reps that are >= ``merge_window`` apart in the output."""
        # Hand-crafted dense percussive cluster spanning ~200 ms.
        events = []
        for i, t in enumerate([0.000, 0.030, 0.055, 0.080, 0.110, 0.135, 0.180]):
            events.append(_make_event("kick", t=t, flux=0.5 + i * 0.05))
        merged = rp.merge_events(events, merge_window=0.050)
        perc = sorted(e["t"] for e in merged if e["layer"] == "percussive")
        gaps = [perc[i + 1] - perc[i] for i in range(len(perc) - 1)]
        for g in gaps:
            self.assertGreaterEqual(
                g, 0.050 - 1e-9,
                f"adjacent same-layer reps {perc} have gap {g*1000:.1f} ms < 50 ms",
            )


class TestMergeEventsLayerField(unittest.TestCase):
    """Every merged event must carry the 'layer' field."""

    def test_single_event_has_layer(self):
        events = [_make_event("kick", t=1.0)]
        merged = rp.merge_events(events)
        self.assertIn("layer", merged[0])
        self.assertEqual(merged[0]["layer"], "percussive")

    def test_merged_group_has_layer(self):
        events = [
            _make_event("kick",  t=1.000),
            _make_event("snare", t=1.020),
        ]
        merged = rp.merge_events(events)
        self.assertIn("layer", merged[0])

    def test_empty_input_returns_empty(self):
        self.assertEqual(rp.merge_events([]), [])

    def test_output_sorted_by_time(self):
        events = [
            _make_event("melody", t=2.0),
            _make_event("kick",   t=1.0),
            _make_event("flux",   t=1.5),
        ]
        merged = rp.merge_events(events)
        times = [e["t"] for e in merged]
        self.assertEqual(times, sorted(times))


class TestMergeEventsMixedScenario(unittest.TestCase):
    """Realistic multi-event scenario: some merge, some stay separate."""

    def test_realistic_cluster(self):
        """
        At t≈1.0 we have:
          kick  at 1.000 (percussive)
          snare at 1.025 (percussive, same layer → merge with kick)
          melody at 1.040 (harmonic, different layer → stays separate)
          flux  at 1.045 (full-spectrum, different layer → stays separate)

        Expected result: 3 events (1 percussive, 1 harmonic, 1 full-spectrum).
        """
        events = [
            _make_event("kick",   t=1.000, flux=0.9),
            _make_event("snare",  t=1.025, flux=0.7),
            _make_event("melody", t=1.040, flux=0.6),
            _make_event("flux",   t=1.045, flux=0.4),
        ]
        merged = rp.merge_events(events, merge_window=0.050)
        layers = sorted(e["layer"] for e in merged)
        self.assertEqual(
            layers,
            ["full-spectrum", "harmonic", "percussive"],
            f"expected 3 distinct layers, got {layers}",
        )


# ---------------------------------------------------------------------------
# #415 — quiet-region smoothing & trailing-quiet classification
# ---------------------------------------------------------------------------


class TestGetQuietRegionsSmoothing(unittest.TestCase):
    """``get_quiet_regions`` must emit canonical alternating QUIET/NOISY transitions."""

    def _quiet_regions(self, mask: list[bool], sr: int = 1000, hop: int = 100,
                       min_duration_s: float = 0.5):
        import numpy as np
        # Build an RMS-like signal from a quiet/noisy mask: quiet-mask True
        # frames produce a low value, False frames a high value.  A length
        # of ``hop`` samples per frame gives reasonable times.
        frames = np.array([0.0 if q else 1.0 for q in mask])
        # Synthesize a waveform whose framed RMS reproduces this mask.
        y = np.zeros(len(mask) * hop, dtype=np.float32)
        for i, val in enumerate(frames):
            if val > 0.5:
                y[i * hop:(i + 1) * hop] = 0.5  # noisy
            else:
                y[i * hop:(i + 1) * hop] = 0.0  # quiet
        cfg = {
            "quiet": {
                "frame_length": hop,
                "hop_length": hop,
                "quantile": 0.5,
                "min_duration_s": min_duration_s,
            }
        }
        return rp.get_quiet_regions(y, sr=sr, config=cfg)

    def test_no_adjacent_duplicate_transitions(self):
        # Pattern: long-quiet, short-noisy (absorbed), long-quiet, short-noisy
        # (absorbed), long-noisy.  Without coalescing, the previous code emitted
        # two QUIET markers in a row.
        mask = (
            [True] * 8 +    # long quiet
            [False] * 1 +   # short noisy → absorbed
            [True] * 8 +    # long quiet (would duplicate previous QUIET)
            [False] * 8     # long noisy
        )
        regions = self._quiet_regions(mask, min_duration_s=0.3)
        types = [r["type"] for r in regions]
        # No two adjacent markers may share a type.
        for i in range(1, len(types)):
            self.assertNotEqual(
                types[i - 1], types[i],
                f"adjacent duplicate {types[i]} transitions at index {i}: {types}",
            )

    def test_trailing_quiet_marker_is_emitted(self):
        mask = [False] * 10 + [True] * 10  # noisy → quiet, never closes
        regions = self._quiet_regions(mask, min_duration_s=0.3)
        self.assertGreaterEqual(len(regions), 2)
        self.assertEqual(regions[-1]["type"], "QUIET")


class TestClassifyIntensityTrailingQuiet(unittest.TestCase):
    """Final QUIET marker must classify subsequent events as low intensity."""

    def test_unclosed_trailing_quiet_classifies_event_as_low(self):
        events = [{"t": 9.0, "flux": 10.0}]
        regions = [{"type": "QUIET", "t": 8.0}]
        out = rp.classify_intensity(events, regions)
        self.assertEqual(out[0]["intensity"], "low")

    def test_unclosed_trailing_quiet_with_explicit_duration(self):
        events = [{"t": 12.0, "flux": 5.0}]
        regions = [{"type": "QUIET", "t": 8.0}]
        out = rp.classify_intensity(events, regions, duration=15.0)
        self.assertEqual(out[0]["intensity"], "low")

    def test_closed_quiet_window_still_works(self):
        events = [
            {"t": 1.0, "flux": 10.0},   # in quiet window → low
            {"t": 5.0, "flux": 10.0},   # outside window
        ]
        regions = [
            {"type": "QUIET", "t": 0.5},
            {"type": "NOISY", "t": 2.0},
        ]
        out = rp.classify_intensity(events, regions, duration=10.0)
        self.assertEqual(out[0]["intensity"], "low")
        self.assertNotEqual(out[1]["intensity"], "low")


# ---------------------------------------------------------------------------
# #419 — public analysis JSON must use broad layer / non-instrumental subpass IDs
# ---------------------------------------------------------------------------


class TestNoRawInstrumentSemanticsInPublicFields(unittest.TestCase):
    """Newly generated analysis must not expose kick/snare/hihat/melody."""

    _RAW_NAMES = {"kick", "snare", "hihat", "melody"}

    def test_onset_passes_use_non_instrumental_ids(self):
        names = {p["name"] for p in rp.ONSET_PASSES}
        leaked = names & self._RAW_NAMES
        self.assertFalse(
            leaked,
            f"ONSET_PASSES exposes raw instrument names: {sorted(leaked)}",
        )

    def test_pass_to_layer_covers_new_ids(self):
        for p in rp.ONSET_PASSES:
            self.assertIn(p["name"], rp.PASS_TO_LAYER)
            self.assertIn(
                rp.PASS_TO_LAYER[p["name"]],
                ("percussive", "harmonic", "full-spectrum"),
            )

    def test_legacy_aliases_still_resolve(self):
        # Read-only fallback so existing shipped analysis JSONs continue
        # to classify correctly.  These aliases must NOT appear in newly
        # generated output, though — see test_build_analysis_output_below.
        for legacy in ("kick", "snare", "hihat", "melody", "flux"):
            self.assertIn(legacy, rp.PASS_TO_LAYER)

    def test_build_analysis_output_excludes_raw_instrument_names(self):
        """build_analysis result has no raw instrument tokens in public fields."""
        # Synthesize a feature dict large enough for build_analysis to run.
        import numpy as np
        n_frames = 20
        mel_times = np.linspace(0.0, 2.0, n_frames)
        mel_energies = np.zeros((n_frames, 40))
        onsets = {p["name"]: [0.5, 1.0, 1.5] for p in rp.ONSET_PASSES}
        onset_breakdown = {p["name"]: [{"n_fft": 1024, "hop_length": 256, "count": 3,
                                         "effective_threshold": 0.3}]
                           for p in rp.ONSET_PASSES}
        features = {
            "bpm": 120.0,
            "beats": [0.0, 0.5, 1.0, 1.5, 2.0],
            "onsets": onsets,
            "onset_resolutions": [{"n_fft": 1024, "hop_length": 256}],
            "onset_breakdown": onset_breakdown,
            "mel_times": mel_times,
            "mel_energies": mel_energies,
            "mfcc_times": np.array([]),
            "mfcc_coeffs": np.array([]),
            "quiet": [],
            "duration": 2.0,
        }
        analysis = rp.build_analysis("synthetic.wav", features, onset_threshold=0.3)
        # 1) onsets keys must use only broad / non-instrumental subpass IDs.
        onset_keys = set(analysis["onsets"].keys())
        leaked_keys = onset_keys & self._RAW_NAMES
        self.assertFalse(
            leaked_keys,
            f"analysis['onsets'] exposes raw instrument names: {sorted(leaked_keys)}",
        )
        # 2) events[].passes must not include raw instrument tokens.
        for ev in analysis["events"]:
            leaked = set(ev.get("passes", [])) & self._RAW_NAMES
            self.assertFalse(
                leaked,
                f"event passes leak raw instrument names: {leaked} in {ev}",
            )
            # Every event must carry the broad layer field.
            self.assertIn(
                ev.get("layer"), ("percussive", "harmonic", "full-spectrum"),
            )
        # 3) onset_diagnostics.raw_per_pass must use the broad/non-instrumental IDs.
        raw_per_pass = analysis["onset_diagnostics"]["raw_per_pass"]
        leaked_diag = set(raw_per_pass.keys()) & self._RAW_NAMES
        self.assertFalse(
            leaked_diag,
            f"onset_diagnostics.raw_per_pass leaks raw names: {sorted(leaked_diag)}",
        )


# ---------------------------------------------------------------------------
# #448 — shipped analysis JSON artifacts must not leak raw instrument names
# ---------------------------------------------------------------------------


class TestShippedAnalysisArtifactsHaveNoRawInstrumentNames(unittest.TestCase):
    """Regression for issue #448.

    Loads every shipped ``content/beatmaps/*_analysis.json`` and asserts the
    public surfaces consumed by ``level_designer`` and downstream tooling
    expose only broad-layer / non-instrumental subpass IDs (no
    kick/snare/hihat/melody/flux).
    """

    _RAW_NAMES = {"kick", "snare", "hihat", "melody", "flux"}
    _BROAD_LAYERS = {"percussive", "harmonic", "full-spectrum"}

    def _shipped_analysis_paths(self):
        repo_root = Path(__file__).resolve().parents[1]
        beatmap_dir = repo_root / "content" / "beatmaps"
        return sorted(beatmap_dir.glob("*_analysis.json"))

    def test_at_least_one_shipped_analysis_present(self):
        self.assertTrue(
            self._shipped_analysis_paths(),
            "expected at least one content/beatmaps/*_analysis.json fixture",
        )

    def test_shipped_analyses_have_no_raw_instrument_names(self):
        import json
        for path in self._shipped_analysis_paths():
            with self.subTest(analysis=path.name):
                with path.open() as f:
                    analysis = json.load(f)

                # 1) onsets keys must be broad subpass IDs, not raw names.
                onset_keys = set(analysis.get("onsets", {}).keys())
                leaked = onset_keys & self._RAW_NAMES
                self.assertFalse(
                    leaked,
                    f"{path.name}: onsets exposes raw instrument names: {sorted(leaked)}",
                )

                # 2) events[*].passes tokens must not include raw names.
                for ev in analysis.get("events", []):
                    tokens = set(ev.get("passes", []) or [])
                    leaked_tokens = tokens & self._RAW_NAMES
                    self.assertFalse(
                        leaked_tokens,
                        f"{path.name}: event passes leak {sorted(leaked_tokens)}: {ev}",
                    )
                    layer = ev.get("layer")
                    self.assertIn(
                        layer, self._BROAD_LAYERS,
                        f"{path.name}: event missing/invalid broad layer: {ev}",
                    )

                # 3) onset_diagnostics.raw_per_pass keys must be broad subpass IDs.
                raw_per_pass = analysis.get("onset_diagnostics", {}).get(
                    "raw_per_pass", {}
                )
                leaked_diag = set(raw_per_pass.keys()) & self._RAW_NAMES
                self.assertFalse(
                    leaked_diag,
                    f"{path.name}: onset_diagnostics.raw_per_pass leaks {sorted(leaked_diag)}",
                )


# ---------------------------------------------------------------------------
# Migration helper coverage (idempotency + completeness)
# ---------------------------------------------------------------------------


class TestMigrateAnalysisRemoveRawInstrumentNames(unittest.TestCase):
    """Covers the public-naming migration helper used to scrub raw
    instrument tokens from shipped analysis JSON artifacts."""

    def _seed(self):
        return {
            "events": [
                {"t": 1.0, "passes": ["kick"], "layer": "percussive", "pass": "kick"},
                {"t": 1.04, "passes": ["melody", "flux"], "layer": "full-spectrum"},
                {"t": 2.0, "passes": ["snare", "hihat"], "layer": "percussive"},
            ],
            "onsets": {
                "kick":   {"method": "x", "zone": "bass",     "count": 2, "timestamps": [1.0, 3.0], "resolutions": []},
                "snare":  {"method": "x", "zone": None,       "count": 1, "timestamps": [2.0],      "resolutions": []},
                "hihat":  {"method": "x", "zone": "high_mid", "count": 1, "timestamps": [2.0],      "resolutions": []},
                "melody": {"method": "x", "zone": "low_mid",  "count": 1, "timestamps": [1.04],     "resolutions": []},
                "flux":   {"method": "x", "zone": None,       "count": 1, "timestamps": [1.04],     "resolutions": []},
            },
            "onset_diagnostics": {
                "raw_per_pass": {"kick": 2, "snare": 1, "hihat": 1, "melody": 1, "flux": 1},
            },
        }

    def test_migration_replaces_all_raw_pass_names(self):
        out = rp.migrate_analysis_remove_raw_instrument_names(self._seed())
        # No raw tokens anywhere in public fields.
        for ev in out["events"]:
            self.assertFalse(set(ev["passes"]) & rp.RAW_INSTRUMENT_PASS_NAMES)
            self.assertNotIn(ev.get("pass"), rp.RAW_INSTRUMENT_PASS_NAMES)
        self.assertFalse(set(out["onsets"].keys()) & rp.RAW_INSTRUMENT_PASS_NAMES)
        self.assertFalse(
            set(out["onset_diagnostics"]["raw_per_pass"].keys())
            & rp.RAW_INSTRUMENT_PASS_NAMES
        )
        # All onset/raw_per_pass keys are broad subpass IDs from the pipeline.
        valid_subpasses = {p["name"] for p in rp.ONSET_PASSES}
        self.assertTrue(set(out["onsets"].keys()).issubset(valid_subpasses))
        self.assertTrue(
            set(out["onset_diagnostics"]["raw_per_pass"].keys()).issubset(valid_subpasses)
        )
        # Layer field is preserved verbatim.
        self.assertEqual([ev["layer"] for ev in out["events"]],
                         ["percussive", "full-spectrum", "percussive"])

    def test_migration_is_idempotent(self):
        once = rp.migrate_analysis_remove_raw_instrument_names(self._seed())
        twice = rp.migrate_analysis_remove_raw_instrument_names(
            json_clone := __import__("json").loads(__import__("json").dumps(once))
        )
        self.assertEqual(once, twice)


class TestAssertNoRawInstrumentPasses(unittest.TestCase):
    """Issue #480 — write-time guard against raw-instrument pass names
    in public artifacts.  Protects #419/#448 from recurring silently."""

    def _clean_analysis(self):
        return {
            "events": [
                {"t": 1.0, "passes": ["percussive_bass"], "layer": "percussive"},
                {"t": 2.0, "passes": ["harmonic_low_mid"], "layer": "harmonic"},
            ],
            "onsets": {
                "percussive_bass": {"count": 1, "timestamps": [1.0]},
                "harmonic_low_mid": {"count": 1, "timestamps": [2.0]},
            },
            "onset_diagnostics": {
                "raw_per_pass": {"percussive_bass": 1, "harmonic_low_mid": 1},
            },
        }

    def test_clean_analysis_passes_guard(self):
        # Should not raise.
        rp.assert_no_raw_instrument_passes(self._clean_analysis())

    def test_raw_instrument_in_event_passes_raises(self):
        analysis = self._clean_analysis()
        analysis["events"][0]["passes"].append("kick")
        with self.assertRaises(ValueError) as ctx:
            rp.assert_no_raw_instrument_passes(analysis)
        self.assertIn("kick", str(ctx.exception))
        self.assertIn("events[*].passes", str(ctx.exception))

    def test_raw_instrument_in_legacy_pass_field_raises(self):
        analysis = self._clean_analysis()
        analysis["events"][1]["pass"] = "melody"
        with self.assertRaises(ValueError):
            rp.assert_no_raw_instrument_passes(analysis)

    def test_raw_instrument_in_onsets_keys_raises(self):
        analysis = self._clean_analysis()
        analysis["onsets"]["snare"] = {"count": 0, "timestamps": []}
        with self.assertRaises(ValueError) as ctx:
            rp.assert_no_raw_instrument_passes(analysis)
        self.assertIn("snare", str(ctx.exception))

    def test_raw_instrument_in_raw_per_pass_keys_raises(self):
        analysis = self._clean_analysis()
        analysis["onset_diagnostics"]["raw_per_pass"]["hihat"] = 3
        with self.assertRaises(ValueError) as ctx:
            rp.assert_no_raw_instrument_passes(analysis)
        self.assertIn("hihat", str(ctx.exception))

    def test_migration_then_guard_clears_legacy_artifact(self):
        legacy = {
            "events": [{"t": 1.0, "passes": ["kick", "snare"], "layer": "percussive"}],
            "onsets": {"melody": {"count": 0, "timestamps": []}},
            "onset_diagnostics": {"raw_per_pass": {"flux": 1}},
        }
        rp.migrate_analysis_remove_raw_instrument_names(legacy)
        # After migration the guard accepts the artifact.
        rp.assert_no_raw_instrument_passes(legacy)


# ---------------------------------------------------------------------------
# #451 — loop1 onset diagnostics must include harmonic broad-layer events
# when the source analysis carries any harmonic events.
# ---------------------------------------------------------------------------


class TestLoop1DiagnosticsCoverHarmonicLayer(unittest.TestCase):
    """Regression for issue #451.

    For every shipped ``*_loop1`` diagnostics directory whose source analysis
    JSON contains at least one harmonic-layer event, the regenerated
    ``onset_timing_events.csv`` must contain at least one row with
    ``onset_class == 'harmonic'`` and the ``timing_source`` column must be
    ``onset`` (no beat fallback)."""

    def _loop1_pairs(self):
        repo_root = Path(__file__).resolve().parents[1]
        diag_root = repo_root / "tools" / "diagnostics"
        beatmap_dir = repo_root / "content" / "beatmaps"
        for csv_path in sorted(diag_root.glob("*_loop1/onset_timing_events.csv")):
            song_id = csv_path.parent.name.removesuffix("_loop1")
            analysis_path = beatmap_dir / f"{song_id}_analysis.json"
            if analysis_path.exists():
                yield analysis_path, csv_path

    def test_harmonic_present_when_source_has_harmonic_events(self):
        import csv
        import json

        pairs = list(self._loop1_pairs())
        self.assertTrue(pairs, "expected at least one *_loop1 diagnostics fixture")

        for analysis_path, csv_path in pairs:
            with self.subTest(song=analysis_path.stem):
                with analysis_path.open() as f:
                    analysis = json.load(f)
                source_has_harmonic = any(
                    ev.get("layer") == "harmonic"
                    for ev in analysis.get("events", [])
                )
                if not source_has_harmonic:
                    continue

                with csv_path.open(newline="") as f:
                    reader = csv.DictReader(f)
                    rows = list(reader)

                self.assertTrue(rows, f"{csv_path} unexpectedly empty")
                onset_classes = {row.get("onset_class") for row in rows}
                self.assertIn(
                    "harmonic", onset_classes,
                    f"{csv_path}: harmonic broad layer missing despite source having harmonic events",
                )
                # Onset-only timing requirement (no beat fallback).
                timing_sources = {row.get("timing_source") for row in rows}
                self.assertEqual(
                    timing_sources, {"onset"},
                    f"{csv_path}: non-onset timing_source values: {timing_sources}",
                )


class TestFullSpectrumFluxIndependence(unittest.TestCase):
    """Regression for issue #491.

    The ``full_spectrum_flux`` pass uses an STFT-based spectral flux envelope
    and must NOT be a deterministic duplicate of ``percussive_broadband``,
    which uses the mel-band flux path.
    """

    def _synth_audio(self, sr: int = 22050) -> "tuple[object, int]":
        import numpy as np
        rng = np.random.default_rng(0xBEA7)
        duration = 4.0
        t = np.arange(int(sr * duration)) / sr
        y = 0.02 * rng.standard_normal(t.shape).astype("float32")
        # Broadband percussive transients.
        for onset in (0.30, 0.95, 1.62, 2.30, 3.05):
            i = int(onset * sr)
            n = int(0.05 * sr)
            env = np.exp(-np.linspace(0, 6, n)).astype("float32")
            noise = rng.standard_normal(n).astype("float32")
            if i + n <= len(y):
                y[i:i + n] += 0.6 * env * noise
        # Tonal harmonic content (sustained sine bursts) — adds spectral
        # energy redistribution that STFT flux can see but mel-band positive
        # flux barely registers as discrete onsets.
        for onset, freq in ((0.55, 440.0), (1.20, 660.0), (2.70, 523.0)):
            i = int(onset * sr)
            n = int(0.30 * sr)
            env = np.hanning(n).astype("float32")
            tone = np.sin(2 * np.pi * freq * np.arange(n) / sr).astype("float32")
            if i + n <= len(y):
                y[i:i + n] += 0.35 * env * tone
        return y, sr

    def test_dispatch_method_is_a_parameter(self):
        import inspect
        sig = inspect.signature(rp._detect_pass_onsets)
        self.assertIn(
            "method", sig.parameters,
            "_detect_pass_onsets must accept a 'method' kwarg so detection "
            "can dispatch on ONSET_PASSES[*].method (issue #491)",
        )

    def test_spectral_flux_envelope_differs_from_mel_band_envelope(self):
        import numpy as np
        y, sr = self._synth_audio()
        n_fft, hop = 2048, 512
        stft_env = rp._spectral_flux_onset_envelope(
            y=y, hop_length=hop, n_fft=n_fft, reduce="mean",
        )
        mel_db = rp._mel_spectrogram_db(y, sr, n_fft=n_fft, hop_length=hop, n_mels=40)
        mel_env = rp._flux_envelope(mel_db, None)
        # Length-align for comparison.
        n = min(len(stft_env), len(mel_env))
        a = np.asarray(stft_env[:n], dtype=float)
        b = np.asarray(mel_env[:n], dtype=float)
        if a.std() > 0 and b.std() > 0:
            corr = float(np.corrcoef(a, b)[0, 1])
        else:
            corr = 0.0
        self.assertLess(
            corr, 0.999,
            "STFT spectral-flux envelope is essentially identical to the "
            "mel-band flux envelope; full_spectrum_flux would be a duplicate",
        )

    def test_full_spectrum_flux_not_equal_to_percussive_broadband(self):
        y, sr = self._synth_audio()
        librosa_config = {
            "onset": {
                "n_mels": 40,
                "peak_pick": {},
                "zone_thresholds": {},
                "resolutions": [{"n_fft": 2048, "hop_length": 512}],
            },
            "beat": {"n_fft": 2048, "hop_length": 512},
        }
        # Run only the onset-pass loop logic by calling _detect_pass_onsets
        # directly for both passes — avoids needing a file on disk.
        res = {"n_fft": 2048, "hop_length": 512}
        broadband = rp._detect_pass_onsets(
            y=y, sr=sr, zone=None, threshold=0.10, resolution=res,
            n_mels=40, peak_pick_cfg={}, zone_thresholds={},
            method="band_flux_full",
        )
        full_spec = rp._detect_pass_onsets(
            y=y, sr=sr, zone=None, threshold=0.10, resolution=res,
            n_mels=40, peak_pick_cfg={}, zone_thresholds={},
            method="spectral_flux",
        )
        self.assertNotEqual(
            broadband, full_spec,
            "full_spectrum_flux must produce a distinct detection list from "
            "percussive_broadband (issue #491)",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
