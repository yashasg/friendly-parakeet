"""
level_designer.py — Combo-based beatmap generator for SHAPESHIFTER.

Architecture:
  PART 1 — GENERATOR: Creates obstacles that make the game fun.
    Reads song structure, builds shape/lane/bar combos per section.
  PART 2 — CLEANER: Removes obstacles that break the game.
    Each rule is one function. Rules never conflict.

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

PASS_TO_SHAPE = {
    "kick": "circle", "snare": "square", "melody": "square",
    "hihat": "triangle", "flux": "square",
}
SHAPE_TO_LANE = {"circle": 0, "square": 1, "triangle": 2}
LANE_TO_SHAPE = {0: "circle", 1: "square", 2: "triangle"}
ALL_SHAPES = ["circle", "square", "triangle"]

INTENSITY_PARAMS = {
    "easy": {
        "low":    {"combo_len": (1, 2), "rest": 3, "max_gap": 3, "usage": 0.30},
        "medium": {"combo_len": (2, 3), "rest": 2, "max_gap": 2, "usage": 0.45},
        "high":   {"combo_len": (2, 3), "rest": 1, "max_gap": 2, "usage": 0.60},
    },
    "medium": {
        "low":    {"combo_len": (2, 3), "rest": 2, "max_gap": 3, "usage": 0.40},
        "medium": {"combo_len": (2, 3), "rest": 1, "max_gap": 2, "usage": 0.60},
        "high":   {"combo_len": (3, 4), "rest": 1, "max_gap": 1, "usage": 0.80},
    },
    "hard": {
        "low":    {"combo_len": (2, 3), "rest": 1, "max_gap": 2, "usage": 0.60},
        "medium": {"combo_len": (3, 4), "rest": 1, "max_gap": 1, "usage": 0.85},
        "high":   {"combo_len": (4, 6), "rest": 0, "max_gap": 1, "usage": 1.00},
    },
}

DIFFICULTY_CONFIG = {
    "easy":   {"intro_rest": 8, "kinds": ["shape_gate"],                                       "flux_pct": 80},
    "medium": {"intro_rest": 4, "kinds": ["shape_gate", "lane_block"],                          "flux_pct": 55},
    "hard":   {"intro_rest": 2, "kinds": ["shape_gate", "lane_block", "low_bar", "high_bar"],   "flux_pct": 30},
}


# ═══════════════════════════════════════════════════════════════
# SHARED HELPERS
# ═══════════════════════════════════════════════════════════════

def snap_events_to_beats(events, beats, tolerance=0.08):
    """Snap each event to nearest beat. One event per beat."""
    snapped, used = [], set()
    for ev in events:
        lo, hi, best, best_d = 0, len(beats)-1, None, tolerance+1
        while lo <= hi:
            mid = (lo+hi)//2
            d = abs(beats[mid] - ev["t"])
            if d < best_d: best_d, best = d, mid
            if beats[mid] < ev["t"]: lo = mid+1
            else: hi = mid-1
        if best is not None and best_d <= tolerance and best not in used:
            used.add(best)
            snapped.append({**ev, "beat_idx": best, "beat_time": beats[best]})
    snapped.sort(key=lambda e: e["beat_idx"])
    return snapped


def pick_shape(passes, beat_idx):
    """Pick shape from onset passes via frequency mapping."""
    cands = [PASS_TO_SHAPE[p] for p in passes if p in PASS_TO_SHAPE] or ["circle"]
    return list(dict.fromkeys(cands))[beat_idx % len(list(dict.fromkeys(cands)))]


def flux_threshold(stats, pct):
    """Interpolate flux threshold from percentile."""
    required = {"min", "p25", "p50", "p75", "p90", "max"}
    if not stats or not required.issubset(stats):
        return 0.0  # no flux data: keep all events
    pts = [(0,stats["min"]),(25,stats["p25"]),(50,stats["p50"]),
           (75,stats["p75"]),(90,stats["p90"]),(100,stats["max"])]
    for i in range(len(pts)-1):
        if pts[i][0] <= pct <= pts[i+1][0]:
            f = (pct-pts[i][0])/(pts[i+1][0]-pts[i][0]) if pts[i+1][0]>pts[i][0] else 0
            return pts[i][1] + f*(pts[i+1][1]-pts[i][1])
    return stats["p50"]


def lane_of(obs, fallback=1):
    """What lane does this obstacle put the player in?"""
    if obs["kind"] == "shape_gate":
        return obs.get("lane", 1)
    elif obs["kind"] == "lane_block":
        blocked = obs.get("blocked", [])
        for l in [0,1,2]:
            if l not in blocked: return l
    return fallback


def family_of(obs):
    """'shape' for shape_gate, 'movement' for everything else."""
    return "shape" if obs["kind"] == "shape_gate" else "movement"


# ═══════════════════════════════════════════════════════════════
# PART 1: GENERATOR — what makes the game fun
# ═══════════════════════════════════════════════════════════════

def gen_shape_combo(beats, event_map, lane):
    """Generate shape combo: stay in one lane, change shapes."""
    obs, last = [], None
    for bi in beats:
        ev = event_map.get(bi)
        shape = pick_shape(ev.get("passes", ["kick"]), bi) if ev else LANE_TO_SHAPE[lane]
        if shape == last:
            shape = [s for s in ALL_SHAPES if s != shape][bi % 2]
        obs.append({"beat": bi, "kind": "shape_gate", "shape": shape, "lane": lane})
        last = shape
    return obs


def gen_lane_combo(beats, start_lane):
    """Generate lane combo: dodge blocks, min gap=2 between moves."""
    spaced = [beats[0]]
    for bi in beats[1:]:
        if bi - spaced[-1] >= 2:
            spaced.append(bi)

    obs, lane, direction = [], start_lane, 1
    for bi in spaced:
        nxt = lane + direction
        if nxt < 0 or nxt > 2: direction = -direction; nxt = lane + direction
        lane = nxt
        obs.append({"beat": bi, "kind": "lane_block",
                     "blocked": [l for l in [0,1,2] if l != lane]})
    return obs, lane


def gen_bar_combo(beats, kinds):
    """Generate bar combo: alternating jump/slide."""
    obs = []
    for bi in beats:
        kind = "low_bar" if bi % 2 == 0 else "high_bar"
        if kind not in kinds:
            kind = "low_bar" if "low_bar" in kinds else "high_bar"
        obs.append({"beat": bi, "kind": kind})
    return obs


def gen_section(beat_indices, intensity, difficulty, kinds, event_map, start_lane):
    """Generate combos for one song section."""
    params = INTENSITY_PARAMS[difficulty][intensity]
    combo_min, combo_max = params["combo_len"]
    rest = params["rest"]

    # Select beats based on usage
    target = max(1, int(len(beat_indices) * params["usage"]))
    with_ev = [b for b in beat_indices if b in event_map]
    without = [b for b in beat_indices if b not in event_map]
    selected = sorted((with_ev + without)[:target])
    if not selected:
        return [], start_lane

    obstacles = []
    lane = start_lane
    combo_type = "shape"
    cursor = 0

    while cursor < len(selected):
        size = min(combo_max, max(combo_min, len(selected) - cursor))
        chunk = selected[cursor:cursor + size]

        if combo_type == "shape":
            # Pick lane from first event (adjacent only)
            ev0 = event_map.get(chunk[0])
            if ev0:
                natural = SHAPE_TO_LANE[pick_shape(ev0.get("passes",["kick"]), chunk[0])]
                if abs(natural - lane) <= 1: lane = natural
            obstacles.extend(gen_shape_combo(chunk, event_map, lane))

        elif combo_type == "lane" and "lane_block" in kinds:
            combo_obs, lane = gen_lane_combo(chunk, lane)
            obstacles.extend(combo_obs)

        elif combo_type == "bar" and ("low_bar" in kinds or "high_bar" in kinds):
            obstacles.extend(gen_bar_combo(chunk, kinds))

        else:
            obstacles.extend(gen_shape_combo(chunk, event_map, lane))

        cursor += size

        # Rotate type
        prev_type = combo_type
        if combo_type == "shape": combo_type = "lane"
        elif combo_type == "lane":
            combo_type = "bar" if intensity == "high" and ("low_bar" in kinds or "high_bar" in kinds) else "shape"
        else: combo_type = "shape"

        # Rest between combos (longer when type changes)
        min_rest = max(rest, 1) if combo_type != prev_type else rest
        if min_rest > 0 and cursor < len(selected):
            last = chunk[-1]
            while cursor < len(selected) and selected[cursor] - last < min_rest + 1:
                cursor += 1

    return obstacles, lane


def generate_level(analysis, difficulty):
    """PART 1: Generate raw obstacle list from song analysis."""
    cfg = DIFFICULTY_CONFIG[difficulty]
    beats = analysis["beats"]
    events = analysis["events"]
    structure = analysis["structure"]

    thresh = flux_threshold(analysis["flux_stats"], cfg["flux_pct"])
    all_on_beat = snap_events_to_beats(events, beats)
    event_map = {ev["beat_idx"]: ev for ev in all_on_beat if ev.get("flux", 0.0) >= thresh}

    all_obs = []
    lane = 1
    for section in structure:
        sec_beats = [i for i, bt in enumerate(beats)
                     if section["start"] <= bt < section["end"] and i >= cfg["intro_rest"]]
        if sec_beats:
            obs, lane = gen_section(sec_beats, section.get("intensity","medium"),
                                    difficulty, cfg["kinds"], event_map, lane)
            all_obs.extend(obs)

    # Deduplicate
    all_obs.sort(key=lambda o: o["beat"])
    seen, deduped = set(), []
    for o in all_obs:
        if o["beat"] not in seen:
            seen.add(o["beat"])
            deduped.append(o)

    return deduped


# ═══════════════════════════════════════════════════════════════
# PART 2: CLEANER — what breaks the game
#
# Each function takes an obstacle list and returns a cleaned one.
# Rules:
#   - Only REMOVE or MUTATE obstacles (never insert)
#   - Each function handles exactly ONE rule
#   - Functions are independent and can run in any order
#   - Running a cleaner twice is idempotent
# Note: clean_triple_shapes mutates shape fields to preserve density
#       rather than removing obstacles.
# ═══════════════════════════════════════════════════════════════

def clean_lane_change_gap(obstacles):
    """RULE: Any lane change needs ≥2 beats gap.
    Remove obstacles that change the player's lane within 1 beat."""
    if not obstacles: return obstacles
    result = [obstacles[0]]
    prev_lane = lane_of(obstacles[0])
    for obs in obstacles[1:]:
        curr_lane = lane_of(obs, prev_lane)
        gap = obs["beat"] - result[-1]["beat"]
        if curr_lane != prev_lane and gap < 2:
            continue  # REMOVE: lane change too fast
        result.append(obs)
        prev_lane = curr_lane
    return result


