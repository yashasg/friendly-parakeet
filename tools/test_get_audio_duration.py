"""
test_get_audio_duration.py
==========================
Unit tests for get_audio_duration() in rhythm_pipeline.py.

Covers:
  - Happy path: ffprobe returns a valid duration float
  - ffprobe not found (FileNotFoundError)  → None, no raise
  - ffprobe times out (TimeoutExpired)     → None, warning on stderr, no raise
  - ffprobe non-zero exit code             → None, no raise
  - ffprobe returns zero / negative        → None (not a valid duration)
  - ffprobe returns unparseable output     → None, no raise

Run from repo root:
    python tools/test_get_audio_duration.py
"""

import subprocess
import sys
import unittest
from io import StringIO
from unittest.mock import MagicMock, patch

# Allow importing from the tools directory regardless of cwd
sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))

from rhythm_pipeline import FFPROBE_TIMEOUT, get_audio_duration  # noqa: E402


def _make_completed(stdout: str = "", returncode: int = 0) -> MagicMock:
    m = MagicMock(spec=subprocess.CompletedProcess)
    m.stdout = stdout
    m.returncode = returncode
    return m


class TestGetAudioDurationHappyPath(unittest.TestCase):
    def test_valid_duration_returned(self):
        with patch("rhythm_pipeline.subprocess.run", return_value=_make_completed("183.42\n")):
            result = get_audio_duration("song.flac")
        self.assertAlmostEqual(result, 183.42, places=5)

    def test_timeout_kwarg_passed_to_subprocess(self):
        with patch("rhythm_pipeline.subprocess.run", return_value=_make_completed("60.0\n")) as mock_run:
            get_audio_duration("song.flac")
        _args, kwargs = mock_run.call_args
        self.assertIn("timeout", kwargs, "timeout must be passed to subprocess.run")
        self.assertEqual(kwargs["timeout"], FFPROBE_TIMEOUT)


class TestGetAudioDurationErrorPaths(unittest.TestCase):
    def test_returns_none_when_ffprobe_not_found(self):
        with patch("rhythm_pipeline.subprocess.run", side_effect=FileNotFoundError):
            result = get_audio_duration("song.flac")
        self.assertIsNone(result)

    def test_returns_none_and_logs_on_timeout(self):
        with patch(
            "rhythm_pipeline.subprocess.run",
            side_effect=subprocess.TimeoutExpired(cmd=["ffprobe"], timeout=FFPROBE_TIMEOUT),
        ):
            buf = StringIO()
            with patch("sys.stderr", buf):
                result = get_audio_duration("song.flac")
        self.assertIsNone(result)
        self.assertIn("timed out", buf.getvalue().lower(),
                      "TimeoutExpired must produce a visible warning on stderr")

    def test_returns_none_on_nonzero_exit(self):
        with patch("rhythm_pipeline.subprocess.run", return_value=_make_completed("", returncode=1)):
            result = get_audio_duration("song.flac")
        self.assertIsNone(result)

    def test_returns_none_on_zero_duration(self):
        with patch("rhythm_pipeline.subprocess.run", return_value=_make_completed("0.0\n")):
            result = get_audio_duration("song.flac")
        self.assertIsNone(result)

    def test_returns_none_on_negative_duration(self):
        with patch("rhythm_pipeline.subprocess.run", return_value=_make_completed("-1.5\n")):
            result = get_audio_duration("song.flac")
        self.assertIsNone(result)

    def test_returns_none_on_unparseable_output(self):
        with patch("rhythm_pipeline.subprocess.run", return_value=_make_completed("N/A\n")):
            result = get_audio_duration("song.flac")
        self.assertIsNone(result)

    def test_does_not_raise_on_timeout(self):
        """TimeoutExpired must never propagate to callers."""
        with patch(
            "rhythm_pipeline.subprocess.run",
            side_effect=subprocess.TimeoutExpired(cmd=["ffprobe"], timeout=FFPROBE_TIMEOUT),
        ):
            try:
                with patch("sys.stderr", StringIO()):
                    get_audio_duration("song.flac")
            except subprocess.TimeoutExpired:
                self.fail("TimeoutExpired must not propagate from get_audio_duration()")


if __name__ == "__main__":
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TestGetAudioDurationHappyPath))
    suite.addTests(loader.loadTestsFromTestCase(TestGetAudioDurationErrorPaths))
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)
