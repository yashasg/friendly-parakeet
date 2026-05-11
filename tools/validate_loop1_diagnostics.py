#!/usr/bin/env python3
"""Validate checked-in Loop 1 onset diagnostics.

This validator intentionally reads the active diagnostics artifacts in
``tools/diagnostics/*_loop1`` instead of regenerating levels through the legacy
beat-selection path.
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO_ROOT / "tools" / "diagnostics"
DEFAULT_BEATMAP_DIR = REPO_ROOT / "content" / "beatmaps"
KNOWN_SUBDIVISIONS = {"downbeat", "eighth", "triplet", "offgrid"}
PUBLIC_LAYERS = {"percussive", "harmonic", "full-spectrum"}


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def load_rows(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def diagnostics_dirs(paths: list[Path]) -> list[Path]:
    if paths:
        return paths
    return sorted(
        path
        for path in DEFAULT_DIR.glob("*_loop1")
        if (path / "snap_diagnostics_summary.json").exists()
        and (path / "onset_timing_events.csv").exists()
    )


def _beatmap_path_for_song(song: str, beatmap_dir: Path) -> Path:
    return beatmap_dir / f"{song}_beatmap.json"


def _validate_shipped_beatmap_counts(path: Path, song: str, summary: dict, rows: list[dict], beatmap_dir: Path) -> list[str]:
    beatmap_path = _beatmap_path_for_song(song, beatmap_dir)
    if not beatmap_path.exists():
        return []

    beatmap = load_json(beatmap_path)
    difficulties = beatmap.get("difficulties", {})
    shipped_counts = {
        difficulty: len(payload.get("beats", []))
        for difficulty, payload in difficulties.items()
        if isinstance(payload, dict)
    }
    csv_counts = Counter(row.get("difficulty", "") for row in rows)
    summary_counts = (
        summary.get("experimental_onset_timing", {})
        .get("obstacle_counts_by_difficulty", {})
    )

    errors: list[str] = []
    for difficulty, shipped_count in sorted(shipped_counts.items()):
        csv_count = csv_counts.get(difficulty, 0)
        summary_count = summary_counts.get(difficulty)
        if csv_count != shipped_count:
            errors.append(
                f"{song}: {path.name} CSV {difficulty} rows={csv_count} "
                f"but shipped beatmap has {shipped_count}"
            )
        if summary_count != shipped_count:
            errors.append(
                f"{song}: {path.name} summary {difficulty} count={summary_count} "
                f"but shipped beatmap has {shipped_count}"
            )

    unknown_csv_difficulties = sorted(set(csv_counts) - set(shipped_counts) - {""})
    if unknown_csv_difficulties:
        errors.append(f"{song}: CSV contains unknown difficulties {unknown_csv_difficulties}")

    return errors


def validate_diagnostics_dir(path: Path, beatmap_dir: Path = DEFAULT_BEATMAP_DIR) -> list[str]:
    errors: list[str] = []
    summary_path = path / "snap_diagnostics_summary.json"
    rows_path = path / "onset_timing_events.csv"
    if not summary_path.exists():
        return [f"{path}: missing snap_diagnostics_summary.json"]
    if not rows_path.exists():
        return [f"{path}: missing onset_timing_events.csv"]

    summary = load_json(summary_path)
    rows = load_rows(rows_path)
    song = str(summary.get("song_id") or path.name.replace("_loop1", ""))

    raw_per_pass = summary.get("onset_pool_summary", {}).get("raw_per_pass", {})
    if isinstance(raw_per_pass, dict):
        leaked = sorted(set(raw_per_pass) - PUBLIC_LAYERS)
        if leaked:
            errors.append(f"{song}: raw_per_pass leaks non-public layers {leaked}")

    if not rows:
        errors.append(f"{song}: onset_timing_events.csv is empty")
        return errors

    layer_hist = Counter(row.get("onset_class", "") for row in rows)
    subdivision_hist = Counter(row.get("subdivision", "") for row in rows)
    timing_sources = Counter(row.get("timing_source", "") for row in rows)

    unknown_layers = sorted(set(layer_hist) - PUBLIC_LAYERS)
    unknown_subdivisions = sorted(set(subdivision_hist) - KNOWN_SUBDIVISIONS)
    non_onset_sources = {key: value for key, value in timing_sources.items() if key != "onset"}

    if unknown_layers:
        errors.append(f"{song}: unknown onset_class bins {unknown_layers}")
    if unknown_subdivisions:
        errors.append(f"{song}: unknown subdivision bins {unknown_subdivisions}")
    if non_onset_sources:
        errors.append(f"{song}: non-onset timing sources {non_onset_sources}")
    errors.extend(_validate_shipped_beatmap_counts(path, song, summary, rows, beatmap_dir))

    print(
        f"{song:24s} rows={len(rows):4d} "
        f"layers={dict(sorted(layer_hist.items()))} "
        f"subdivisions={dict(sorted(subdivision_hist.items()))}"
    )
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path, help="Optional *_loop1 diagnostics directories")
    parser.add_argument(
        "--beatmap-dir",
        type=Path,
        default=DEFAULT_BEATMAP_DIR,
        help="Directory containing shipped *_beatmap.json files.",
    )
    args = parser.parse_args(argv)

    paths = diagnostics_dirs(args.paths)
    if not paths:
        print(f"ERROR: no *_loop1 diagnostics directories found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    failures: list[str] = []
    for path in paths:
        failures.extend(validate_diagnostics_dir(path, args.beatmap_dir))

    if failures:
        print("\nLOOP1 DIAGNOSTIC VALIDATION FAILURES:", file=sys.stderr)
        for failure in failures:
            print(f"  x {failure}", file=sys.stderr)
        return 1

    print(f"\nPASS: Loop 1 checked-in diagnostics validated for {len(paths)} directory(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
