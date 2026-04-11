"""
level_designer.py
=================
Takes audio analysis JSON (from rhythm_pipeline.py) and produces a beatmap
JSON that the game engine can load.

Responsibilities (separated from audio analysis):
  - Snap onsets to the beat grid
  - Assign obstacle kinds (shape_gate, lane_block, low_bar, high_bar)
  - Assign shapes (circle/square/triangle) from frequency bands
  - Apply difficulty rules (density, gap, type unlocking)
  - Ensure playability (no impossible patterns, breathing room)

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
# DESIGN CONSTANTS — from rhythm-design.md and game.md
# ---------------------------------------------------------------------------

# frequency band → shape → lane (from rhythm-design.md Section 2)
PASS_TO_SHAPE = {
    "kick":   "circle",    # low band  → circle → lane 0
    "snare":  "square",    # mid band  → square → lane 1
    "melody": "square",    # mid band  → square → lane 1
    "hihat":  "triangle",  # high band → triangle → lane 2
}

SHAPE_TO_LANE = {
    "circle":   0,
    "square":   1,
    "triangle": 2,
}

# difficulty brackets (from game.md / rhythm-design.md)
DIFFICULTY = {
    "easy": {
        "min_beat_gap": 4,        # minimum beats between obstacles
        "flux_percentile": 80,    # only top 20% flux events
        "kinds": ["shape_gate"],
        "intro_rest_beats": 8,    # silent intro
        "density_in_chorus": 2,   # every Nth beat in chorus
        "density_in_verse": 4,    # every Nth beat in verse
    },
    "medium": {
        "min_beat_gap": 2,
        "flux_percentile": 55,
        "kinds": ["shape_gate", "lane_block"],
        "intro_rest_beats": 4,
        "density_in_chorus": 2,
        "density_in_verse": 2,
    },
    "hard": {
        "min_beat_gap": 1,
        "flux_percentile": 30,
        "kinds": ["shape_gate", "lane_block", "low_bar", "high_bar"],
        "intro_rest_beats": 2,
        "density_in_chorus": 1,
        "density_in_verse": 1,
    },
}


# ---------------------------------------------------------------------------
# BEAT GRID SNAPPING
# ---------------------------------------------------------------------------

def snap_events_to_beats(events: list[dict], beats: list[float],
                         tolerance: float = 0.08) -> list[dict]:
    """
    Snap each event to the nearest beat within tolerance.
    Returns events with 'beat_idx' and 'beat_time' added.
    Events that don't land near any beat are discarded.
    """
    snapped = []
    beat_set = set()

    for ev in events:
        t = ev["t"]
        best_idx = None
        best_dist = tolerance + 1

        # binary search would be faster, but this is fine for offline tool
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
            # only one event per beat
            if best_idx not in beat_set:
                beat_set.add(best_idx)
                snapped.append({
                    **ev,
                    "beat_idx": best_idx,
                    "beat_time": beats[best_idx],
                })

    snapped.sort(key=lambda e: e["beat_idx"])
    return snapped


# ---------------------------------------------------------------------------
# SHAPE ASSIGNMENT — which shape does this event demand?
# ---------------------------------------------------------------------------

def pick_shape(passes: list[str], beat_idx: int) -> str:
    """
    Pick shape from which onset passes fired, using frequency band mapping.
    Rotates priority to keep shapes balanced rather than always favoring kick.
    """
    # map each pass to its natural shape
    candidates = []
    for p in passes:
        if p in PASS_TO_SHAPE:
            candidates.append(PASS_TO_SHAPE[p])

    if not candidates:
        candidates = ["circle"]

    # when multiple candidates, rotate selection based on beat index
    # to maintain variety across the level
    unique = list(dict.fromkeys(candidates))  # dedup preserving order
    return unique[beat_idx % len(unique)]


# ---------------------------------------------------------------------------
# COMBO PATTERN SYSTEM
#
# Instead of assigning obstacles one-by-one (chaotic), group them into
# musical phrases ("combos") where the player does ONE type of action:
#
#   SHAPE COMBO:  Stay in one lane, change shapes on every beat
#   LANE COMBO:   Keep shape irrelevant, dodge lane_blocks on every beat
#   BAR COMBO:    Jump/slide sequence
#   REST:         Breathing room between combos
#
# This prevents impossible patterns like "be in lane 2 then leave lane 2"
# on consecutive beats. Within a combo, the action type is consistent.
# ---------------------------------------------------------------------------

def build_combos(selected_events: list[dict], cfg: dict,
                 structure: list[dict], allowed_kinds: list[str]) -> list[dict]:
    """
    Group selected beat events into combos and assign obstacle kinds.
    Returns a flat list of obstacle dicts.
    """
    shapes = ["circle", "square", "triangle"]
    obstacles = []

    i = 0
    current_lane = 1  # player starts center
    combo_id = 0

    while i < len(selected_events):
        ev = selected_events[i]
        section = get_section_at(ev["beat_time"], structure)

        # Decide combo type based on section intensity and allowed kinds
        combo_type = _pick_combo_type(
            ev, section, allowed_kinds, combo_id, current_lane)

        if combo_type == "shape":
            # ── SHAPE COMBO: stay in one lane, change shapes ──────
            # Pick a lane (prefer current to avoid movement)
            combo_lane = current_lane
            # If the event's natural shape maps to a different lane,
            # move there first (this becomes the combo's lane)
            natural_shape = pick_shape(ev.get("passes", ["kick"]), ev["beat_idx"])
            natural_lane = SHAPE_TO_LANE[natural_shape]
            if abs(natural_lane - current_lane) <= 1:
                combo_lane = natural_lane  # adjacent, safe to move

            # Collect consecutive beats for this combo (max 4, or until gap > 2)
            combo_events = [ev]
            j = i + 1
            max_combo = 4 if section.get("intensity") == "high" else 3
            while j < len(selected_events) and len(combo_events) < max_combo:
                next_ev = selected_events[j]
                gap = next_ev["beat_idx"] - combo_events[-1]["beat_idx"]
                if gap > 2:
                    break
                combo_events.append(next_ev)
                j += 1

            # Assign shapes — rotate through shapes, never 3 in a row
            last_shape = None
            shape_idx = 0
            for ce in combo_events:
                shape = pick_shape(ce.get("passes", ["kick"]), ce["beat_idx"])
                # If same as last, rotate to next
                if shape == last_shape:
                    shape_idx = (shape_idx + 1) % len(shapes)
                    shape = shapes[shape_idx]
                obstacles.append({
                    "beat": ce["beat_idx"],
                    "kind": "shape_gate",
                    "shape": shape,
                    "lane": combo_lane,
                })
                last_shape = shape
                shape_idx = shapes.index(shape)

            current_lane = combo_lane
            i = j

        elif combo_type == "lane":
            # ── LANE COMBO: change lanes on consecutive beats ─────
            combo_events = [ev]
            j = i + 1
            max_combo = 3
            while j < len(selected_events) and len(combo_events) < max_combo:
                next_ev = selected_events[j]
                gap = next_ev["beat_idx"] - combo_events[-1]["beat_idx"]
                if gap > 2:
                    break
                combo_events.append(next_ev)
                j += 1

            # Alternate between lanes (only adjacent moves)
            lane_sequence = _make_lane_sequence(current_lane, len(combo_events))
            for k, ce in enumerate(combo_events):
                free_lane = lane_sequence[k]
                blocked = [l for l in [0, 1, 2] if l != free_lane]
                obstacles.append({
                    "beat": ce["beat_idx"],
                    "kind": "lane_block",
                    "blocked": blocked,
                })
                current_lane = free_lane

            i = j

        elif combo_type == "bar":
            # ── BAR COMBO: jump/slide ─────────────────────────────
            obstacles.append({
                "beat": ev["beat_idx"],
                "kind": "low_bar" if ev["beat_idx"] % 2 == 0 else "high_bar",
            })
            i += 1

        else:
            # ── SINGLE: standalone obstacle ───────────────────────
            shape = pick_shape(ev.get("passes", ["kick"]), ev["beat_idx"])
            lane = SHAPE_TO_LANE[shape]
            # Avoid 2-lane jumps
            if abs(lane - current_lane) > 1:
                lane = 1  # step through center
                shape = "square"
            obstacles.append({
                "beat": ev["beat_idx"],
                "kind": "shape_gate",
                "shape": shape,
                "lane": lane,
            })
            current_lane = lane
            i += 1

        combo_id += 1

    return obstacles


def _pick_combo_type(event: dict, section: dict, allowed_kinds: list[str],
                     combo_id: int, current_lane: int) -> str:
    """Decide what type of combo to start based on musical context."""
    intensity = section.get("intensity", "medium")
    sec_name = section.get("section", "verse")

    # Intro/outro: always shape combos (simple)
    if sec_name in ("intro", "outro"):
        return "shape"

    # Bridge: mix of lane combos and bars
    if sec_name == "bridge":
        if "lane_block" in allowed_kinds and combo_id % 3 == 1:
            return "lane"
        if ("low_bar" in allowed_kinds or "high_bar" in allowed_kinds) and combo_id % 3 == 2:
            return "bar"
        return "shape"

    # Verse: mostly shape combos, occasional lane combo
    if sec_name in ("verse", "pre-chorus"):
        if "lane_block" in allowed_kinds and combo_id % 4 == 2:
            return "lane"
        return "shape"

    # Chorus/drop: high variety — rotate through combo types
    if sec_name in ("chorus", "drop"):
        cycle = combo_id % 5
        if cycle < 2:
            return "shape"
        elif cycle == 2 and "lane_block" in allowed_kinds:
            return "lane"
        elif cycle == 3:
            return "shape"
        elif cycle == 4 and ("low_bar" in allowed_kinds or "high_bar" in allowed_kinds):
            return "bar"
        return "shape"

    return "shape"


def _make_lane_sequence(start_lane: int, count: int) -> list[int]:
    """
    Generate a sequence of lanes for a lane combo.
    Only adjacent moves (no 0↔2 jumps). Bounces off edges.
    """
    lanes = [start_lane]
    direction = 1  # start moving right
    for _ in range(count - 1):
        next_lane = lanes[-1] + direction
        if next_lane < 0 or next_lane > 2:
            direction = -direction
            next_lane = lanes[-1] + direction
        lanes.append(next_lane)
    return lanes


# ---------------------------------------------------------------------------
# ANTI-REPETITION — make patterns feel musical, not robotic
# ---------------------------------------------------------------------------

def apply_variety(obstacles: list[dict]) -> list[dict]:
    """
    Post-process to prevent boring patterns:
    - No more than 3 identical shapes in a row
    - No more than 3 same-lane shape_gates in a row
    - No 2-lane jumps (0↔2): force through center lane by inserting step
    """
    shapes = ["circle", "square", "triangle"]

    # Pass 1: prevent 2-lane jumps (0↔2 → step through lane 1)
    prev_lane = 1  # player starts center
    for obs in obstacles:
        if obs["kind"] != "shape_gate":
            continue
        lane = obs.get("lane", 1)
        if abs(lane - prev_lane) == 2:
            # Force to adjacent lane (step toward target through center)
            mid_lane = 1  # center is always between 0 and 2
            obs["lane"] = mid_lane
            # Pick a shape that maps to the center lane
            obs["shape"] = "square"
        prev_lane = obs.get("lane", prev_lane)

    # Pass 2: prevent 3+ same shape in a row
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
# STRUCTURE-AWARE DENSITY
# ---------------------------------------------------------------------------

def get_section_at(t: float, structure: list[dict]) -> dict:
    """Find which song section a timestamp falls in."""
    for sec in structure:
        if sec["start"] <= t < sec["end"]:
            return sec
    # fallback: last section
    return structure[-1] if structure else {
        "section": "verse", "intensity": "medium"
    }


# ---------------------------------------------------------------------------
# MAIN LEVEL DESIGN PIPELINE
# ---------------------------------------------------------------------------

def design_level(analysis: dict, difficulty: str) -> list[dict]:
    """
    Full pipeline: analysis data → obstacle list for one difficulty.
    """
    cfg = DIFFICULTY[difficulty]
    beats = analysis["beats"]
    events = analysis["events"]
    structure = analysis["structure"]

    # 1. Filter events by flux threshold
    # Interpolate between available percentile stats
    pct = cfg["flux_percentile"]
    stats = analysis["flux_stats"]
    breakpoints = [(0, stats["min"]), (25, stats["p25"]), (50, stats["p50"]),
                   (75, stats["p75"]), (90, stats["p90"]), (100, stats["max"])]

    flux_threshold = stats["p50"]  # fallback
    for i in range(len(breakpoints) - 1):
        p_lo, v_lo = breakpoints[i]
        p_hi, v_hi = breakpoints[i + 1]
        if p_lo <= pct <= p_hi:
            frac = (pct - p_lo) / (p_hi - p_lo) if p_hi > p_lo else 0
            flux_threshold = v_lo + frac * (v_hi - v_lo)
            break

    strong_events = [e for e in events if e["flux"] >= flux_threshold]

    # 2. Snap to beat grid
    on_beat = snap_events_to_beats(strong_events, beats, tolerance=0.08)

    # 3. Apply density + gap rules
    intro_end_beat = cfg["intro_rest_beats"]
    min_gap = cfg["min_beat_gap"]

    selected = []
    last_beat = -999

    for ev in on_beat:
        bi = ev["beat_idx"]

        # skip intro rest
        if bi < intro_end_beat:
            continue

        # enforce minimum beat gap
        if bi - last_beat < min_gap:
            continue

        # structure-aware density: in verse, thin out; in chorus, allow more
        section = get_section_at(ev["beat_time"], structure)
        if section["section"] in ("verse", "bridge"):
            density = cfg["density_in_verse"]
        else:
            density = cfg["density_in_chorus"]

        # density filter: only place obstacle every Nth beat from last
        if bi - last_beat < density:
            continue

        selected.append(ev)
        last_beat = bi

    # 3b. Fill gaps — if too long without an obstacle, place one at the
    # nearest beat with ANY event (even below flux threshold) to maintain
    # engagement. Max silent run: 8 beats (easy) to 4 beats (hard).
    max_silent = cfg["min_beat_gap"] * 3
    all_on_beat = snap_events_to_beats(events, beats, tolerance=0.08)
    all_beat_map = {ev["beat_idx"]: ev for ev in all_on_beat}

    filled = []
    for i, ev in enumerate(selected):
        filled.append(ev)

        # check gap to next selected obstacle
        next_beat = selected[i + 1]["beat_idx"] if i + 1 < len(selected) else len(beats)
        gap = next_beat - ev["beat_idx"]

        if gap > max_silent:
            # find beats in the gap that have any event
            cursor = ev["beat_idx"] + cfg["min_beat_gap"]
            while cursor < next_beat - cfg["min_beat_gap"]:
                if cursor in all_beat_map:
                    filled.append(all_beat_map[cursor])
                    cursor += cfg["min_beat_gap"]
                else:
                    cursor += 1

    filled.sort(key=lambda e: e["beat_idx"])
    # deduplicate
    seen = set()
    deduped = []
    for ev in filled:
        if ev["beat_idx"] not in seen:
            seen.add(ev["beat_idx"])
            deduped.append(ev)
    selected = deduped

    # 4. Assign obstacle kinds using combo system
    obstacles = build_combos(selected, cfg, structure, cfg["kinds"])

    # 5. Apply variety rules (anti-repetition pass)
    obstacles = apply_variety(obstacles)

    return obstacles


def build_beatmap(analysis: dict, difficulties: list[str]) -> dict:
    """Build the full beatmap JSON from analysis + designed levels."""

    # find the first beat as offset
    beats = analysis["beats"]
    offset = beats[0] if beats else 0.0

    diff_data = {}
    for diff in difficulties:
        obstacles = design_level(analysis, diff)
        diff_data[diff] = {
            "beats": obstacles,
            "count": len(obstacles),
        }

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
    parser.add_argument(
        "--output", "-o",
        default=None,
        help="Output beatmap JSON path (default: <song_id>_beatmap.json)"
    )
    parser.add_argument(
        "--difficulty", "-d",
        choices=["easy", "medium", "hard", "all"],
        default="all",
        help="Which difficulty to generate (default: all)"
    )
    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    with open(args.input) as f:
        analysis = json.load(f)

    print("=" * 60)
    print("  LEVEL DESIGNER")
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
