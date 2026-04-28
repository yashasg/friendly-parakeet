"""
validate_beatmap_offset.py
==========================
Validates that each shipped beatmap's `offset` field is correctly anchored
to its first authored beat, not to an unreliable early detection.

Semantics enforced:
    offset == beats[anchor_idx] - anchor_idx * beat_period  (within tolerance)

where anchor_idx is the minimum beat_index across all authored obstacles
(all difficulties).  This guarantees the runtime formula

    arrival_time = offset + beat_index * beat_period

is phase-aligned to real musical content (see beat_map.h and GitHub #137).

Usage:
    python tools/validate_beatmap_offset.py
    python tools/validate_beatmap_offset.py --tolerance-ms 2.0
"""

import argparse
import json
import sys
from pathlib import Path


BEATMAP_DIR = Path("content/beatmaps")
TOLERANCE_MS_DEFAULT = 2.0  # acceptable rounding error in milliseconds


def validate_beatmap_pair(beatmap_path: Path, analysis_path: Path,
                          tolerance_ms: float) -> list[str]:
    """Return a list of error strings (empty = pass)."""
    errors = []

    beatmap = json.loads(beatmap_path.read_text())
    analysis = json.loads(analysis_path.read_text())

    beats = analysis.get("beats", [])
    bpm = beatmap.get("bpm", 0)
    offset = beatmap.get("offset", 0.0)
    difficulties = beatmap.get("difficulties", {})

    if not beats:
        errors.append(f"{beatmap_path.name}: analysis has no beats")
        return errors
    if bpm <= 0:
        errors.append(f"{beatmap_path.name}: bpm <= 0")
        return errors

    beat_period = 60.0 / bpm

    # Collect all authored beat indices across all difficulties.
    all_authored = [
        o["beat"]
        for d in difficulties.values()
        for o in d.get("beats", [])
    ]
    if not all_authored:
        errors.append(f"{beatmap_path.name}: no authored beats in any difficulty")
        return errors

    anchor_idx = min(all_authored)
    if anchor_idx >= len(beats):
        errors.append(
            f"{beatmap_path.name}: anchor_idx={anchor_idx} out of range "
            f"(beats has {len(beats)} entries)"
        )
        return errors

    anchor_time = beats[anchor_idx]
    expected_offset = anchor_time - anchor_idx * beat_period
    delta_ms = abs(offset - expected_offset) * 1000.0

    if delta_ms > tolerance_ms:
        errors.append(
            f"{beatmap_path.name}: offset={offset:.4f}s but expected "
            f"{expected_offset:.4f}s (anchored to beat_idx={anchor_idx} "
            f"at {anchor_time:.4f}s).  Delta={delta_ms:.2f}ms > "
            f"tolerance={tolerance_ms}ms. "
            f"(beats[0]={beats[0]:.4f}s for comparison)"
        )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tolerance-ms", type=float, default=TOLERANCE_MS_DEFAULT,
                        help=f"Max allowed offset error in ms (default: {TOLERANCE_MS_DEFAULT})")
    args = parser.parse_args()

    beatmap_dir = BEATMAP_DIR
    if not beatmap_dir.is_dir():
        print(f"ERROR: {beatmap_dir} not found. Run from repo root.", file=sys.stderr)
        return 1

    pairs: list[tuple[Path, Path]] = []
    for bm in sorted(beatmap_dir.glob("*_beatmap.json")):
        stem = bm.stem.replace("_beatmap", "")
        analysis = beatmap_dir / f"{stem}_analysis.json"
        if analysis.is_file():
            pairs.append((bm, analysis))
        else:
            print(f"WARNING: no analysis file for {bm.name}, skipping", file=sys.stderr)

    if not pairs:
        print("ERROR: no beatmap/analysis pairs found", file=sys.stderr)
        return 1

    all_errors: list[str] = []
    for bm, an in pairs:
        all_errors.extend(validate_beatmap_pair(bm, an, args.tolerance_ms))

    if all_errors:
        for e in all_errors:
            print(f"FAIL: {e}")
        return 1

    print(f"PASS: all {len(pairs)} beatmap(s) have correctly anchored offsets "
          f"(tolerance={args.tolerance_ms}ms)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
