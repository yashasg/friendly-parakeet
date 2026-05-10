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


if __name__ == "__main__":
    unittest.main(verbosity=2)
