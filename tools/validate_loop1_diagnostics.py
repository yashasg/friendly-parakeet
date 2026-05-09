#!/usr/bin/env python3
"""Validate Loop 1 beatmap diagnostic histograms (shape + subdivision)."""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

import level_designer as ld

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO_ROOT / "content" / "beatmaps"
KNOWN_SUBDIVISIONS = {"downbeat", "eighth", "triplet", "offgrid"}


def load_analysis(path: Path) -> dict:
    with open(path) as handle:
        return json.load(handle)


def summarize_difficulty(analysis: dict, difficulty: str) -> tuple[Counter, Counter]:
    selected, _ = ld.select_beats(analysis, difficulty)
    obstacles = ld.design_level(analysis, difficulty, cleanup_enabled=True)

    shape_hist = Counter(
        obs.get("shape", "")
        for obs in obstacles
        if obs.get("kind") == "shape_gate"
    )
    subdivision_hist = Counter(
        event.get("subdivision", "unknown")
        for event in selected.values()
        if isinstance(event, dict)
    )
    return shape_hist, subdivision_hist


def validate_analysis(path: Path) -> list[str]:
    errors: list[str] = []
    analysis = load_analysis(path)
    song = analysis.get("title", path.stem.replace("_analysis", ""))

    for difficulty in ("easy", "medium", "hard"):
        try:
            shape_hist, subdivision_hist = summarize_difficulty(analysis, difficulty)
        except Exception as exc:  # pragma: no cover - defensive CLI validation
            errors.append(f"{song} [{difficulty}] generation failed: {exc}")
            continue

        shape_total = sum(shape_hist.values())
        subdivision_total = sum(subdivision_hist.values())

        unknown_shapes = set(shape_hist) - set(ld.ALL_SHAPES)
        unknown_subdivisions = set(subdivision_hist) - KNOWN_SUBDIVISIONS

        if shape_total <= 0:
            errors.append(f"{song} [{difficulty}] empty shape histogram")
        if subdivision_total <= 0:
            errors.append(f"{song} [{difficulty}] empty subdivision histogram")
        if unknown_shapes:
            errors.append(
                f"{song} [{difficulty}] unknown shape bins: {sorted(unknown_shapes)}"
            )
        if unknown_subdivisions:
            errors.append(
                f"{song} [{difficulty}] unknown subdivision bins: {sorted(unknown_subdivisions)}"
            )

        print(
            f"{song:24s} [{difficulty:6s}] "
            f"shapes={dict(sorted(shape_hist.items()))} "
            f"subdivisions={dict(sorted(subdivision_hist.items()))}"
        )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", help="Optional *_analysis.json paths")
    args = parser.parse_args()

    paths = (
        [Path(raw) for raw in args.files]
        if args.files
        else sorted(DEFAULT_DIR.glob("*_analysis.json"))
    )
    if not paths:
        print(f"ERROR: no analysis files found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    failures: list[str] = []
    for path in paths:
        failures.extend(validate_analysis(path))

    if failures:
        print("\nLOOP1 DIAGNOSTIC VALIDATION FAILURES:", file=sys.stderr)
        for failure in failures:
            print(f"  x {failure}", file=sys.stderr)
        return 1

    print(f"\nPASS: Loop 1 diagnostics histograms validated for {len(paths)} analysis file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
