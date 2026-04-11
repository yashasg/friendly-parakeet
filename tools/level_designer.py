"""
level_designer.py
=================
Takes audio analysis JSON (from rhythm_pipeline.py) and produces a beatmap
JSON that the game engine can load.

Design philosophy: COMBO SEQUENCES
  - Obstacles are grouped into combos by action type
  - SHAPE COMBO: stay in one lane, change shapes on every beat
  - LANE COMBO: dodge lane_blocks on every beat
  - BAR COMBO: jump/slide (high intensity only)
  - Never interleave shape changes with lane changes on adjacent beats
  - Intensity from song structure drives density and combo length:
    LOW:    few beats mapped, short combos (2-3), long rests
    MEDIUM: more beats, medium combos (2-3), short rests
    HIGH:   all beats mapped, long combos (3-5), no rests

Usage:
    python level_designer.py analysis.json
    python level_designer.py analysis.json --output my_beatmap.json
    python level_designer.py analysis.json --difficulty easy
"""

import argparse
import json
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# DESIGN CONSTANTS
# ---------------------------------------------------------------------------

PASS_TO_SHAPE = {
    "kick":   "circle",
    "snare":  "square",
    "melody": "square",
    "hihat":  "triangle",
    "flux":   "square",
}

SHAPE_TO_LANE = {
    "circle":   0,
    "square":   1,
    "triangle": 2,
}

# Intensity-driven combo parameters per difficulty
# combo_len: (min, max) beats in a combo
# rest_beats: beats of silence between combos
# beat_usage: fraction of available beats that get obstacles (0.0-1.0)
INTENSITY_PARAMS = {
    "easy": {
        "low":    {"combo_len": (1, 2), "rest_beats": 4, "beat_usage": 0.20},
        "medium": {"combo_len": (2, 3), "rest_beats": 3, "beat_usage": 0.35},
        "high":   {"combo_len": (2, 3), "rest_beats": 2, "beat_usage": 0.50},
    },
    "medium": {
        "low":    {"combo_len": (2, 3), "rest_beats": 3, "beat_usage": 0.30},
        "medium": {"combo_len": (2, 3), "rest_beats": 1, "beat_usage": 0.55},
        "high":   {"combo_len": (3, 4), "rest_beats": 1, "beat_usage": 0.75},
    },
    "hard": {
        "low":    {"combo_len": (2, 3), "rest_beats": 2, "beat_usage": 0.50},
        "medium": {"combo_len": (3, 4), "rest_beats": 1, "beat_usage": 0.80},
        "high":   {"combo_len": (4, 6), "rest_beats": 0, "beat_usage": 1.00},
    },
}

DIFFICULTY_CONFIG = {
    "easy": {
        "intro_rest_beats": 8,
        "allowed_kinds": ["shape_gate"],
        "flux_percentile": 80,
    },
    "medium": {
        "intro_rest_beats": 4,
        "allowed_kinds": ["shape_gate", "lane_block"],
        "flux_percentile": 55,
    },
    "hard": {
        "intro_rest_beats": 2,
        "allowed_kinds": ["shape_gate", "lane_block", "low_bar", "high_bar"],
        "flux_percentile": 30,
    },
}


# ---------------------------------------------------------------------------
# BEAT GRID SNAPPING
# ---------------------------------------------------------------------------

def snap_events_to_beats(events: list[dict], beats: list[float],
                         tolerance: float = 0.08) -> list[dict]:
    snapped = []
    beat_set = set()
    for ev in events:
        t = ev["t"]
        best_idx = None
        best_dist = tolerance + 1
        lo, hi = 0, len(beats) - 1
        while lo <= hi:
            mid = (lo + hi) // 2
            dist = abs(beats[mid] - t)
            if dist < best_dist:
                best_dist = dist
                best_idx = mid
            if beats[mid] < t:
                lo = mid + 1
            else:
                hi = mid - 1
        if best_idx is not None and best_dist <= tolerance:
            if best_idx not in beat_set:
                beat_set.add(best_idx)
                snapped.append({**ev, "beat_idx": best_idx, "beat_time": beats[best_idx]})
    snapped.sort(key=lambda e: e["beat_idx"])
    return snapped


