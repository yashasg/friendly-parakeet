"""
level_designer.py — Music-driven beatmap generator for SHAPESHIFTER.

Architecture (3 phases, each has one job, they don't fight):

  PHASE 1 — SELECT: Which beats get obstacles?
    Input:  analysis events, beat grid, song structure, difficulty
    Output: list of beat indices with their event data
    Rule:   density comes from song structure + flux threshold.
            Repeated sections reuse the same beat pattern.

  PHASE 2 — ASSIGN: What obstacle goes on each beat?
    Input:  selected beats with event data, difficulty, section context
    Output: obstacle list with kind/shape/lane
    Rule:   onset passes determine obstacle type (not rotation).
            Gap to previous note influences type choice.

  PHASE 3 — CLEAN: Fix violations without inserting new obstacles.
    Input:  raw obstacle list
    Output: cleaned obstacle list
    Rule:   each cleaner handles ONE rule, order is fixed,
            cleaners only remove or mutate (never insert).

Usage:
    python level_designer.py analysis.json
    python level_designer.py analysis.json --output my_beatmap.json
    python level_designer.py analysis.json --difficulty easy
"""

import argparse
import json
import sys
from pathlib import Path
from collections import Counter


# ═══════════════════════════════════════════════════════════════
# CONSTANTS
# ═══════════════════════════════════════════════════════════════

SHAPE_TO_LANE = {"circle": 0, "square": 1, "triangle": 2}
LANE_TO_SHAPE = {0: "circle", 1: "square", 2: "triangle"}
ALL_SHAPES = ["circle", "square", "triangle"]

# Section role determines density and allowed obstacle types.
# Chorus/drop = densest but most CONSISTENT (nori-nori).
# Bridge = sparse, breathing room.
# Final section = hardest patterns (the "final fortress").
SECTION_ROLE = {
    "intro":      {"density": 0.30, "types": ["shape_gate"], "consistent": True},
    "verse":      {"density": 0.65, "types": ["shape_gate", "lane_block"], "consistent": False},
    "pre-chorus": {"density": 0.80, "types": ["shape_gate", "lane_block"], "consistent": False},
    "drop":       {"density": 0.90, "types": ["shape_gate", "lane_block"], "consistent": True},
    "bridge":     {"density": 0.35, "types": ["shape_gate"], "consistent": False},
}

DIFFICULTY_SCALE = {"easy": 0.55, "medium": 0.80, "hard": 1.20}
DIFFICULTY_INTRO_REST = {"easy": 8, "medium": 4, "hard": 2}
DIFFICULTY_KINDS = {
    "easy":   {"shape_gate"},
    "medium": {"shape_gate", "lane_block"},
    "hard":   {"shape_gate", "lane_block"},
}

# Shape palette per section: controls how many shapes are in play.
# Intro/bridge = 1 shape (center lane, player learns/rests).
# Verse = 2 shapes (alternating, player grooves).
# Pre-chorus/drop = all 3 (full variety, player is warmed up).
SECTION_SHAPE_PALETTE = {
    "intro":      [1],
    "verse":      [0, 1],
    "pre-chorus": [0, 1, 2],
    "drop":       [0, 1, 2],
    "bridge":     [1],
}


# ═══════════════════════════════════════════════════════════════
# HELPERS
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


def flux_threshold(stats, pct):
    """Interpolate flux threshold from percentile."""
    required = {"min", "p25", "p50", "p75", "p90", "max"}
    if not stats or not required.issubset(stats):
        return 0.0
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
        for l in [1, 0, 2]:  # prefer center
            if l not in blocked:
                return l
    return fallback


# ═══════════════════════════════════════════════════════════════
# PHASE 1: SELECT — which beats get obstacles?
# ═══════════════════════════════════════════════════════════════

