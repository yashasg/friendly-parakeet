"""Unit tests for duration handling in rhythm_pipeline.extract_features()."""

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np

# Allow importing from the tools directory regardless of cwd
sys.path.insert(0, str(Path(__file__).parent))

import rhythm_pipeline as rp  # noqa: E402


class TestExtractFeaturesDuration(unittest.TestCase):
    def test_duration_uses_track_duration_when_larger_than_computed(self):
        with patch("rhythm_pipeline.librosa.load", return_value=(np.zeros(8, dtype=np.float32), 48000)), \
             patch("rhythm_pipeline.librosa.get_duration", return_value=120.0), \
             patch("rhythm_pipeline.get_tempo_and_beats", return_value=(120.0, [])), \
             patch("rhythm_pipeline.onset_resolutions", return_value=[{"n_fft": 2048, "hop_length": 512}]), \
             patch("rhythm_pipeline._detect_pass_onsets", return_value=[]), \
             patch(
                 "rhythm_pipeline.get_melbands",
                 return_value=(np.array([0.0], dtype=np.float64), np.zeros((1, 1), dtype=np.float32)),
             ), \
             patch(
                 "rhythm_pipeline.get_mfcc",
                 return_value=(np.array([0.0], dtype=np.float64), np.zeros((1, 1), dtype=np.float32)),
             ), \
             patch("rhythm_pipeline.get_quiet_regions", return_value=[]):
            features = rp.extract_features("song.wav", onset_threshold=0.1, librosa_config={})

        self.assertEqual(features["duration"], 120.0)
        self.assertEqual(features["last_event_time"], 0.0)

    def test_duration_does_not_inflate_past_true_track_length(self):
        # Issue #477 — even when beats or onsets sit past the true audio
        # length, ``duration`` must report the real track length, not a
        # synthetic max(true, last_event + 1.0).  The latest event time
        # is exposed separately as ``last_event_time`` for downstream
        # validators that need event bounds.
        detect_returns = [[9.0]] + [[] for _ in range(len(rp.ONSET_PASSES) - 1)]
        with patch("rhythm_pipeline.librosa.load", return_value=(np.zeros(8, dtype=np.float32), 48000)), \
             patch("rhythm_pipeline.librosa.get_duration", return_value=5.0), \
             patch("rhythm_pipeline.get_tempo_and_beats", return_value=(120.0, [8.0])), \
             patch("rhythm_pipeline.onset_resolutions", return_value=[{"n_fft": 2048, "hop_length": 512}]), \
             patch("rhythm_pipeline._detect_pass_onsets", side_effect=detect_returns), \
             patch("rhythm_pipeline.get_melbands", return_value=(np.array([0.0], dtype=np.float64), np.zeros((1, 1), dtype=np.float32))), \
             patch("rhythm_pipeline.get_mfcc", return_value=(np.array([0.0], dtype=np.float64), np.zeros((1, 1), dtype=np.float32))), \
             patch("rhythm_pipeline.get_quiet_regions", return_value=[]):
            features = rp.extract_features("song.wav", onset_threshold=0.1, librosa_config={})

        # True audio duration preserved, NOT inflated to last_event + 1.0.
        self.assertEqual(features["duration"], 5.0)
        # Latest event boundary surfaced separately.
        self.assertEqual(features["last_event_time"], 9.0)

    def test_duration_stays_track_length_when_no_beats_or_onsets(self):
        with patch("rhythm_pipeline.librosa.load", return_value=(np.zeros(8, dtype=np.float32), 48000)), \
             patch("rhythm_pipeline.librosa.get_duration", return_value=7.5), \
             patch("rhythm_pipeline.get_tempo_and_beats", return_value=(120.0, [])), \
             patch("rhythm_pipeline.onset_resolutions", return_value=[{"n_fft": 2048, "hop_length": 512}]), \
             patch("rhythm_pipeline._detect_pass_onsets", return_value=[]), \
             patch("rhythm_pipeline.get_melbands", return_value=(np.array([0.0], dtype=np.float64), np.zeros((1, 1), dtype=np.float32))), \
             patch("rhythm_pipeline.get_mfcc", return_value=(np.array([0.0], dtype=np.float64), np.zeros((1, 1), dtype=np.float32))), \
             patch("rhythm_pipeline.get_quiet_regions", return_value=[]):
            features = rp.extract_features("song.wav", onset_threshold=0.1, librosa_config={})

        self.assertEqual(features["duration"], 7.5)
        self.assertEqual(features["last_event_time"], 0.0)


class TestQuietFallbackStructure(unittest.TestCase):
    def test_quiet_fallback_promotes_pre_chorus_before_high_energy_drop(self):
        mel_times = np.arange(0.0, 40.0, 1.0, dtype=np.float64)
        mel_energies = np.full_like(mel_times, 0.4, dtype=np.float64)
        mel_energies[(mel_times >= 5.0) & (mel_times < 10.0)] = 0.05
        mel_energies[(mel_times >= 10.0) & (mel_times < 22.0)] = 0.7
        mel_energies[(mel_times >= 22.0) & (mel_times < 24.0)] = 0.05
        mel_energies[(mel_times >= 24.0) & (mel_times < 32.0)] = 1.0

        structure = rp._build_structure_from_quiet({
            "duration": 40.0,
            "quiet": [
                {"type": "QUIET", "t": 5.0},
                {"type": "NOISY", "t": 10.0},
                {"type": "QUIET", "t": 22.0},
                {"type": "NOISY", "t": 24.0},
                {"type": "QUIET", "t": 32.0},
            ],
            "mel_times": mel_times,
            "mel_energies": mel_energies,
        })

        noisy_sections = [s["section"] for s in structure if s["intensity"] != "low"]
        self.assertIn("pre-chorus", noisy_sections)
        self.assertIn("drop", noisy_sections)

    def test_quiet_fallback_keeps_single_noisy_segment_as_verse_without_energy_context(self):
        structure = rp._build_structure_from_quiet({
            "duration": 20.0,
            "quiet": [{"type": "QUIET", "t": 12.0}],
            "mel_times": np.array([], dtype=np.float64),
            "mel_energies": np.array([], dtype=np.float64),
        })

        self.assertEqual(structure[0]["section"], "verse")
        self.assertEqual(structure[0]["intensity"], "medium")


if __name__ == "__main__":
    unittest.main(verbosity=2)