# ---------------------------------------------------------------------------
# SHAPE / LANE HELPERS
# ---------------------------------------------------------------------------

def pick_shape(passes: list[str], beat_idx: int) -> str:
    candidates = []
    for p in passes:
        if p in PASS_TO_SHAPE:
            candidates.append(PASS_TO_SHAPE[p])
    if not candidates:
        candidates = ["circle"]
    unique = list(dict.fromkeys(candidates))
    return unique[beat_idx % len(unique)]


def get_section_at(t: float, structure: list[dict]) -> dict:
    for sec in structure:
        if sec["start"] <= t < sec["end"]:
            return sec
    return structure[-1] if structure else {"section": "verse", "intensity": "medium"}


# ---------------------------------------------------------------------------
# COMBO SEQUENCE BUILDER
#
# The core algorithm:
# 1. Walk through the song section by section
# 2. For each section, determine combo parameters from intensity
# 3. Alternate: SHAPE COMBO → rest → LANE COMBO → rest → SHAPE COMBO → ...
# 4. Within each combo, place obstacles on consecutive beats
# 5. Between combos, leave rest beats (breathing room)
# ---------------------------------------------------------------------------

def build_section_combos(beat_indices: list[int], section: dict,
                         difficulty: str, allowed_kinds: list[str],
                         event_map: dict, current_lane: int) -> tuple[list[dict], int]:
    """
    Build combo sequences for one song section.
    Returns (obstacles, updated_lane).
    """
    intensity = section.get("intensity", "medium")
    params = INTENSITY_PARAMS[difficulty][intensity]
    combo_min, combo_max = params["combo_len"]
    rest = params["rest_beats"]
    usage = params["beat_usage"]

    if not beat_indices:
        return [], current_lane

    # Select which beats to use based on usage fraction
    # Prefer beats that have strong events (in event_map)
    total_available = len(beat_indices)
    target_count = max(1, int(total_available * usage))

    # Prioritize beats with events, then fill evenly
    with_events = [bi for bi in beat_indices if bi in event_map]
    without_events = [bi for bi in beat_indices if bi not in event_map]
    selected_beats = with_events[:target_count]
    if len(selected_beats) < target_count:
        selected_beats += without_events[:target_count - len(selected_beats)]
    selected_beats.sort()

    if not selected_beats:
        return [], current_lane

    obstacles = []
    shapes = ["circle", "square", "triangle"]
    combo_type = "shape"  # alternate: shape → lane → shape → ...
    cursor = 0

    while cursor < len(selected_beats):
        # Determine combo length for this combo
        remaining = len(selected_beats) - cursor
        combo_len = min(combo_max, remaining)
        combo_len = max(combo_min, combo_len)
        combo_len = min(combo_len, remaining)

        combo_beats = selected_beats[cursor:cursor + combo_len]

        if combo_type == "shape":
            # ── SHAPE COMBO: stay in one lane, change shapes ──────
            ev0 = event_map.get(combo_beats[0])
            if ev0:
                natural_shape = pick_shape(ev0.get("passes", ["kick"]), combo_beats[0])
                natural_lane = SHAPE_TO_LANE[natural_shape]
                if abs(natural_lane - current_lane) <= 1:
                    current_lane = natural_lane
            combo_lane = current_lane

            last_shape = None
            for bi in combo_beats:
                ev = event_map.get(bi)
                if ev:
                    shape = pick_shape(ev.get("passes", ["kick"]), bi)
                else:
                    shape = shapes[bi % len(shapes)]
                # Prevent same shape twice in a row
                if shape == last_shape:
                    others = [s for s in shapes if s != shape]
                    shape = others[bi % len(others)]
                obstacles.append({
                    "beat": bi,
                    "kind": "shape_gate",
                    "shape": shape,
                    "lane": combo_lane,
                })
                last_shape = shape

        elif combo_type == "lane" and "lane_block" in allowed_kinds:
            # ── LANE COMBO: dodge lane_blocks ─────────────────────
            # Lane changes need minimum gap=2 between beats (0.82s)
            # to give the player time to swipe + transition.
            # Filter combo_beats to enforce min gap=2.
            spaced_beats = [combo_beats[0]]
            for bi in combo_beats[1:]:
                if bi - spaced_beats[-1] >= 2:
                    spaced_beats.append(bi)
            lane_seq = _make_lane_sequence(current_lane, len(spaced_beats))
            for k, bi in enumerate(spaced_beats):
                free_lane = lane_seq[k]
                blocked = [l for l in [0, 1, 2] if l != free_lane]
                obstacles.append({
                    "beat": bi,
                    "kind": "lane_block",
                    "blocked": blocked,
                })
                current_lane = free_lane

        elif combo_type == "bar" and ("low_bar" in allowed_kinds or "high_bar" in allowed_kinds):
            # ── BAR COMBO: jump/slide ─────────────────────────────
            for bi in combo_beats:
                kind = "low_bar" if bi % 2 == 0 else "high_bar"
                if kind not in allowed_kinds:
                    kind = "low_bar" if "low_bar" in allowed_kinds else "high_bar"
                obstacles.append({"beat": bi, "kind": kind})

        else:
            # Fallback: shape combo if lane/bar not allowed
            combo_lane = current_lane
            last_shape = None
            for bi in combo_beats:
                shape = shapes[bi % len(shapes)]
                if shape == last_shape:
                    others = [s for s in shapes if s != shape]
                    shape = others[bi % len(others)]
                obstacles.append({
                    "beat": bi,
                    "kind": "shape_gate",
                    "shape": shape,
                    "lane": combo_lane,
                })
                last_shape = shape

        cursor += combo_len

        # Skip rest beats after combo
        if rest > 0 and cursor < len(selected_beats):
            # Find the next beat that's at least 'rest' beats after the last combo beat
            last_combo_beat = combo_beats[-1]
            while cursor < len(selected_beats) and selected_beats[cursor] - last_combo_beat < rest + 1:
                cursor += 1

        # Alternate combo type
        if combo_type == "shape":
            combo_type = "lane"
        elif combo_type == "lane":
            # In high intensity, add bar combos occasionally
            if intensity == "high" and ("low_bar" in allowed_kinds or "high_bar" in allowed_kinds):
                combo_type = "bar"
            else:
                combo_type = "shape"
        else:
            combo_type = "shape"

    return obstacles, current_lane


