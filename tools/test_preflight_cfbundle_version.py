#!/usr/bin/env python3
"""Unit tests for tools/ios/preflight_cfbundle_version.sh."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from shutil import which


REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPT = REPO_ROOT / "tools" / "ios" / "preflight_cfbundle_version.sh"
BASH = which("bash")


class TestCFBundleVersionPreflight(unittest.TestCase):
    def _run(self, args, cwd: Path | None = None):
        bash = BASH
        if bash is None:
            self.fail("bash is required to run preflight_cfbundle_version.sh")
        return subprocess.run(
            [bash, str(SCRIPT), *args],
            cwd=str(cwd or REPO_ROOT),
            text=True,
            capture_output=True,
            check=False,
        )

    def test_accepts_strictly_increasing_explicit_previous(self):
        result = self._run(["--current", "101", "--previous", "100"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("preflight passed", result.stdout)

    def test_rejects_non_increasing_version(self):
        result = self._run(["--current", "100", "--previous", "100"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must be strictly greater", result.stdout + result.stderr)

    def test_rejects_non_integer_current(self):
        result = self._run(["--current", "v100", "--previous", "99"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must be a non-negative integer", result.stdout + result.stderr)

    def test_resolves_previous_from_matching_tags(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            subprocess.run(["git", "init"], cwd=repo, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.email", "ci@example.com"], cwd=repo, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.name", "CI"], cwd=repo, check=True, capture_output=True)
            (repo / "README.md").write_text("fixture\n")
            subprocess.run(["git", "add", "README.md"], cwd=repo, check=True, capture_output=True)
            subprocess.run(["git", "commit", "-m", "fixture"], cwd=repo, check=True, capture_output=True)
            subprocess.run(["git", "tag", "ios-build-41"], cwd=repo, check=True, capture_output=True)
            subprocess.run(["git", "tag", "ios-build-100"], cwd=repo, check=True, capture_output=True)
            subprocess.run(["git", "tag", "other-prefix-999"], cwd=repo, check=True, capture_output=True)

            fail = self._run(["--current", "100"], cwd=repo)
            self.assertNotEqual(fail.returncode, 0)
            self.assertIn("previous=100", fail.stdout + fail.stderr)

            ok = self._run(["--current", "101"], cwd=repo)
            self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
            self.assertIn("current=101 > previous=100", ok.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
