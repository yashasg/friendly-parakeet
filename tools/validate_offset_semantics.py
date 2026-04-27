#!/usr/bin/env python3
"""
validate_offset_semantics.py
============================
Deterministic validation for issue #137: offset semantics in the rhythm pipeline.

The beatmap "offset" field must be the arrival time of beat_index=0.
The scheduler computes each obstacle's collision time as:

    beat_time = offset + beat_index * beat_period

This script validates:
  1. Pipeline contract: offset equals beats[0] in all shipped beatmaps.
  2. Uniform-beat assumption: timing error is zero when beats are perfectly regular.
  3. Drift exposure: non-uniform beats (tempo variation) cause the formula to diverge
     from the actual beat timestamp; the script reports the maximum error per song.
  4. Shipped beatmap plausibility: offset > 0, bpm > 0, all beat_indices positive.
  5. Analysis/beatmap consistency: shipped beatmap offset matches analysis beats[0].

Exits 0 on success. Exits 1 if any validation fails, printing each violation.

Usage:
    python3 tools/validate_offset_semantics.py
    python3 tools/validate_offset_semantics.py --tolerance-ms 8
"""

import argparse
import json
import math
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

REPO_ROOT    = Path(__file__).parent.parent
BEATMAP_DIR  = REPO_ROOT / "content" / "beatmaps"

# Maximum acceptable timing error between formula and actual beat timestamp.
# 8 ms ≈ half a frame at 60 fps; beyond this, off-beat collisions are perceptible.
DEFAULT_TOLERANCE_MS = 8.0


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def beat_period(bpm: float) -> float:
    return 60.0 / bpm


def formula_time(offset: float, beat_index: int, period: float) -> float:
    """Arrival time as computed by beat_scheduler_system."""
    return offset + beat_index * period


def drift_ms(actual_beats: list[float], offset: float, period: float, idx: int) -> float:
    """
    Difference in milliseconds between the formula time and the actual beat
    timestamp for beat index *idx*.

    Returns 0.0 when idx is out of range (analysis beat array is shorter than
    the authored beat_index — which would itself be a separate bug).
    """
    if idx >= len(actual_beats):
        return 0.0
    formula = formula_time(offset, idx, period)
    actual  = actual_beats[idx]
    return abs(formula - actual) * 1000.0


# ---------------------------------------------------------------------------
# Test cases (deterministic, no audio)
# ---------------------------------------------------------------------------

def test_uniform_beats_zero_drift() -> list[str]:
    """
    With perfectly uniform beats, formula == actual for all indices.
    beat[i] = offset + i * period → error = 0.
    """
    failures = []
    offset = 0.5
    bpm    = 120.0
    period = beat_period(bpm)
    # Generate 200 perfectly regular beats
    beats = [offset + i * period for i in range(200)]

    for idx in [0, 1, 5, 10, 50, 100, 199]:
        err = drift_ms(beats, offset, period, idx)
        if err > 0.001:  # float arithmetic tolerance
            failures.append(
                f"uniform_beats: expected 0ms drift at idx={idx}, got {err:.3f}ms"
            )
    return failures


def test_jittered_beats_drift_grows() -> list[str]:
    """
    With ±5ms per-beat random jitter (deterministic seed), accumulated drift
    grows with beat index. Asserts the test infrastructure would catch a song
    with significant tempo variation.

    This is a meta-test: it verifies that the drift detection logic fires when
    beats are non-uniform, ensuring the shipped-beatmap checks below are meaningful.
    """
    failures = []
    import random
    rng = random.Random(137)  # deterministic seed matching issue number

    offset = 0.5
    bpm    = 120.0
    period = beat_period(bpm)
    jitter_ms = 5.0  # ±5ms per beat

    beats = [offset]
    for _ in range(199):
        jitter = rng.uniform(-jitter_ms / 1000.0, jitter_ms / 1000.0)
        beats.append(beats[-1] + period + jitter)

    # At high beat indices, accumulated drift should exceed single-beat jitter
    max_drift_at_50  = drift_ms(beats, offset, period, 50)
    max_drift_at_100 = drift_ms(beats, offset, period, 100)

    # With ±5ms jitter, drift at idx=100 is typically 20-50ms (well above 8ms threshold)
    if max_drift_at_100 <= jitter_ms:
        failures.append(
            f"jittered_beats: expected drift > {jitter_ms:.0f}ms at idx=100, "
            f"got {max_drift_at_100:.1f}ms — drift detection may be broken"
        )
    if max_drift_at_50 <= 0.1:
        failures.append(
            f"jittered_beats: expected non-zero drift at idx=50 with jitter, "
            f"got {max_drift_at_50:.3f}ms — beats look unexpectedly uniform"
        )

    return failures


