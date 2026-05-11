#!/usr/bin/env python3
"""Summarize and validate SHAPESHIFTER test-player session logs."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path


SKILLS = ("pro", "good", "bad")
LEVELS = ("1_stomper", "2_drama", "3_mental_corruption")
DIFFICULTIES = ("easy", "medium", "hard")

HEADER_RE = re.compile(r"\bskill=(?P<skill>\w+)\s+level=(?P<level>[\w_]+)\s+difficulty=(?P<difficulty>\w+)")
FILENAME_RE = re.compile(
    r"session_(?P<skill>pro|good|bad)_(?P<level>1_stomper|2_drama|3_mental_corruption)"
    r"_(?P<difficulty>easy|medium|hard)_"
)
TIME_RE = re.compile(r"\bT:(?P<time>[0-9]+\.[0-9]+)")
BEAT_RE = re.compile(r"\bbeat=(?P<beat>-?[0-9]+)")
SECTION_RE = re.compile(r"\bsection=(?P<section>[\w.-]+)")
RESULT_RE = re.compile(r"\bresult=(?P<result>MISS|CLEAR)\b")
TIMING_RE = re.compile(r"\btiming=(?P<timing>[A-Za-z]+)\(")


@dataclass
class LogSummary:
    path: Path
    skill: str = "unknown"
    level: str = "unknown"
    difficulty: str = "unknown"
    clears: int = 0
    misses: int = 0
    grade_counts: Counter[str] = field(default_factory=Counter)
    first_failure_time: str = "-"
    first_failure_section: str = "-"
    first_failure_beat: str = "-"

    @property
    def status(self) -> str:
        return "CLEAR" if self.misses == 0 else "FAIL"


def _metadata_from_filename(path: Path) -> tuple[str, str, str] | None:
    match = FILENAME_RE.search(path.name)
    if not match:
        return None
    return match.group("skill"), match.group("level"), match.group("difficulty")


def summarize_log(path: Path) -> LogSummary:
    summary = LogSummary(path=path)
    from_name = _metadata_from_filename(path)
    if from_name:
        summary.skill, summary.level, summary.difficulty = from_name

    for line in path.read_text(encoding="utf-8").splitlines():
        header = HEADER_RE.search(line)
        if header:
            summary.skill = header.group("skill")
            summary.level = header.group("level")
            summary.difficulty = header.group("difficulty")
            continue

        result = RESULT_RE.search(line)
        if not result:
            continue

        if result.group("result") == "MISS":
            summary.misses += 1
            if summary.first_failure_time == "-":
                summary.first_failure_time = _match_or_default(TIME_RE, line, "time")
                summary.first_failure_section = _match_or_default(SECTION_RE, line, "section")
                summary.first_failure_beat = _match_or_default(BEAT_RE, line, "beat")
        else:
            summary.clears += 1
            timing = TIMING_RE.search(line)
            if timing:
                summary.grade_counts[timing.group("timing")] += 1

    return summary


def _match_or_default(pattern: re.Pattern[str], line: str, group: str) -> str:
    match = pattern.search(line)
    return match.group(group) if match else "-"


def _format_grades(grades: Counter[str]) -> str:
    if not grades:
        return "-"
    return ",".join(f"{grade}:{count}" for grade, count in sorted(grades.items()))


def _missing_matrix_entries(summaries: list[LogSummary]) -> list[tuple[str, str, str]]:
    seen = {(item.skill, item.level, item.difficulty) for item in summaries}
    return [
        (skill, level, difficulty)
        for skill in SKILLS
        for level in LEVELS
        for difficulty in DIFFICULTIES
        if (skill, level, difficulty) not in seen
    ]


def print_summary(summaries: list[LogSummary]) -> None:
    print("skill level difficulty status clears misses grades first_failure_time first_failure_section first_failure_beat path")
    for item in sorted(summaries, key=lambda s: (s.skill, s.level, s.difficulty, str(s.path))):
        print(
            f"{item.skill} {item.level} {item.difficulty} {item.status} "
            f"{item.clears} {item.misses} {_format_grades(item.grade_counts)} "
            f"{item.first_failure_time} {item.first_failure_section} {item.first_failure_beat} {item.path}"
        )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path, help="session_*.log files to summarize")
    parser.add_argument(
        "--expect-matrix",
        action="store_true",
        help="fail if the pro/good/bad x shipped-level x easy/medium/hard matrix is incomplete",
    )
    args = parser.parse_args(argv)

    summaries = [summarize_log(path) for path in args.logs]
    print_summary(summaries)

    if args.expect_matrix:
        missing = _missing_matrix_entries(summaries)
        if missing:
            for skill, level, difficulty in missing:
                print(f"missing matrix log: skill={skill} level={level} difficulty={difficulty}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
