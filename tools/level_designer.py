"""
level_designer.py — Combo-based beatmap generator for SHAPESHIFTER.

Pipeline:
  1. Load analysis JSON (from rhythm_pipeline.py)
  2. Filter events by flux threshold (difficulty-dependent)
  3. Snap events to beat grid
  4. Walk song structure section-by-section
  5. For each section, build combo sequences:
     SHAPE COMBO → [transition beat] → LANE COMBO → [transition beat] → ...
  6. Fill long gaps to keep player engaged
  7. Enforce transition beats between combo types
  8. Apply anti-repetition rules

Design rules:
  - Obstacles grouped into combos (shape / lane / bar)
  - Never interleave shape presses with lane changes on adjacent beats
  - Always skip ≥1 beat when switching combo types
  - Lane combos spaced ≥2 beats apart (human swipe timing)
  - No 2-lane jumps (lane 0↔2)
  - Intensity drives density: low=sparse, medium=moderate, high=dense

Usage:
    python level_designer.py analysis.json
    python level_designer.py analysis.json --output my_beatmap.json
    python level_designer.py analysis.json --difficulty easy
"""

import argparse
import json
import sys
from pathlib import Path


# ═══════════════════════════════════════════════════════════════
# CONSTANTS
# ═══════════════════════════════════════════════════════════════

# Frequency band → shape → lane
PASS_TO_SHAPE = {
    "kick": "circle", "snare": "square", "melody": "square",
    "hihat": "triangle", "flux": "square",
}
SHAPE_TO_LANE = {"circle": 0, "square": 1, "triangle": 2}
ALL_SHAPES = ["circle", "square", "triangle"]

# Per-difficulty, per-intensity combo parameters
INTENSITY_PARAMS = {
    "easy": {
        "low":    {"combo_len": (1, 2), "rest_beats": 3, "max_gap": 3, "beat_usage": 0.30},
        "medium": {"combo_len": (2, 3), "rest_beats": 2, "max_gap": 2, "beat_usage": 0.45},
        "high":   {"combo_len": (2, 3), "rest_beats": 1, "max_gap": 2, "beat_usage": 0.60},
    },
    "medium": {
        "low":    {"combo_len": (2, 3), "rest_beats": 2, "max_gap": 3, "beat_usage": 0.40},
        "medium": {"combo_len": (2, 3), "rest_beats": 1, "max_gap": 2, "beat_usage": 0.60},
        "high":   {"combo_len": (3, 4), "rest_beats": 1, "max_gap": 1, "beat_usage": 0.80},
    },
    "hard": {
        "low":    {"combo_len": (2, 3), "rest_beats": 1, "max_gap": 2, "beat_usage": 0.60},
        "medium": {"combo_len": (3, 4), "rest_beats": 1, "max_gap": 1, "beat_usage": 0.85},
        "high":   {"combo_len": (4, 6), "rest_beats": 0, "max_gap": 1, "beat_usage": 1.00},
    },
}

DIFFICULTY_CONFIG = {
    "easy":   {"intro_rest_beats": 8, "allowed_kinds": ["shape_gate"],                                "flux_percentile": 80},
    "medium": {"intro_rest_beats": 4, "allowed_kinds": ["shape_gate", "lane_block"],                   "flux_percentile": 55},
    "hard":   {"intro_rest_beats": 2, "allowed_kinds": ["shape_gate", "lane_block", "low_bar", "high_bar"], "flux_percentile": 30},
}


# ═══════════════════════════════════════════════════════════════
# HELPERS
# ═══════════════════════════════════════════════════════════════