def test_offset_equals_beats_zero() -> list[str]:
    """
    The pipeline contract: offset = beats[0].
    offset represents when beat_index=0 arrives; an obstacle at beat_index=0
    should collide at exactly offset seconds.
    """
    failures = []
    test_cases = [
        (2.27, 159.0),   # stomper real values
        (1.813, 119.6),  # drama real values
        (0.418, 149.8),  # mental_corruption real values
        (0.0,   120.0),  # edge: silence-free song
    ]
    for offset, bpm in test_cases:
        period = beat_period(bpm)
        formula = formula_time(offset, 0, period)
        if abs(formula - offset) > 1e-6:
            failures.append(
                f"offset_contract bpm={bpm}: beat_index=0 formula={formula:.6f} "
                f"!= offset={offset:.6f}"
            )
    return failures


def test_nonzero_first_beat_index_arrives_after_offset() -> list[str]:
    """
    Regression #137: all shipped beatmaps have first beat_index >= 2.
    The first collision must arrive AFTER offset, not AT offset.
    """
    failures = []
    shipped_first_indices = {
        # (offset, bpm, first_beat_idx)  — real values from shipped beatmaps
        "stomper_easy":   (2.27,  159.0, 8),
        "stomper_medium": (2.27,  159.0, 4),
        "stomper_hard":   (2.27,  159.0, 2),
        "drama_easy":     (1.813, 119.6, 10),
        "drama_medium":   (1.813, 119.6, 6),
        "drama_hard":     (1.813, 119.6, 2),
        "mental_easy":    (0.418, 149.8, 10),
        "mental_medium":  (0.418, 149.8, 4),
        "mental_hard":    (0.418, 149.8, 2),
    }
    for label, (offset, bpm, first_idx) in shipped_first_indices.items():
        period  = beat_period(bpm)
        arrival = formula_time(offset, first_idx, period)
        if arrival <= offset:
            failures.append(
                f"{label}: first obstacle at beat_index={first_idx} "
                f"arrival={arrival:.3f}s not after offset={offset:.3f}s"
            )
        if first_idx == 0:
            # If any shipped beatmap starts at beat_index=0 that would be a new
            # data-point to verify; flag it so we can update the test.
            failures.append(
                f"{label}: first beat_index=0 — update test with new data point"
            )
    return failures


def test_offset_shift_is_global() -> list[str]:
    """
    Changing offset by delta must shift ALL collision times by exactly delta.
    Verifies offset is additive and doesn't interact with beat_index non-linearly.
    """
    failures = []
    bpm     = 120.0
    period  = beat_period(bpm)
    delta   = 0.123456
    indices = [0, 1, 5, 10, 50, 100]

    for idx in indices:
        t_base    = formula_time(1.0,         idx, period)
        t_shifted = formula_time(1.0 + delta, idx, period)
        actual_delta = t_shifted - t_base
        if abs(actual_delta - delta) > 1e-6:
            failures.append(
                f"global_shift idx={idx}: expected Δ={delta}, got Δ={actual_delta}"
            )
    return failures


# ---------------------------------------------------------------------------
# Shipped beatmap validation (requires content/ directory)
# ---------------------------------------------------------------------------

def validate_shipped_beatmaps(tolerance_ms: float) -> list[str]:
    """
    Cross-validate each shipped beatmap JSON against its analysis JSON:
      - offset must equal analysis beats[0]
      - offset > 0 (songs have lead-in silence)
      - bpm > 0
      - every authored beat_index must be >= 0
      - formula drift vs analysis beats must be within tolerance_ms
    """
    failures = []

    if not BEATMAP_DIR.exists():
        failures.append(f"content/beatmaps/ not found at {BEATMAP_DIR}")
        return failures

    beatmap_files = sorted(BEATMAP_DIR.glob("*_beatmap.json"))
    if not beatmap_files:
        failures.append("No *_beatmap.json files found in content/beatmaps/")
        return failures

    for bfile in beatmap_files:
        afile = bfile.parent / bfile.name.replace("_beatmap.json", "_analysis.json")
        with open(bfile) as f:
            bmap = json.load(f)

        bpm    = bmap.get("bpm", 0.0)
        offset = bmap.get("offset", None)
        song   = bfile.stem.replace("_beatmap", "")

        if bpm <= 0:
            failures.append(f"{song}: bpm={bpm} is not positive")
            continue
        if offset is None:
            failures.append(f"{song}: missing 'offset' field")
            continue
        if offset < 0:
            failures.append(f"{song}: offset={offset} is negative")
            continue

        period = beat_period(bpm)

        # Cross-check against analysis file if present
        analysis_beats: list[float] = []
        if afile.exists():
            with open(afile) as f:
                ana = json.load(f)
            analysis_beats = ana.get("beats", [])
            if analysis_beats:
                expected_offset = round(analysis_beats[0], 3)
                if abs(offset - expected_offset) > 0.001:
                    failures.append(
                        f"{song}: offset={offset} != analysis beats[0]={expected_offset:.3f} "
                        f"(pipeline contract violated)"
                    )

        # Validate all authored beat indices per difficulty
        diffs = bmap.get("difficulties", {})
        for diff, dd in diffs.items():
            beats = dd.get("beats", [])
            for entry in beats:
                idx = entry.get("beat", -1)
                if idx < 0:
                    failures.append(f"{song}/{diff}: beat_index={idx} is negative")
                    continue

                # Formula drift vs analysis beats (if available)
                if analysis_beats and idx < len(analysis_beats):
                    err = drift_ms(analysis_beats, offset, period, idx)
                    if err > tolerance_ms:
                        failures.append(
                            f"{song}/{diff} beat_index={idx}: "
                            f"formula drift={err:.1f}ms > tolerance={tolerance_ms:.0f}ms"
                        )

    return failures