def select_beats(analysis, difficulty):
    """Select which beats will have obstacles.

    Key insight: repeated sections reuse the same relative beat pattern.
    The first verse defines the pattern; subsequent verses replay it.
    This gives players the "second time = mastery" feeling.
    """
    beats = analysis["beats"]
    events = analysis["events"]
    structure = analysis["structure"]
    scale = DIFFICULTY_SCALE[difficulty]
    intro_rest = DIFFICULTY_INTRO_REST[difficulty]

    # Flux filter: harder = lower threshold = more events kept
    flux_pcts = {"easy": 50, "medium": 25, "hard": 5}
    thresh = flux_threshold(analysis.get("flux_stats", {}), flux_pcts[difficulty])
    all_on_beat = snap_events_to_beats(events, beats)
    event_map = {ev["beat_idx"]: ev for ev in all_on_beat if ev.get("flux", 0.0) >= thresh}

    # Track patterns by section type for reuse
    section_patterns = {}      # "verse" -> list of relative beat offsets
    section_pattern_lens = {}  # "verse" -> original section length in beats

    selected = {}  # beat_idx -> event data (or None for imaginary beats)

    for section in structure:
        sec_name = section.get("section", "verse")
        sec_start = section["start"]
        sec_end = section["end"]

        # Get all beat indices in this section
        sec_beats = [i for i, bt in enumerate(beats)
                     if sec_start <= bt < sec_end and i >= intro_rest]
        if not sec_beats:
            continue

        role = SECTION_ROLE.get(sec_name, SECTION_ROLE["verse"])
        density = role["density"] * scale

        # Get beats that have events (musical content)
        event_beats = [b for b in sec_beats if b in event_map]
        target_count = max(1, int(len(sec_beats) * density))

        if sec_name in section_patterns:
            # REUSE pattern from first occurrence of this section type.
            # If this section is much longer than the original, tile the
            # pattern instead of stretching it (prevents huge gaps).
            pattern = section_patterns[sec_name]
            sec_base = sec_beats[0]
            sec_len = sec_beats[-1] - sec_beats[0] + 1
            orig_len = section_pattern_lens.get(sec_name, sec_len)

            # How many tiles fit?
            tiles = max(1, round(sec_len / orig_len))

            for tile in range(tiles):
                tile_base = sec_base + int(tile * sec_len / tiles)
                tile_len = sec_len / tiles
                for rel_offset in pattern:
                    bi = tile_base + int(rel_offset * tile_len)
                    closest = min(sec_beats, key=lambda b: abs(b - bi))
                    if closest not in selected:
                        selected[closest] = event_map.get(closest)
        else:
            # FIRST occurrence: select beats based on events, save pattern
            # Prefer beats with events; fill with evenly-spaced beats if needed
            chosen = sorted(event_beats[:target_count])
            if len(chosen) < target_count:
                # Add evenly-spaced beats from the section
                step = max(1, len(sec_beats) // target_count)
                for j in range(0, len(sec_beats), step):
                    if len(chosen) >= target_count:
                        break
                    if sec_beats[j] not in chosen:
                        chosen.append(sec_beats[j])
                chosen = sorted(chosen[:target_count])

            # Save relative pattern for reuse
            sec_base = sec_beats[0]
            sec_len = max(1, sec_beats[-1] - sec_beats[0])
            section_patterns[sec_name] = [(b - sec_base) / sec_len for b in chosen]
            section_pattern_lens[sec_name] = sec_len

            for bi in chosen:
                selected[bi] = event_map.get(bi)

    return selected, event_map


# ═══════════════════════════════════════════════════════════════
# PHASE 2: ASSIGN — what obstacle goes on each beat?
# ═══════════════════════════════════════════════════════════════

def assign_obstacle(beat_idx, event, gap_to_prev, section_name, difficulty,
                    prev_lane, prev_shapes, allowed_kinds):
    """Assign obstacle type based on musical content and context.

    Key insight from research: onset passes determine type, not rotation.
      - kick-only → high_bar (bass thump → slide down)
      - hihat-only → low_bar (percussive pop → jump up)
      - snare-only or snare+kick → lane_block (transient → dodge)
      - melody or multi-pass → shape_gate (harmonic → match shape)

    Gap context (from Liang et al.): short gaps favor same-type obstacles.
    """
    passes = set(event.get("passes", ["flux"])) if event else {"flux"}
    passes.discard("flux")  # flux is catch-all, ignore for type decision
    if not passes:
        passes = {"flux"}

    # Determine natural type from passes
    has_kick = "kick" in passes
    has_snare = "snare" in passes
    has_hihat = "hihat" in passes
    has_melody = "melody" in passes
    multi = len(passes) >= 3

    if multi or has_melody:
        natural_kind = "shape_gate"
    elif has_kick and not has_snare and not has_hihat:
        natural_kind = "high_bar"
    elif has_hihat and not has_kick and not has_snare:
        natural_kind = "low_bar"
    elif has_snare:
        natural_kind = "lane_block"
    else:
        natural_kind = "shape_gate"

    # Respect difficulty restrictions
    if natural_kind not in allowed_kinds:
        natural_kind = "shape_gate"

    # Short gaps (1-2 beats) should stay same action family
    # to avoid impossible type-switching
    if gap_to_prev is not None and gap_to_prev <= 1:
        natural_kind = "shape_gate"  # safest at fast pace

    # Build the obstacle
    obs = {"beat": beat_idx, "kind": natural_kind}

    if natural_kind == "shape_gate":
        # Flow-based shape picker:
        # 1. Section palette limits which shapes are available
        # 2. Short gaps → stay on same shape (groove / "L1 kick" principle)
        # 3. Longer gaps → alternate shape (variety)
        # 4. Anti-triple built in (never 3 of same shape in a row)
        palette = SECTION_SHAPE_PALETTE.get(section_name, [0, 1, 2])

        if len(palette) == 1:
            # Single-shape section (intro/bridge): always center
            lane = palette[0]
            shape = LANE_TO_SHAPE[lane]
        elif gap_to_prev is not None and gap_to_prev <= 2 and prev_shapes:
            # Short gap: stay on same shape for groove
            shape = prev_shapes[-1]
            lane = SHAPE_TO_LANE[shape]
            # But check anti-triple
            if (lane not in palette or
                    (len(prev_shapes) >= 2 and prev_shapes[-1] == prev_shapes[-2] == shape)):
                idx = palette.index(lane) if lane in palette else 0
                lane = palette[(idx + 1) % len(palette)]
                shape = LANE_TO_SHAPE[lane]
        elif prev_shapes:
            # Longer gap: pick a different shape
            prev_lane_s = SHAPE_TO_LANE[prev_shapes[-1]]
            candidates = [l for l in palette if l != prev_lane_s]
            if candidates:
                adjacent = [l for l in candidates if abs(l - prev_lane) <= 1]
                pool = adjacent if adjacent else candidates
                lane = pool[beat_idx % len(pool)]
            else:
                lane = palette[beat_idx % len(palette)]
            shape = LANE_TO_SHAPE[lane]
        else:
            # First note: start center if available
            lane = 1 if 1 in palette else palette[0]
            shape = LANE_TO_SHAPE[lane]

        # Keep lane adjacent to previous (no 2-lane jumps)
        if abs(lane - prev_lane) > 1:
            if lane > prev_lane:
                lane = prev_lane + 1
            else:
                lane = prev_lane - 1
            shape = LANE_TO_SHAPE[lane]

        obs["shape"] = shape
        obs["lane"] = lane

    elif natural_kind == "lane_block":
        # Block lanes that DIDN'T fire; keep player in lane that did
        target_lane = prev_lane  # default: stay put
        if has_kick:
            target_lane = 0
        elif has_snare:
            target_lane = 1
        elif has_hihat:
            target_lane = 2

        # Keep adjacent
        if abs(target_lane - prev_lane) > 1:
            target_lane = 1  # center is always safe

        obs["blocked"] = [l for l in [0, 1, 2] if l != target_lane]

    # low_bar and high_bar need no extra fields

    return obs


def assign_obstacles(selected_beats, event_map, analysis, difficulty):
    """Assign obstacle types to all selected beats."""
    structure = analysis["structure"]
    allowed = DIFFICULTY_KINDS[difficulty]

    # Variety targets: force non-shape_gate after consecutive runs
    # This ensures obstacle variety without fighting the onset-based type picker.
    # The consecutive threshold varies by section — drops are more consistent.
    variety_threshold = {"easy": 999, "medium": 3, "hard": 2}
    max_consecutive = variety_threshold[difficulty]

    # Build section lookup
    def section_at(beat_idx):
        bt = analysis["beats"][beat_idx] if beat_idx < len(analysis["beats"]) else 0
        for s in structure:
            if s["start"] <= bt < s["end"]:
                return s.get("section", "verse")
        return "verse"

    sorted_beats = sorted(selected_beats.keys())
    obstacles = []
    prev_lane = 1
    prev_shapes = []
    consecutive_shapes = 0

    for i, bi in enumerate(sorted_beats):
        event = selected_beats[bi]
        gap = (bi - sorted_beats[i-1]) if i > 0 else None
        sec = section_at(bi)

        # Restrict kinds based on section role AND difficulty
        role = SECTION_ROLE.get(sec, SECTION_ROLE["verse"])
        sec_kinds = set(role["types"]) & allowed

        # Variety injection: after N consecutive shape_gates, force a different type
        # BUT only if the section allows it and gap is large enough for a type switch
        force_variety = False
        if consecutive_shapes >= max_consecutive and gap is not None and gap >= 2:
            non_shape = sec_kinds - {"shape_gate"}
            if non_shape:
                force_variety = True

        obs = assign_obstacle(bi, event, gap, sec, difficulty,
                              prev_lane, prev_shapes, sec_kinds)

        if force_variety and obs["kind"] == "shape_gate":
            # Override to a non-shape type based on passes
            passes = set(event.get("passes", ["flux"])) if event else {"flux"}
            non_shape = sec_kinds - {"shape_gate"}
            if "lane_block" in non_shape:
                target_lane = prev_lane
                obs = {"beat": bi, "kind": "lane_block",
                       "blocked": [l for l in [0,1,2] if l != target_lane]}
            elif "low_bar" in non_shape and "high_bar" in non_shape:
                obs = {"beat": bi, "kind": "low_bar" if bi % 2 == 0 else "high_bar"}
            elif "low_bar" in non_shape:
                obs = {"beat": bi, "kind": "low_bar"}
            elif "high_bar" in non_shape:
                obs = {"beat": bi, "kind": "high_bar"}

        obstacles.append(obs)
        prev_lane = lane_of(obs, prev_lane)
        if obs["kind"] == "shape_gate":
            prev_shapes.append(obs.get("shape", "square"))
            consecutive_shapes += 1
        else:
            prev_shapes = []
            consecutive_shapes = 0

    return obstacles


# ═══════════════════════════════════════════════════════════════
# PHASE 3: CLEAN — fix violations (never insert, only remove/mutate)
# ═══════════════════════════════════════════════════════════════

def clean_lane_change_gap(obstacles):
    """RULE: Lane changes need ≥2 beats gap."""
    if not obstacles:
        return obstacles
    result = [obstacles[0]]
    prev_lane = lane_of(obstacles[0])
    for obs in obstacles[1:]:
        curr_lane = lane_of(obs, prev_lane)
        gap = obs["beat"] - result[-1]["beat"]
        if curr_lane != prev_lane and gap < 2:
            continue
        result.append(obs)
        prev_lane = curr_lane
    return result


def clean_type_transition(obstacles):
    """RULE: Switching shape↔movement needs ≥2 beats gap."""
    if not obstacles:
        return obstacles
    result = [obstacles[0]]
    prev_kind = obstacles[0]["kind"]
    prev_is_shape = prev_kind == "shape_gate"
    for obs in obstacles[1:]:
        curr_is_shape = obs["kind"] == "shape_gate"
        gap = obs["beat"] - result[-1]["beat"]
        if curr_is_shape != prev_is_shape and gap < 2:
            continue
        result.append(obs)
        prev_is_shape = curr_is_shape
    return result


def clean_two_lane_jumps(obstacles):
    """RULE: No shape_gate jumps > 1 lane."""
    if not obstacles:
        return obstacles
    result = [obstacles[0]]
    prev_lane = lane_of(obstacles[0])
    for obs in obstacles[1:]:
        if obs["kind"] == "shape_gate":
            obs_lane = obs.get("lane", 1)
            if abs(obs_lane - prev_lane) > 1:
                continue
        result.append(obs)
        prev_lane = lane_of(obs, prev_lane)
    return result


def clean_breathing_room(obstacles, boundary_beats, difficulty):
    """RULE: No obstacles at section boundaries."""
    if not boundary_beats or not obstacles:
        return obstacles
    protected = set()
    for bb in boundary_beats:
        if difficulty == "easy":
            protected.update([bb - 1, bb, bb + 1])
        else:
            protected.update([bb, bb + 1])
    return [obs for obs in obstacles if obs["beat"] not in protected]


def clean_gap_monotony(obstacles):
    """RULE: No single gap value > 40% of all gaps.
    Thin the most common gap by removing every 3rd occurrence."""
    if len(obstacles) < 5:
        return obstacles

    for _ in range(3):  # iterate until stable
        gaps = []
        for i in range(1, len(obstacles)):
            gaps.append(obstacles[i]["beat"] - obstacles[i-1]["beat"])
        if not gaps:
            break

        gap_counts = Counter(gaps)
        total = len(gaps)
        worst_gap, worst_count = gap_counts.most_common(1)[0]
        if worst_count / total <= 0.40:
            break  # passes

        # Remove every 3rd obstacle that creates this gap
        result = [obstacles[0]]
        consecutive = 0
        for i in range(1, len(obstacles)):
            g = obstacles[i]["beat"] - result[-1]["beat"]
            if g == worst_gap:
                consecutive += 1
                if consecutive % 3 == 0:
                    continue  # skip to thin
            else:
                consecutive = 0
            result.append(obstacles[i])
        obstacles = result

    return obstacles


def get_section_boundary_beats(analysis):
    """Map structure section boundaries to beat indices."""
    beats = analysis.get("beats", [])
    structure = analysis.get("structure", [])
    if not beats or not structure:
        return []
    boundaries = []
    for i in range(1, len(structure)):
        boundary_time = structure[i]["start"]
        best_idx = min(range(len(beats)), key=lambda j: abs(beats[j] - boundary_time))
        boundaries.append(best_idx)
    return boundaries


def clean_level(obstacles, difficulty, boundary_beats):
    """Run all cleaners. Order:
    1. Breathing room (section boundaries)
    2. Two-lane jumps (worst violations)
    3. Lane change gap (depends on lane tracking)
    4. Type transition (action family switches)
    5. Gap monotony (variety enforcement)
    """
    obstacles = clean_breathing_room(obstacles, boundary_beats, difficulty)
    obstacles = clean_two_lane_jumps(obstacles)
    obstacles = clean_lane_change_gap(obstacles)
    obstacles = clean_type_transition(obstacles)
    obstacles = clean_gap_monotony(obstacles)
    return obstacles


# ═══════════════════════════════════════════════════════════════
# PIPELINE: SELECT → ASSIGN → CLEAN
# ═══════════════════════════════════════════════════════════════

def design_level(analysis, difficulty):
    """Full pipeline: select beats, assign obstacles, clean violations."""
    selected, event_map = select_beats(analysis, difficulty)
    obstacles = assign_obstacles(selected, event_map, analysis, difficulty)
    boundary_beats = get_section_boundary_beats(analysis)
    obstacles = clean_level(obstacles, difficulty, boundary_beats)
    return obstacles


def build_beatmap(analysis, difficulties):
    """Build the full beatmap JSON."""
    beats = analysis["beats"]
    diff_data = {}
    for diff in difficulties:
        obs = design_level(analysis, diff)
        diff_data[diff] = {"beats": obs, "count": len(obs)}
    return {
        "song_id": analysis.get("title", "unknown"),
        "title": analysis.get("title", "unknown"),
        "bpm": analysis["bpm"],
        "offset": round(beats[0], 3) if beats else 0.0,
        "lead_beats": 4,
        "duration_sec": analysis.get("duration", 180),
        "difficulties": diff_data,
        "structure": analysis["structure"],
    }


# ═══════════════════════════════════════════════════════════════
# CLI + STATS
# ═══════════════════════════════════════════════════════════════

def print_stats(obstacles, diff_name):
    """Print quality metrics for a difficulty."""
    if not obstacles:
        print(f"\n  {diff_name.upper()}: 0 obstacles")
        return

    kinds = Counter(o["kind"] for o in obstacles)
    shapes = Counter(o.get("shape", "") for o in obstacles if o["kind"] == "shape_gate")
    gaps = [obstacles[i]["beat"] - obstacles[i-1]["beat"] for i in range(1, len(obstacles))]
    gap_dist = Counter(gaps)

    print(f"\n  {diff_name.upper()}: {len(obstacles)} obstacles")
    for k, v in sorted(kinds.items()):
        pct = 100 * v / len(obstacles)
        print(f"    {k:15s}: {v:3d} ({pct:.0f}%)")

    if shapes:
        print(f"    shapes: {dict(shapes)}")

    if gaps:
        max_gap_val, max_gap_count = gap_dist.most_common(1)[0]
        max_pct = 100 * max_gap_count / len(gaps)
        pps = len(obstacles) / (obstacles[-1]["beat"] - obstacles[0]["beat"] + 1) if len(obstacles) > 1 else 0
        print(f"    gaps: {dict(sorted(gap_dist.items()))}")
        print(f"    max gap dominance: {max_pct:.0f}% (gap={max_gap_val}) {'✓' if max_pct <= 40 else '✗ >40%'}")
        print(f"    density: {pps:.2f} obstacles/beat")

    # Check shape variety
    sg_count = kinds.get("shape_gate", 0)
    if sg_count > 0:
        sg_pct = 100 * sg_count / len(obstacles)
        target = 80 if diff_name == "medium" else (70 if diff_name == "hard" else 100)
        status = "✓" if sg_pct <= target else f"✗ >{target}%"
        print(f"    shape_gate%: {sg_pct:.0f}% {status}")


def main():
    parser = argparse.ArgumentParser(description="Music-driven beatmap generator")
    parser.add_argument("input", help="Path to analysis JSON")
    parser.add_argument("--output", "-o", default=None)
    parser.add_argument("--difficulty", "-d", choices=["easy","medium","hard","all"], default="all")
    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: {args.input} not found", file=sys.stderr)
        sys.exit(1)

    with open(args.input) as f:
        analysis = json.load(f)

    diffs = ["easy","medium","hard"] if args.difficulty == "all" else [args.difficulty]

    print("=" * 60)
    print(f"  LEVEL DESIGNER | {analysis.get('title','?')} | BPM {analysis['bpm']}")
    print("=" * 60)

    beatmap = build_beatmap(analysis, diffs)

    for diff in diffs:
        print_stats(beatmap["difficulties"][diff]["beats"], diff)

    out_path = args.output or f"{analysis.get('title','beatmap')}_beatmap.json"
    with open(out_path, "w") as f:
        json.dump(beatmap, f, indent=2)

    print(f"\n✓ {out_path}")
    print("=" * 60)


if __name__ == "__main__":
    main()
