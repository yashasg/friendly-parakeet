#!/usr/bin/env python3
"""Acceptance check for #125: verify hard beatmaps contain low_bar and high_bar.

Usage:
  # Check all beatmaps in the default content directory:
  python tools/validate_beatmap_bars.py

  # Check a specific beatmap file:
  python tools/validate_beatmap_bars.py content/beatmaps/1_stomper_beatmap.json

  # Check a specific file at a specific difficulty:
  python tools/validate_beatmap_bars.py content/beatmaps/1_stomper_beatmap.json --difficulty hard

Exit codes:
  0 — all checked beatmaps pass
  1 — one or more beatmaps fail (printed to stderr)
"""

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
DEFAULT_BEATMAP_DIR = REPO_ROOT / "content" / "beatmaps"


def check_beatmap(path: Path, difficulty: str) -> list[str]:
    """Return list of failure messages. Empty list means pass."""
    failures = []
    try:
        with open(path) as f:
            data = json.load(f)
    except Exception as e:
        return [f"{path.name}: could not load JSON: {e}"]

    diffs = data.get("difficulties", {})
    if not diffs:
        # Flat format — check top-level beats
        beats = data.get("beats", [])
        diff_label = "flat"
        _check_beats(path.name, diff_label, beats, failures)
        return failures

    # Check requested difficulty (or all if "all")
    targets = list(diffs.keys()) if difficulty == "all" else [difficulty]
    for diff_name in targets:
        if diff_name not in diffs:
            failures.append(f"{path.name} [{diff_name}]: difficulty not found")
            continue
        beats = diffs[diff_name].get("beats", [])
        _check_beats(path.name, diff_name, beats, failures)

    return failures


def _check_beats(file_label: str, diff_label: str, beats: list, failures: list) -> None:
    if not beats:
        failures.append(f"{file_label} [{diff_label}]: 0 obstacles — beatmap is empty")
        return

    kinds = [o.get("kind", "MISSING") for o in beats]
    kind_set = set(kinds)

    has_low = "low_bar" in kind_set
    has_high = "high_bar" in kind_set

    if not has_low:
        failures.append(
            f"{file_label} [{diff_label}]: no low_bar obstacles "
            f"(total={len(beats)}, kinds={sorted(kind_set)})"
        )
    if not has_high:
        failures.append(
            f"{file_label} [{diff_label}]: no high_bar obstacles "
            f"(total={len(beats)}, kinds={sorted(kind_set)})"
        )

    # Sanity: no unknown or deprecated/non-shipping kinds
    known = {
        "shape_gate",
        "low_bar", "high_bar", "combo_gate", "split_path",
    }
    deprecated = kind_set & {"lane_block", "lane_push_left", "lane_push_right"}
    if deprecated:
        failures.append(
            f"{file_label} [{diff_label}]: deprecated/non-shipping obstacle kinds: "
            f"{sorted(deprecated)}"
        )

    unknown = kind_set - known - deprecated
    if unknown:
        failures.append(
            f"{file_label} [{diff_label}]: unknown obstacle kinds: {sorted(unknown)}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate low_bar/high_bar in beatmaps")
    parser.add_argument(
        "files",
        nargs="*",
        help="Beatmap JSON files to check. Defaults to all *_beatmap.json in content/beatmaps/",
    )
    parser.add_argument(
        "--difficulty", "-d",
        default="hard",
        choices=["easy", "medium", "hard", "all"],
        help="Difficulty to check (default: hard)",
    )
    args = parser.parse_args()

    if args.files:
        paths = [Path(f) for f in args.files]
    else:
        paths = sorted(DEFAULT_BEATMAP_DIR.glob("*_beatmap.json"))
        if not paths:
            print(f"No beatmaps found in {DEFAULT_BEATMAP_DIR}", file=sys.stderr)
            return 1

    all_failures: list[str] = []
    for path in paths:
        failures = check_beatmap(path, args.difficulty)
        all_failures.extend(failures)
        status = "✓" if not failures else "✗"
        print(f"  {status} {path.name} [{args.difficulty}]")
        for f in failures:
            print(f"      FAIL: {f}", file=sys.stderr)

    print()
    if all_failures:
        print(f"FAILED: {len(all_failures)} check(s) failed.", file=sys.stderr)
        return 1

    print(f"PASSED: all {len(paths)} beatmap(s) contain low_bar and high_bar in [{args.difficulty}].")
    return 0


if __name__ == "__main__":
    sys.exit(main())
