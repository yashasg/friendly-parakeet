#!/usr/bin/env python3
"""Deterministic check: every shipped hard beatmap must contain at least one
low_bar and one high_bar obstacle (issue #125).

Exits 0 on success, 1 on failure. Prints a summary either way.

Usage:
    python3 tools/check_bar_coverage.py
"""
from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BEATMAPS = REPO / "content" / "beatmaps"
REQUIRED_HARD_KINDS = ("low_bar", "high_bar")


def main() -> int:
    failures: list[str] = []
    rows: list[str] = []

    for path in sorted(BEATMAPS.glob("*_beatmap.json")):
        bm = json.loads(path.read_text())
        hard = bm.get("difficulties", {}).get("hard", {}).get("beats", [])
        counts = Counter(b["kind"] for b in hard)
        missing = [k for k in REQUIRED_HARD_KINDS if counts.get(k, 0) < 1]
        status = "OK" if not missing else f"MISSING {missing}"
        rows.append(
            f"  {path.name:40s} hard={len(hard):4d}  "
            f"low_bar={counts.get('low_bar',0):3d}  "
            f"high_bar={counts.get('high_bar',0):3d}  {status}"
        )
        if missing:
            failures.append(f"{path.name}: missing {missing}")

    print("Bar coverage check (issue #125)")
    print("\n".join(rows))
    if failures:
        print("\nFAIL:")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("\nPASS: all hard beatmaps contain low_bar and high_bar.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
