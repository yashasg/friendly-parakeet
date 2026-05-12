#!/usr/bin/env python3
"""Validate maximum beat gap per difficulty level.

Issue #138: Shipped songs contain 56-64 beat silent gaps.

This validator checks the longest mid-song gap between authored required
obstacles. Non-blocking `onset_marker` rows preserve analysis metadata but do
not spawn at runtime, so they are ignored for playable-density checks.
Leading intro rest and trailing outro rest are capped separately.

Two timing models are supported:

1. Beat-grid timing (legacy): `beat` is the index into the analysis beat grid;
   gaps are measured as consecutive missing beat indices and capped per
   difficulty in beat units.

2. Onset-only timing (issue #447 / PR #427): `beat` is a sequential ordinal
   across selected onsets, not a musical-beat number, so a beat-ordinal cap
   is meaningless. When every authored beat in a difficulty has
   `timing_source == "onset"`, gaps are measured in seconds (using
   `time_sec`) and compared against the per-difficulty time-equivalent cap
   derived from the original beat cap and the song BPM.

Per-difficulty beat-grid limits (used when timing is beat-aligned):
  EASY:   max gap <= 40 beats
  MEDIUM: max gap <= 33 beats
  HARD:   max gap <= 32 beats

Under onset-only timing, the same caps are translated to seconds using the
declared BPM: limit_seconds = limit_beats * 60 / bpm.

Exit codes:
  0 - all beatmaps pass
  1 - one or more violations found

Usage:
    python3 tools/validate_max_beat_gap.py
    python3 tools/validate_max_beat_gap.py content/beatmaps/1_stomper_beatmap.json
"""

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO / "content" / "beatmaps"

# Maximum silent gap (in beats) per difficulty.
# These thresholds allow intentional breathing room (section transitions) while
# catching excessive gaps that indicate generation issues.
#
# Issue #138: "shipped songs contain 56-64 beat silent gaps" - we flag issues
# at these high levels to catch regressions, but allow somewhat higher thresholds
# than absolute minimum to account for song structure (intros, outros, transitions).
#
# Guidelines per BPM:
#   At 120 BPM: 33 beats = 16.5sec, 40 beats = 20sec
#   At 160 BPM: 33 beats = 12.4sec, 40 beats = 15sec
#
# This is more about catching *unintentional* gaps from generation bugs rather
# than strictly limiting longest comfortable silence.
MAX_GAP = {
    "easy":   40,    # allow breathing room for learning
    "medium": 33,    # slightly less for ramp difficulty
    "hard":   32,    # slightly more density expected
}

# Issue #527 — bound the lead-in (silence from t=0 to first authored
# obstacle) and trail-out (silence from last authored obstacle to
# duration_sec) per difficulty.  ``find_max_time_gap_sec`` only measures
# inter-obstacle gaps, so an entire dropped intro would otherwise pass.
# Caps mirror the design-docs guideline (intro ≥ 4 beats of rest BUT
# not more than a few seconds of silence on easy/medium).  Onset-only:
# the validator never invents a beat — it only flags that the generator
# silently dropped the leading/trailing real onsets that earlier
# revisions emitted.
MAX_LEAD_IN_SEC = {"easy": 8.0, "medium": 6.0, "hard": 4.0}
# Trailing-edge cap: songs frequently fade out for several seconds with no
# real onsets, so the trailing cap is wider than the lead-in cap (where the
# regression #527 was observed).  Mirrors the level designer's
# ``_fill_silent_gaps`` behaviour — no synthetic filler is ever inserted at
# the song tail; if the audio's last real onset leaves more than this much
# silence, the validator flags it as a regression vs. earlier rounds.
MAX_TRAIL_OUT_SEC = {"easy": 12.0, "medium": 10.0, "hard": 8.0}


