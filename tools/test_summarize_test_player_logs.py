#!/usr/bin/env python3
from __future__ import annotations

import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import summarize_test_player_logs as summary  # noqa: E402


class TestSummarizeTestPlayerLogs(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Path(__file__).resolve().parent / ".test_summarize_test_player_logs"
        if self.base.exists():
            shutil.rmtree(self.base)
        self.base.mkdir(parents=True, exist_ok=True)
        self.addCleanup(lambda: shutil.rmtree(self.base, ignore_errors=True))

    def _write(self, name: str, text: str) -> Path:
        path = self.base / name
        path.write_text(text, encoding="utf-8")
        return path

    def test_summarizes_metadata_grades_and_first_failure(self) -> None:
        log = self._write(
            "session_pro_1_stomper_hard_rt0000000001_n0001.log",
            "\n".join(
                [
                    "skill=pro level=1_stomper difficulty=hard seed=1",
                    "[F:0001 T:001.000] [GAME  ] COLLISION obstacle=1 beat=4 expected=1.000 drift=+0.000s kind=ShapeGate result=CLEAR timing=Perfect(1.00)",
                    "[F:0002 T:002.000] [GAME  ] COLLISION obstacle=2 beat=8 expected=2.000 drift=+0.250s kind=ShapeGate result=MISS section=chorus",
                ]
            ),
        )

        item = summary.summarize_log(log)

        self.assertEqual(item.skill, "pro")
        self.assertEqual(item.level, "1_stomper")
        self.assertEqual(item.difficulty, "hard")
        self.assertEqual(item.status, "FAIL")
        self.assertEqual(item.clears, 1)
        self.assertEqual(item.misses, 1)
        self.assertEqual(item.grade_counts["Perfect"], 1)
        self.assertEqual(item.first_failure_time, "002.000")
        self.assertEqual(item.first_failure_section, "chorus")
        self.assertEqual(item.first_failure_beat, "8")

    def test_matrix_validator_reports_missing_entries(self) -> None:
        log = self._write(
            "session_bad_2_drama_easy_rt0000000001_n0001.log",
            "skill=bad level=2_drama difficulty=easy seed=1\n",
        )

        missing = summary._missing_matrix_entries([summary.summarize_log(log)])

        self.assertEqual(len(missing), 26)
        self.assertNotIn(("bad", "2_drama", "easy"), missing)


if __name__ == "__main__":
    unittest.main(verbosity=2)