def _make_lane_sequence(start_lane: int, count: int) -> list[int]:
    """Adjacent-only lane sequence. Bounces off edges."""
    lanes = [start_lane]
    direction = 1
    for _ in range(count - 1):
        next_lane = lanes[-1] + direction
        if next_lane < 0 or next_lane > 2:
            direction = -direction
            next_lane = lanes[-1] + direction
        lanes.append(next_lane)
    return lanes


# ---------------------------------------------------------------------------
# ANTI-REPETITION
# ---------------------------------------------------------------------------

def apply_variety(obstacles: list[dict]) -> list[dict]:
    shapes = ["circle", "square", "triangle"]

    # Prevent 2-lane jumps
    prev_lane = 1
    for obs in obstacles:
        if obs["kind"] != "shape_gate":
            continue
        lane = obs.get("lane", 1)
        if abs(lane - prev_lane) == 2:
            obs["lane"] = 1
            obs["shape"] = "square"
        prev_lane = obs.get("lane", prev_lane)

    # Prevent 3+ same shape in a row
    for i in range(len(obstacles)):
        obs = obstacles[i]
        if obs["kind"] != "shape_gate":
            continue
        if i >= 2:
            prev_shapes = [obstacles[j].get("shape") for j in range(i-2, i)
                           if obstacles[j]["kind"] == "shape_gate"]
            if len(prev_shapes) == 2 and all(s == obs["shape"] for s in prev_shapes):
                others = [s for s in shapes if s != obs["shape"]]
                obs["shape"] = others[i % len(others)]
                obs["lane"] = SHAPE_TO_LANE[obs["shape"]]

    return obstacles


# ---------------------------------------------------------------------------
# MAIN LEVEL DESIGN PIPELINE
# ---------------------------------------------------------------------------