def snap_events_to_beats(events, beats, tolerance=0.08):
    """Snap each event to the nearest beat within tolerance. One event per beat."""
    snapped, used = [], set()
    for ev in events:
        lo, hi, best_idx, best_dist = 0, len(beats) - 1, None, tolerance + 1
        while lo <= hi:
            mid = (lo + hi) // 2
            dist = abs(beats[mid] - ev["t"])
            if dist < best_dist:
                best_dist, best_idx = dist, mid
            if beats[mid] < ev["t"]: lo = mid + 1
            else: hi = mid - 1
        if best_idx is not None and best_dist <= tolerance and best_idx not in used:
            used.add(best_idx)
            snapped.append({**ev, "beat_idx": best_idx, "beat_time": beats[best_idx]})
    snapped.sort(key=lambda e: e["beat_idx"])
    return snapped


def pick_shape(passes, beat_idx):
    """Pick shape from onset passes using frequency band mapping."""
    candidates = [PASS_TO_SHAPE[p] for p in passes if p in PASS_TO_SHAPE]
    if not candidates:
        candidates = ["circle"]
    unique = list(dict.fromkeys(candidates))
    return unique[beat_idx % len(unique)]


def get_section_at(t, structure):
    """Find which song section a timestamp falls in."""
    for sec in structure:
        if sec["start"] <= t < sec["end"]:
            return sec
    return structure[-1] if structure else {"section": "verse", "intensity": "medium"}


def make_lane_sequence(start_lane, count):
    """Adjacent-only lane sequence. Bounces off edges 0↔2."""
    lanes, direction = [start_lane], 1
    for _ in range(count - 1):
        nxt = lanes[-1] + direction
        if nxt < 0 or nxt > 2:
            direction = -direction
            nxt = lanes[-1] + direction
        lanes.append(nxt)
    return lanes


def interpolate_flux_threshold(stats, percentile):
    """Interpolate flux threshold from percentile breakpoints."""
    points = [(0, stats["min"]), (25, stats["p25"]), (50, stats["p50"]),
              (75, stats["p75"]), (90, stats["p90"]), (100, stats["max"])]
    for i in range(len(points) - 1):
        p_lo, v_lo = points[i]
        p_hi, v_hi = points[i + 1]
        if p_lo <= percentile <= p_hi:
            frac = (percentile - p_lo) / (p_hi - p_lo) if p_hi > p_lo else 0
            return v_lo + frac * (v_hi - v_lo)
    return stats["p50"]


def action_family(kind):
    """Group obstacle types: 'shape' (buttons) vs 'movement' (dodging)."""
    return "shape" if kind == "shape_gate" else "movement"


# ═══════════════════════════════════════════════════════════════
# COMBO BUILDERS — one function per combo type
# ═══════════════════════════════════════════════════════════════

def build_shape_combo(combo_beats, event_map, current_lane):
    """Stay in one lane, change shapes each beat."""
    # Pick combo lane from first event's natural shape (adjacent only)
    ev0 = event_map.get(combo_beats[0])
    if ev0:
        natural = SHAPE_TO_LANE[pick_shape(ev0.get("passes", ["kick"]), combo_beats[0])]
        if abs(natural - current_lane) <= 1:
            current_lane = natural
    combo_lane = current_lane

    obstacles, last_shape = [], None
    for bi in combo_beats:
        ev = event_map.get(bi)
        shape = pick_shape(ev.get("passes", ["kick"]), bi) if ev else ALL_SHAPES[bi % 3]
        if shape == last_shape:
            shape = [s for s in ALL_SHAPES if s != shape][bi % 2]
        obstacles.append({"beat": bi, "kind": "shape_gate", "shape": shape, "lane": combo_lane})
        last_shape = shape

    return obstacles, current_lane


def build_lane_combo(combo_beats, current_lane):
    """Dodge lane_blocks. Min gap=2 between beats for swipe timing."""
    spaced = [combo_beats[0]]
    for bi in combo_beats[1:]:
        if bi - spaced[-1] >= 2:
            spaced.append(bi)

    lane_seq = make_lane_sequence(current_lane, len(spaced))
    obstacles = []
    for k, bi in enumerate(spaced):
        free = lane_seq[k]
        obstacles.append({"beat": bi, "kind": "lane_block", "blocked": [l for l in [0,1,2] if l != free]})
        current_lane = free

    return obstacles, current_lane