def clean_type_transition(obstacles):
    """RULE: Switching between shape↔movement needs ≥2 beats gap.
    Remove obstacles that change action family within 1 beat."""
    if not obstacles: return obstacles
    result = [obstacles[0]]
    prev_fam = family_of(obstacles[0])
    for obs in obstacles[1:]:
        curr_fam = family_of(obs)
        gap = obs["beat"] - result[-1]["beat"]
        if curr_fam != prev_fam and gap < 2:
            continue  # REMOVE: type switch too fast
        result.append(obs)
        prev_fam = curr_fam
    return result


def clean_two_lane_jumps(obstacles):
    """RULE: No shape_gate jumps > 1 lane (0↔2).
    Remove shape_gates that require a 2-lane jump."""
    if not obstacles: return obstacles
    result = [obstacles[0]]
    prev_lane = lane_of(obstacles[0])
    for obs in obstacles[1:]:
        if obs["kind"] == "shape_gate":
            obs_lane = obs.get("lane", 1)
            if abs(obs_lane - prev_lane) > 1:
                continue  # REMOVE: 2-lane jump
        result.append(obs)
        prev_lane = lane_of(obs, prev_lane)
    return result


def clean_triple_shapes(obstacles):
    """RULE: No 3+ consecutive same shape.
    Swap shape (not remove) to maintain density."""
    for i in range(2, len(obstacles)):
        if obstacles[i]["kind"] != "shape_gate": continue
        prev2 = [obstacles[j].get("shape") for j in range(i-2, i)
                  if obstacles[j]["kind"] == "shape_gate"]
        if len(prev2) == 2 and all(s == obstacles[i]["shape"] for s in prev2):
            others = [s for s in ALL_SHAPES if s != obstacles[i]["shape"]]
            new_shape = others[i % len(others)]
            # Only swap if the new shape stays in the same lane (no new violation)
            new_lane = SHAPE_TO_LANE[new_shape]
            if new_lane == obstacles[i]["lane"]:
                obstacles[i]["shape"] = new_shape
            else:
                # Keep shape, accept the triple — better than creating a lane jump
                pass
    return obstacles


