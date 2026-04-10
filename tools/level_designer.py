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
# OBSTACLE KIND ASSIGNMENT
# ---------------------------------------------------------------------------

def pick_kind(event: dict, allowed_kinds: list[str],
              prev_kinds: list[str], beat_idx: int,
              section: dict) -> dict:
    """
    Decide what kind of obstacle this beat should be.
    Returns {"kind": ..., "shape": ..., "lane": ..., "blocked": ...}
    """
    shape = pick_shape(event["passes"], beat_idx)
    lane = SHAPE_TO_LANE[shape]

    # default: shape_gate (most common, core mechanic)
    result = {
        "kind": "shape_gate",
        "shape": shape,
        "lane": lane,
    }

    # lane_block: triggered by hihat presence in the event
    # limit to ~1 in 5 obstacles to keep shape_gate as the core
    if "lane_block" in allowed_kinds:
        has_hihat = "hihat" in event["passes"]
        no_kick = "kick" not in event["passes"]
        recent_lane_blocks = sum(1 for k in prev_kinds[-5:] if k == "lane_block")

        if has_hihat and no_kick and recent_lane_blocks == 0:
            free_lane = lane
            blocked = [l for l in [0, 1, 2] if l != free_lane]
            result = {
                "kind": "lane_block",
                "blocked": blocked,
            }

    # low_bar / high_bar: sparingly in chorus/drop sections
    is_intense = section.get("section") in ("chorus", "drop")
    if is_intense:
        recent_bars = sum(1 for k in prev_kinds[-10:]
                          if k in ("low_bar", "high_bar"))
        if recent_bars == 0 and len(prev_kinds) >= 6:
            if "low_bar" in allowed_kinds and beat_idx % 16 < 8:
                result = {"kind": "low_bar"}
            elif "high_bar" in allowed_kinds and beat_idx % 16 >= 8:
                result = {"kind": "high_bar"}

    return result


# ---------------------------------------------------------------------------
# ANTI-REPETITION — make patterns feel musical, not robotic
# ---------------------------------------------------------------------------

def apply_variety(obstacles: list[dict]) -> list[dict]:
    """
    Post-process to prevent boring patterns:
    - No more than 3 identical shapes in a row
    - No more than 3 same-lane shape_gates in a row
    """
    shapes = ["circle", "square", "triangle"]

    for i in range(len(obstacles)):
        obs = obstacles[i]
        if obs["kind"] != "shape_gate":
            continue

        # check for 3+ same shape in a row
        if i >= 2:
            prev_shapes = [obstacles[j].get("shape") for j in range(i-2, i)
                           if obstacles[j]["kind"] == "shape_gate"]
            if len(prev_shapes) == 2 and all(s == obs["shape"] for s in prev_shapes):
                # pick a different shape
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

    # 4. Assign obstacle kinds
    obstacles = []
    prev_kinds = []
    for ev in selected:
        section = get_section_at(ev["beat_time"], structure)
        obs_info = pick_kind(ev, cfg["kinds"], prev_kinds, ev["beat_idx"], section)
        obs = {
            "beat": ev["beat_idx"],
            **obs_info,
        }
        obstacles.append(obs)
        prev_kinds.append(obs["kind"])

    # 5. Apply variety rules
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