def build_bar_combo(combo_beats, allowed_kinds):
    """Jump/slide sequence for high-intensity sections."""
    obstacles = []
    for bi in combo_beats:
        kind = "low_bar" if bi % 2 == 0 else "high_bar"
        if kind not in allowed_kinds:
            kind = "low_bar" if "low_bar" in allowed_kinds else "high_bar"
        obstacles.append({"beat": bi, "kind": kind})
    return obstacles


# ═══════════════════════════════════════════════════════════════
# SECTION COMBO SEQUENCER
# ═══════════════════════════════════════════════════════════════

def pick_next_combo_type(current, intensity, allowed_kinds):
    """Rotate combo types: shape → lane → bar → shape..."""
    if current == "shape":
        return "lane"
    elif current == "lane":
        if intensity == "high" and ("low_bar" in allowed_kinds or "high_bar" in allowed_kinds):
            return "bar"
        return "shape"
    return "shape"


def select_beats_for_section(beat_indices, usage, event_map):
    """Pick which beats to use based on usage fraction. Prefer beats with events."""
    target = max(1, int(len(beat_indices) * usage))
    with_events = [bi for bi in beat_indices if bi in event_map]
    without = [bi for bi in beat_indices if bi not in event_map]
    selected = (with_events + without)[:target]
    selected.sort()
    return selected


def build_section_combos(beat_indices, section, difficulty, allowed_kinds,
                         event_map, current_lane):
    """Build combo sequences for one song section. Returns (obstacles, lane)."""
    intensity = section.get("intensity", "medium")
    params = INTENSITY_PARAMS[difficulty][intensity]
    combo_min, combo_max = params["combo_len"]
    rest = params["rest_beats"]

    selected = select_beats_for_section(beat_indices, params["beat_usage"], event_map)
    if not selected:
        return [], current_lane

    obstacles = []
    combo_type = "shape"
    cursor = 0

    while cursor < len(selected):
        # Slice out beats for this combo
        combo_len = min(combo_max, len(selected) - cursor)
        combo_len = max(combo_min, combo_len)
        combo_beats = selected[cursor:cursor + combo_len]

        # Build the combo
        if combo_type == "shape":
            obs, current_lane = build_shape_combo(combo_beats, event_map, current_lane)
            obstacles.extend(obs)
        elif combo_type == "lane" and "lane_block" in allowed_kinds:
            obs, current_lane = build_lane_combo(combo_beats, current_lane)
            obstacles.extend(obs)
        elif combo_type == "bar" and ("low_bar" in allowed_kinds or "high_bar" in allowed_kinds):
            obstacles.extend(build_bar_combo(combo_beats, allowed_kinds))
        else:
            # Fallback to shape
            obs, current_lane = build_shape_combo(combo_beats, event_map, current_lane)
            obstacles.extend(obs)

        cursor += combo_len

        # Advance combo type and apply rest
        prev_type = combo_type
        combo_type = pick_next_combo_type(combo_type, intensity, allowed_kinds)
        type_changed = (combo_type != prev_type)
        min_rest = max(rest, 1) if type_changed else rest

        if min_rest > 0 and cursor < len(selected):
            last_beat = combo_beats[-1]
            while cursor < len(selected) and selected[cursor] - last_beat < min_rest + 1:
                cursor += 1

    # Fill long same-type gaps within this section
    obstacles = fill_gaps_within_section(obstacles, selected, params["max_gap"], current_lane)
    return obstacles, current_lane


# ═══════════════════════════════════════════════════════════════
# GAP FILLING — keep player engaged, but respect transitions
# ═══════════════════════════════════════════════════════════════