def get_section_boundary_beats(analysis):
    """Map structure section boundaries to beat indices.
    The breathing room happens AT section transitions — the analysis
    already detected these from MFCC timbral changes."""
    beats = analysis.get("beats", [])
    structure = analysis.get("structure", [])
    if not beats or not structure:
        return []

    boundaries = []
    for i in range(1, len(structure)):
        boundary_time = structure[i]["start"]
        # Find the beat nearest to this boundary
        best_idx = 0
        best_dist = abs(beats[0] - boundary_time)
        for j, bt in enumerate(beats):
            d = abs(bt - boundary_time)
            if d < best_dist:
                best_dist = d
                best_idx = j
        boundaries.append(best_idx)

    return boundaries


def clean_breathing_room(obstacles, boundary_beats, difficulty):
    """RULE: No obstacles at section boundaries — the music breathes there.

    Breathing room per difficulty:
      hard:   2 beats around boundary (1 before, 1 after)
      medium: 2 beats around boundary (1 before, 1 after)
      easy:   3 beats around boundary (1 before, boundary, 1 after)
    """
    if not boundary_beats or not obstacles:
        return obstacles

    protected = set()
    for bb in boundary_beats:
        if difficulty == "easy":
            protected.update([bb - 1, bb, bb + 1])
        else:
            protected.update([bb, bb + 1])

    return [obs for obs in obstacles if obs["beat"] not in protected]


