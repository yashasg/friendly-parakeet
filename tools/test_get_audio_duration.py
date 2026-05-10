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

    def test_duration_extends_when_beats_or_onsets_exceed_track_duration(self):
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

        self.assertEqual(features["duration"], 10.0)

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


if __name__ == "__main__":
    unittest.main(verbosity=2)