def fill_gaps_within_section(obstacles, available_beats, max_gap, current_lane):
    """Fill gaps that exceed max_gap, but only between same-type obstacles."""
    if not obstacles or not available_beats:
        return obstacles

    filled = sorted(obstacles, key=lambda o: o["beat"])

    # Fill gap from section start to first obstacle
    if filled[0]["beat"] - available_beats[0] > max_gap:
        fill_beat = available_beats[0] + max_gap
        if fill_beat < filled[0]["beat"]:
            filled.insert(0, _make_filler("shape_gate", fill_beat, current_lane))

    # Fill gaps between consecutive same-type obstacles
    i = 0
    while i < len(filled) - 1:
        gap = filled[i + 1]["beat"] - filled[i]["beat"]
        if gap > max_gap and filled[i]["kind"] == filled[i + 1]["kind"]:
            fill_beat = filled[i]["beat"] + max_gap
            filled.insert(i + 1, _make_filler(filled[i]["kind"], fill_beat, current_lane))
        i += 1

    return filled


def fill_gaps_globally(obstacles, max_gap):
    """Fill gaps across section boundaries (same-type only)."""
    i = 0
    fill_lane = 1
    while i < len(obstacles) - 1:
        gap = obstacles[i + 1]["beat"] - obstacles[i]["beat"]
        if obstacles[i]["kind"] == "shape_gate":
            fill_lane = obstacles[i].get("lane", 1)
        if gap > max_gap and obstacles[i]["kind"] == obstacles[i + 1]["kind"]:
            fill_beat = obstacles[i]["beat"] + max_gap
            obstacles.insert(i + 1, _make_filler(obstacles[i]["kind"], fill_beat, fill_lane))
        i += 1
    return obstacles


def _make_filler(kind, beat, lane):
    """Create a filler obstacle matching the given type."""
    if kind == "lane_block":
        return {"beat": beat, "kind": "lane_block", "blocked": [l for l in [0,1,2] if l != lane]}
    return {"beat": beat, "kind": "shape_gate", "shape": ALL_SHAPES[beat % 3], "lane": lane}


# ═══════════════════════════════════════════════════════════════
# POST-PROCESSING — enforce rules after all obstacles placed
# ═══════════════════════════════════════════════════════════════

def enforce_transition_beats(obstacles):
    """Remove obstacles that create shape↔movement transitions with gap < 2."""
    if not obstacles:
        return obstacles
    cleaned = [obstacles[0]]
    for obs in obstacles[1:]:
        prev_fam = action_family(cleaned[-1]["kind"])
        curr_fam = action_family(obs["kind"])
        gap = obs["beat"] - cleaned[-1]["beat"]
        if prev_fam != curr_fam and gap < 2:
            continue
        cleaned.append(obs)
    return cleaned


def prevent_two_lane_jumps(obstacles):
    """Force lane 0↔2 transitions through center lane."""
    prev_lane = 1
    for obs in obstacles:
        if obs["kind"] != "shape_gate":
            continue
        if abs(obs.get("lane", 1) - prev_lane) == 2:
            obs["lane"] = 1
            obs["shape"] = "square"
        prev_lane = obs.get("lane", prev_lane)
    return obstacles


def enforce_lane_change_gap(obstacles):
    """Any lane change requires ≥1 beat gap. Remove obstacles that violate."""
    if not obstacles:
        return obstacles
    # Track the player's lane through all obstacle types
    result = [obstacles[0]]
    prev_lane = obstacles[0].get("lane", 1) if obstacles[0]["kind"] == "shape_gate" else 1
    for obs in obstacles[1:]:
        if obs["kind"] == "shape_gate":
            obs_lane = obs.get("lane", 1)
            if obs_lane != prev_lane:
                gap = obs["beat"] - result[-1]["beat"]
                if gap < 2:
                    continue  # skip — lane change on consecutive beat
            prev_lane = obs_lane
        elif obs["kind"] == "lane_block":
            # Lane blocks change the player's lane — track it
            blocked = obs.get("blocked", [])
            for lane in [0, 1, 2]:
                if lane not in blocked:
                    prev_lane = lane
                    break
        result.append(obs)
    return result