def _is_valid_beat_index(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _collect_valid_beat_indices(beats: list[object]) -> tuple[set[int], int]:
    valid_beats: set[int] = set()
    invalid_rows = 0
    for row in beats:
        beat_index = row.get("beat") if isinstance(row, dict) else None
        if _is_valid_beat_index(beat_index):
            valid_beats.add(beat_index)
        else:
            invalid_rows += 1
    return valid_beats, invalid_rows


def _is_onset_marker_row(row: object) -> bool:
    return isinstance(row, dict) and row.get("kind") == "onset_marker"


def _required_action_rows(rows: list[object]) -> list[object]:
    return [row for row in rows if not _is_onset_marker_row(row)]


def find_max_gap(beats_in_beatmap: set[int]) -> int:
    """Return the longest mid-song run of consecutive beats with no obstacle."""
    if len(beats_in_beatmap) <= 1:
        return 0

    sorted_beats = sorted(beats_in_beatmap)
    max_gap = 0
    for prev_beat, next_beat in zip(sorted_beats, sorted_beats[1:]):
        max_gap = max(max_gap, next_beat - prev_beat - 1)
    return max_gap


def _all_onset_timed(rows: list[object]) -> bool:
    """True iff every dict row in `rows` carries timing_source == 'onset'."""
    saw_dict = False
    for row in rows:
        if not isinstance(row, dict):
            continue
        saw_dict = True
        if row.get("timing_source") != "onset":
            return False
    return saw_dict


def find_max_time_gap_sec(rows: list[object]) -> float:
    """Longest mid-song silent stretch (seconds) between consecutive obstacles.

    Uses the authored `time_sec` field; rows without a numeric time_sec are
    skipped so a single malformed row cannot collapse the gap measurement.
    """
    times: list[float] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        ts = row.get("time_sec")
        if isinstance(ts, (int, float)) and not isinstance(ts, bool):
            times.append(float(ts))
    if len(times) <= 1:
        return 0.0
    times.sort()
    longest = 0.0
    for prev_t, next_t in zip(times, times[1:]):
        gap = next_t - prev_t
        if gap > longest:
            longest = gap
    return longest


def _collect_times_sec(rows: list[object]) -> list[float]:
    times: list[float] = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        ts = row.get("time_sec")
        if isinstance(ts, (int, float)) and not isinstance(ts, bool):
            times.append(float(ts))
    times.sort()
    return times


def find_lead_in_sec(rows: list[object]) -> float:
    """Silence between t=0 and the first authored obstacle (seconds)."""
    times = _collect_times_sec(rows)
    return times[0] if times else 0.0


def find_trail_out_sec(rows: list[object], duration_sec: float | None) -> float:
    """Silence between the last authored obstacle and ``duration_sec`` (seconds)."""
    if duration_sec is None or duration_sec <= 0:
        return 0.0
    times = _collect_times_sec(rows)
    if not times:
        return 0.0
    return max(0.0, float(duration_sec) - times[-1])


def main(argv: list[str] | None = None) -> int:
    import argparse
    ap = argparse.ArgumentParser(description="Validate max beat gap per difficulty")
    ap.add_argument("files", nargs="*")
    ap.add_argument("--max-gap-easy", type=int, default=MAX_GAP["easy"])
    ap.add_argument("--max-gap-medium", type=int, default=MAX_GAP["medium"])
    ap.add_argument("--max-gap-hard", type=int, default=MAX_GAP["hard"])
    args = ap.parse_args(argv)

    max_gaps = {
        "easy": args.max_gap_easy,
        "medium": args.max_gap_medium,
        "hard": args.max_gap_hard,
    }

    paths = [Path(f) for f in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"ERROR: no beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    all_violations: list[str] = []

    for path in paths:
        with open(path) as f:
            beatmap = json.load(f)

        name = path.stem.replace("_beatmap", "")
        bpm = float(beatmap.get("bpm") or 0.0)
        duration_sec = beatmap.get("duration_sec")
        if isinstance(duration_sec, bool) or not isinstance(duration_sec, (int, float)):
            duration_sec = None

        for difficulty in ["easy", "medium", "hard"]:
            if difficulty not in beatmap.get("difficulties", {}):
                continue

            diff_data = beatmap["difficulties"][difficulty]
            raw_rows = diff_data.get("beats", []) or []
            required_rows = _required_action_rows(raw_rows)
            beats_in_beatmap, invalid_rows = _collect_valid_beat_indices(required_rows)
            if invalid_rows:
                print(
                    f"  {name} [{difficulty}]: skipped {invalid_rows} invalid beat row(s)",
                    file=sys.stderr,
                )

            if not beats_in_beatmap:
                continue

            # Issue #527 — lead-in and trail-out caps.  Always enforced
            # whenever any time_sec values are available; the limits do not
            # depend on the beat-grid vs onset-only timing model since the
            # "silence at the edges of the song" notion is purely temporal.
            lead_cap = MAX_LEAD_IN_SEC.get(difficulty)
            if lead_cap is not None:
                lead_in = find_lead_in_sec(required_rows)
                if lead_in > lead_cap:
                    all_violations.append(
                        f"{name} [{difficulty}]: lead-in {lead_in:.2f}s "
                        f"exceeds cap {lead_cap:.2f}s (#527)"
                    )
            trail_cap = MAX_TRAIL_OUT_SEC.get(difficulty)
            if trail_cap is not None and duration_sec is not None:
                trail_out = find_trail_out_sec(required_rows, duration_sec)
                if trail_out > trail_cap:
                    all_violations.append(
                        f"{name} [{difficulty}]: trail-out {trail_out:.2f}s "
                        f"exceeds cap {trail_cap:.2f}s (#527)"
                    )

            max_allowed_beats = max_gaps[difficulty]

            # Issue #452 — onset-only mode: validate gap in seconds rather than
            # beat ordinals (the latter is meaningless when `beat` is a
            # sequential onset index, not a musical-beat number).
            if bpm > 0 and _all_onset_timed(required_rows):
                max_allowed_sec = max_allowed_beats * 60.0 / bpm
                max_gap_sec = find_max_time_gap_sec(required_rows)
                if max_gap_sec > max_allowed_sec:
                    all_violations.append(
                        f"{name} [{difficulty}]: max silent gap {max_gap_sec:.1f}s "
                        f"exceeds onset-mode limit {max_allowed_sec:.1f}s "
                        f"({max_allowed_beats} beats @ {bpm:.1f} BPM)"
                    )
                continue

            max_gap = find_max_gap(beats_in_beatmap)
            if max_gap > max_allowed_beats:
                all_violations.append(
                    f"{name} [{difficulty}]: max gap {max_gap} beats "
                    f"exceeds limit {max_allowed_beats} beats"
                )

    if all_violations:
        print("MAX BEAT GAP VIOLATIONS (#138 / lead-in & trail-out #527):", file=sys.stderr)
        for v in all_violations:
            print(f"  x {v}", file=sys.stderr)
        return 1

    print(f"OK: max beat gap checks passed for {len(paths)} beatmap(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
