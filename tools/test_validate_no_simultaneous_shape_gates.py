#!/usr/bin/env python3
"""Unit tests for validate_no_simultaneous_shape_gates.py (issue #642)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import validate_no_simultaneous_shape_gates as v  # noqa: E402


def _gate(t, lane, shape):
    return {"kind": "shape_gate", "time_sec": t, "lane": lane, "shape": shape}


def _marker(t, lane, shape, onset_class="percussive"):
    return {
        "kind": "onset_marker",
        "time_sec": t,
        "lane": lane,
        "shape": shape,
        "onset_class": onset_class,
        "timing_source": "onset",
    }


class TestNoSimultaneousShapeGates(unittest.TestCase):
    def test_distinct_pair_within_morph_window_is_flagged(self):
        beats = [_gate(0.0, 0, "circle"), _gate(0.05, 1, "square")]
        pairs = v.find_simultaneous_shape_gate_pairs(beats)
        self.assertEqual(len(pairs), 1)

    def test_same_lane_shape_pair_is_not_flagged(self):
        beats = [_gate(0.0, 0, "circle"), _gate(0.05, 0, "circle")]
        pairs = v.find_simultaneous_shape_gate_pairs(beats)
        self.assertEqual(pairs, [])

    def test_pair_outside_morph_window_passes(self):
        beats = [_gate(0.0, 0, "circle"), _gate(0.051, 1, "square")]
        pairs = v.find_simultaneous_shape_gate_pairs(beats)
        self.assertEqual(pairs, [])

    def test_three_clustered_distinct_gates_yields_three_pairs(self):
        beats = [
            _gate(0.0, 0, "circle"),
            _gate(0.04, 1, "square"),
            _gate(0.08, 2, "triangle"),
        ]
        pairs = v.find_simultaneous_shape_gate_pairs(beats)
        # (0,1), (0,2), (1,2)
        self.assertEqual(len(pairs), 3)

    def test_protected_cross_layer_required_pair_is_flagged(self):
        beats = [
            {**_gate(1.0, 0, "triangle"), "onset_class": "percussive", "timing_source": "onset"},
            {**_gate(1.0, 1, "square"), "onset_class": "full-spectrum", "timing_source": "onset"},
        ]
        groups = v.find_required_action_groups(beats)
        self.assertEqual(len(groups), 1)

    def test_onset_marker_preserves_sibling_without_required_action(self):
        beats = [
            {**_gate(1.0, 0, "triangle"), "onset_class": "percussive", "timing_source": "onset"},
            _marker(1.0, 1, "square", "full-spectrum"),
        ]
        groups = v.find_required_action_groups(beats)
        self.assertEqual(groups, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