def prevent_triple_shapes(obstacles):
    """No 3+ same shape in a row."""
    for i in range(2, len(obstacles)):
        if obstacles[i]["kind"] != "shape_gate":
            continue
        prev = [obstacles[j].get("shape") for j in range(i-2, i)
                if obstacles[j]["kind"] == "shape_gate"]
        if len(prev) == 2 and all(s == obstacles[i]["shape"] for s in prev):
            others = [s for s in ALL_SHAPES if s != obstacles[i]["shape"]]
            obstacles[i]["shape"] = others[i % len(others)]
            obstacles[i]["lane"] = SHAPE_TO_LANE[obstacles[i]["shape"]]
    return obstacles


def deduplicate(obstacles):
    """Sort by beat and remove duplicates."""
    obstacles.sort(key=lambda o: o["beat"])
    seen, result = set(), []
    for obs in obstacles:
        if obs["beat"] not in seen:
            seen.add(obs["beat"])
            result.append(obs)
    return result


# ═══════════════════════════════════════════════════════════════
# MAIN PIPELINE
# ═══════════════════════════════════════════════════════════════

def design_level(analysis, difficulty):
    """Full pipeline: analysis → obstacle list for one difficulty."""
    cfg = DIFFICULTY_CONFIG[difficulty]
    beats = analysis["beats"]
    events = analysis["events"]
    structure = analysis["structure"]

    # Step 1: Filter events by flux strength
    threshold = interpolate_flux_threshold(analysis["flux_stats"], cfg["flux_percentile"])
    strong = [e for e in events if e["flux"] >= threshold]

    # Step 2: Snap to beat grid
    all_on_beat = snap_events_to_beats(events, beats)
    event_map = {ev["beat_idx"]: ev for ev in all_on_beat}

    # Step 3: Walk sections, build combos
    intro_end = cfg["intro_rest_beats"]
    all_obstacles = []
    current_lane = 1

    for section in structure:
        section_beats = [i for i, bt in enumerate(beats)
                         if section["start"] <= bt < section["end"] and i >= intro_end]
        if section_beats:
            obs, current_lane = build_section_combos(
                section_beats, section, difficulty,
                cfg["allowed_kinds"], event_map, current_lane)
            all_obstacles.extend(obs)

    # Step 4: Post-processing pipeline
    all_obstacles = deduplicate(all_obstacles)
    all_obstacles = fill_gaps_globally(all_obstacles, INTENSITY_PARAMS[difficulty]["low"]["max_gap"])
    all_obstacles = enforce_transition_beats(all_obstacles)
    all_obstacles = prevent_two_lane_jumps(all_obstacles)
    all_obstacles = prevent_triple_shapes(all_obstacles)
    all_obstacles = enforce_lane_change_gap(all_obstacles)

    return all_obstacles


def build_beatmap(analysis, difficulties):
    """Build the full beatmap JSON from analysis + designed levels."""
    beats = analysis["beats"]
    diff_data = {}
    for diff in difficulties:
        obs = design_level(analysis, diff)
        diff_data[diff] = {"beats": obs, "count": len(obs)}
    return {
        "song_id": analysis["title"],
        "title": analysis["title"],
        "bpm": analysis["bpm"],
        "offset": round(beats[0], 3) if beats else 0.0,
        "lead_beats": 4,
        "duration_sec": analysis["duration"],
        "difficulties": diff_data,
        "structure": analysis["structure"],
    }


# ═══════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="Combo-based beatmap generator")
    parser.add_argument("input", help="Path to analysis JSON (from rhythm_pipeline.py)")
    parser.add_argument("--output", "-o", default=None)
    parser.add_argument("--difficulty", "-d", choices=["easy", "medium", "hard", "all"], default="all")
    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: {args.input} not found", file=sys.stderr)
        sys.exit(1)

    with open(args.input) as f:
        analysis = json.load(f)

    diffs = ["easy", "medium", "hard"] if args.difficulty == "all" else [args.difficulty]

    print("=" * 60)
    print(f"  LEVEL DESIGNER | {analysis['title']} | BPM {analysis['bpm']}")
    print("=" * 60)

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