def clean_level(obstacles, difficulty="medium", boundary_beats=None):
    """Run all cleaners in order. Order matters:
    1. Breathing room (remove obstacles at section boundaries)
    2. Two-lane jumps (removes the most egregious)
    3. Lane change gap (depends on accurate lane tracking)
    4. Type transition (independent of lane)
    5. Triple shapes (swaps, doesn't remove — safe to run last)
    """
    if boundary_beats:
        obstacles = clean_breathing_room(obstacles, boundary_beats, difficulty)
    obstacles = clean_two_lane_jumps(obstacles)
    obstacles = clean_lane_change_gap(obstacles)
    obstacles = clean_type_transition(obstacles)
    obstacles = clean_triple_shapes(obstacles)
    return obstacles


# ═══════════════════════════════════════════════════════════════
# PIPELINE: GENERATE → CLEAN
# ═══════════════════════════════════════════════════════════════

def design_level(analysis, difficulty):
    """Full pipeline: generate fun obstacles, then clean rule violations."""
    raw = generate_level(analysis, difficulty)
    boundary_beats = get_section_boundary_beats(analysis)
    clean = clean_level(raw, difficulty, boundary_beats)
    return clean


def build_beatmap(analysis, difficulties):
    """Build the full beatmap JSON."""
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
    parser.add_argument("input", help="Path to analysis JSON")
    parser.add_argument("--output", "-o", default=None)
    parser.add_argument("--difficulty", "-d", choices=["easy","medium","hard","all"], default="all")
    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: {args.input} not found", file=sys.stderr); sys.exit(1)

    with open(args.input) as f:
        analysis = json.load(f)

    diffs = ["easy","medium","hard"] if args.difficulty == "all" else [args.difficulty]

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