def design_level(analysis: dict, difficulty: str) -> list[dict]:
    cfg = DIFFICULTY_CONFIG[difficulty]
    beats = analysis["beats"]
    events = analysis["events"]
    structure = analysis["structure"]

    # Filter events by flux threshold
    pct = cfg["flux_percentile"]
    stats = analysis["flux_stats"]
    breakpoints = [(0, stats["min"]), (25, stats["p25"]), (50, stats["p50"]),
                   (75, stats["p75"]), (90, stats["p90"]), (100, stats["max"])]
    flux_threshold = stats["p50"]
    for i in range(len(breakpoints) - 1):
        p_lo, v_lo = breakpoints[i]
        p_hi, v_hi = breakpoints[i + 1]
        if p_lo <= pct <= p_hi:
            frac = (pct - p_lo) / (p_hi - p_lo) if p_hi > p_lo else 0
            flux_threshold = v_lo + frac * (v_hi - v_lo)
            break

    strong_events = [e for e in events if e["flux"] >= flux_threshold]

    # Snap to beat grid
    on_beat = snap_events_to_beats(strong_events, beats, tolerance=0.08)
    all_on_beat = snap_events_to_beats(events, beats, tolerance=0.08)
    event_map = {ev["beat_idx"]: ev for ev in all_on_beat}

    # Build beat index range for each section
    intro_end = cfg["intro_rest_beats"]
    all_obstacles = []
    current_lane = 1  # start center

    for section in structure:
        sec_start = section["start"]
        sec_end = section["end"]

        # Find beats within this section
        section_beats = []
        for i, bt in enumerate(beats):
            if bt >= sec_start and bt < sec_end and i >= intro_end:
                section_beats.append(i)

        if not section_beats:
            continue

        # Build combos for this section
        obs, current_lane = build_section_combos(
            section_beats, section, difficulty,
            cfg["allowed_kinds"], event_map, current_lane)
        all_obstacles.extend(obs)

    # Sort and deduplicate
    all_obstacles.sort(key=lambda o: o["beat"])
    seen = set()
    deduped = []
    for obs in all_obstacles:
        if obs["beat"] not in seen:
            seen.add(obs["beat"])
            deduped.append(obs)

    # Apply variety rules
    deduped = apply_variety(deduped)
    return deduped


def build_beatmap(analysis: dict, difficulties: list[str]) -> dict:
    beats = analysis["beats"]
    offset = beats[0] if beats else 0.0

    diff_data = {}
    for diff in difficulties:
        obstacles = design_level(analysis, diff)
        diff_data[diff] = {"beats": obstacles, "count": len(obstacles)}

    return {
        "song_id": analysis["title"],
        "title": analysis["title"],
        "bpm": analysis["bpm"],
        "offset": round(offset, 3),
        "lead_beats": 4,
        "duration_sec": analysis["duration"],
        "difficulties": diff_data,
        "structure": analysis["structure"],
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Level designer — converts audio analysis into a playable beatmap"
    )
    parser.add_argument("input", help="Path to analysis JSON (from rhythm_pipeline.py)")
    parser.add_argument("--output", "-o", default=None,
                        help="Output beatmap JSON path (default: <song_id>_beatmap.json)")
    parser.add_argument("--difficulty", "-d", choices=["easy", "medium", "hard", "all"],
                        default="all", help="Which difficulty to generate (default: all)")
    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    with open(args.input) as f:
        analysis = json.load(f)

    print("=" * 60)
    print("  LEVEL DESIGNER (combo-based)")
    print(f"  Song: {analysis['title']}  BPM: {analysis['bpm']}")
    print("=" * 60)

    if args.difficulty == "all":
        diffs = ["easy", "medium", "hard"]
    else:
        diffs = [args.difficulty]

    beatmap = build_beatmap(analysis, diffs)

    for diff in diffs:
        data = beatmap["difficulties"][diff]
        kinds = {}
        for obs in data["beats"]:
            kinds[obs["kind"]] = kinds.get(obs["kind"], 0) + 1
        print(f"\n  {diff.upper()}: {data['count']} obstacles")
        for k, v in sorted(kinds.items()):
            print(f"    {k:15s}: {v}")

    out_path = args.output or f"{analysis['title']}_beatmap.json"
    with open(out_path, "w") as f:
        json.dump(beatmap, f, indent=2)

    print(f"\n✓ {out_path}")
    print("=" * 60)


if __name__ == "__main__":
    main()