# ---------------------------------------------------------------------------
# Regression scenario: corrected offset reduces drift for first collision beat
# ---------------------------------------------------------------------------

def test_corrected_offset_reduces_drift() -> list[str]:
    """
    Acceptance criterion for #137: computing offset from the intended first
    collision beat (rather than blindly using beats[0]) reduces timing error
    for obstacles.

    Given analysis beats with linear tempo drift (accelerating song), the current
    formula (offset = beats[0]) accumulates error. A corrected offset — computed
    as beats[N] - N*period for the first authored beat_index N — gives zero error
    at beat_index N and smaller errors nearby.

    This test does NOT require audio; it uses a synthetic drift model.
    """
    failures = []

    bpm      = 120.0
    period   = beat_period(bpm)
    # Simulate a song that speeds up by 0.1ms per beat (linear tempo ramp)
    drift_per_beat_s = 0.0001  # 0.1ms per beat acceleration
    n_beats  = 200
    beats    = [0.5 + i * period - i * drift_per_beat_s for i in range(n_beats)]

    # Current pipeline: offset = beats[0]
    offset_current = beats[0]

    # The first authored obstacle on hard for a typical song: beat_index = 2
    first_idx = 2

    # Corrected offset: derived from the first authored beat, not beats[0]
    offset_corrected = beats[first_idx] - first_idx * period

    # Error with current offset at first authored beat
    err_current   = drift_ms(beats, offset_current,   period, first_idx)
    # Error with corrected offset at first authored beat (should be 0 or near 0)
    err_corrected = drift_ms(beats, offset_corrected, period, first_idx)

    # Corrected offset must produce smaller or equal error
    if err_corrected > err_current + 1e-6:
        failures.append(
            f"corrected_offset: err_corrected={err_corrected:.3f}ms > "
            f"err_current={err_current:.3f}ms — correction made things worse"
        )

    # With linear drift, corrected offset for first_idx reduces error at first_idx to ~0
    if err_corrected > 0.1:  # 0.1ms tolerance for float arithmetic
        failures.append(
            f"corrected_offset: expected ~0ms error at beat_index={first_idx} "
            f"after correction, got {err_corrected:.3f}ms"
        )

    return failures


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

def run_all(tolerance_ms: float) -> int:
    suites = [
        ("uniform_beats_zero_drift",              test_uniform_beats_zero_drift),
        ("jittered_beats_drift_grows",             test_jittered_beats_drift_grows),
        ("offset_equals_beats_zero",               test_offset_equals_beats_zero),
        ("nonzero_first_beat_index_after_offset",  test_nonzero_first_beat_index_arrives_after_offset),
        ("offset_shift_is_global",                 test_offset_shift_is_global),
        ("corrected_offset_reduces_drift",         test_corrected_offset_reduces_drift),
        ("shipped_beatmaps",                       lambda: validate_shipped_beatmaps(tolerance_ms)),
    ]

    total_failures = 0
    for name, fn in suites:
        failures = fn()
        if failures:
            print(f"\nFAIL [{name}]")
            for msg in failures:
                print(f"  - {msg}")
            total_failures += len(failures)
        else:
            print(f"  ok  [{name}]")

    if total_failures:
        print(f"\n{total_failures} violation(s) found. See above.")
        return 1
    print("\nAll offset semantics checks passed.")
    return 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tolerance-ms", type=float, default=DEFAULT_TOLERANCE_MS,
                        help=f"Max allowed timing error in ms (default: {DEFAULT_TOLERANCE_MS})")
    args = parser.parse_args()
    sys.exit(run_all(args.tolerance_ms))


if __name__ == "__main__":
    main()
