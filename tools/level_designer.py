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
    python level_designer.py analysis.json --no-cleanup
    python level_designer.py analysis.json --diagnostics-out tools/diagnostics/song_loop1 --diagnostics-only
"""

import argparse
import csv
import hashlib
import json
import random
import sys
from pathlib import Path
from bisect import bisect_right
from collections import Counter, defaultdict


# ═══════════════════════════════════════════════════════════════
# CONSTANTS
# ═══════════════════════════════════════════════════════════════

SHAPE_TO_LANE = {"circle": 0, "square": 1, "triangle": 2}
LANE_TO_SHAPE = {0: "circle", 1: "square", 2: "triangle"}
ALL_SHAPES = ["circle", "square", "triangle"]
SUBDIVISION_FRACTIONS = [
    (0.0, "downbeat"),
    (1.0 / 3.0, "triplet"),
    (0.5, "eighth"),
    (2.0 / 3.0, "triplet"),
    (1.0, "downbeat"),
]
SUBDIVISION_TO_LANE = {
    "downbeat": 1,  # square (center lane)
    "eighth": 0,    # circle (left lane)
    "triplet": 2,   # triangle (right lane)
    "offgrid": 1,   # fallback
}

# Section role determines density and allowed obstacle types.
# Chorus/drop = densest but most CONSISTENT (nori-nori).
# Bridge = sparse, breathing room.
# Final section = hardest patterns (the "final fortress").
SECTION_ROLE = {
    "intro":      {"density": 0.45, "types": ["shape_gate"], "consistent": True},
    "verse":      {"density": 0.65, "types": ["shape_gate"], "consistent": False},
    "pre-chorus": {"density": 0.80, "types": ["shape_gate"], "consistent": False},
    "drop":       {"density": 0.90, "types": ["shape_gate"], "consistent": True},
    "bridge":     {"density": 0.35, "types": ["shape_gate"], "consistent": False},
}

DIFFICULTY_SCALE = {"easy": 0.55, "medium": 0.80, "hard": 1.20}
DIFFICULTY_INTRO_REST = {"easy": 8, "medium": 4, "hard": 2}
# Issue #175 — first-collision readability floor (seconds).
# The first authored obstacle must give the player time to register the song
# before they have to react. Measured as offset + first.beat * 60/bpm.
# See design-docs/rhythm-spec.md "First-collision floor (per difficulty)".
MIN_FIRST_COLLISION_SEC = {"easy": 4.0, "medium": 2.5, "hard": 2.0}
DIFFICULTY_KINDS = {
    "easy":   {"shape_gate"},
    "medium": {"shape_gate"},
    "hard":   {"shape_gate"},
}
UNREADABLE_KINDS = {"low_bar", "high_bar"}
MAX_EMPTY_GAP = {"easy": 40, "medium": 32, "hard": 30}
MAX_BEAT_DIFF = {diff: gap + 1 for diff, gap in MAX_EMPTY_GAP.items()}
MIN_SHAPE_CHANGE_GAP = 3
GAP_ONE_MEDIUM_START_PROGRESS = 0.30
GAP_ONE_HARD_MIN_BEAT = 11
GAP_ONE_MAX_RUN = {"medium": 1, "hard": 2}
GAP_ONE_SHARE_CAP = {"medium": 0.20, "hard": 0.20}
GAP_MONOTONY_CAP = {"medium": 0.40, "hard": 0.35}
MIN_IOI_MS = {"easy": 700.0, "medium": 380.0, "hard": 300.0}
MEDIUM_SHAPE_TARGETS = {
    0: (10, 20),  # Circle / lane 0
    1: (45, 60),  # Square / lane 1
    2: (25, 45),  # Triangle / lane 2
}
HARD_TRIANGLE_FLOOR_PCT = 25
HARD_CIRCLE_CEIL_PCT = 40

# Shape palette per section: controls how many shapes are in play.
# Intro uses all lanes for early variety; bridge stays centered for a breather.
# Verse = 2 shapes (alternating, player grooves).
# Pre-chorus/drop = all 3 (full variety, player is warmed up).
SECTION_SHAPE_PALETTE = {
    "intro":      [0, 1, 2],
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
    for idx, ev in enumerate(events):
        lo, hi, best, best_d = 0, len(beats)-1, None, tolerance+1
        while lo <= hi:
            mid = (lo+hi)//2
            d = abs(beats[mid] - ev["t"])
            if d < best_d: best_d, best = d, mid
            if beats[mid] < ev["t"]: lo = mid+1
            else: hi = mid-1
        if best is not None and best_d <= tolerance and best not in used:
            subdivision, lane_hint = classify_subdivision(ev["t"], beats, best)
            used.add(best)
            snapped.append({
                **ev,
                "source_event_idx": idx,
                "beat_idx": best,
                "beat_time": beats[best],
                "subdivision": subdivision,
                "subdivision_lane": lane_hint,
            })
    snapped.sort(key=lambda e: e["beat_idx"])
    return snapped


def classify_subdivision(event_time, beats, beat_idx):
    """Classify event position inside a beat interval and map it to a lane."""
    if len(beats) < 2:
        return "downbeat", SUBDIVISION_TO_LANE["downbeat"]

    left_idx = max(0, beat_idx - 1)
    right_idx = min(len(beats) - 1, beat_idx + 1)

    if beat_idx < len(beats) - 1 and event_time >= beats[beat_idx]:
        left, right = beats[beat_idx], beats[beat_idx + 1]
    else:
        left, right = beats[left_idx], beats[beat_idx]

    span = max(1e-6, right - left)
    phase = min(max((event_time - left) / span, 0.0), 1.0)

    closest_frac, closest_label = min(
        SUBDIVISION_FRACTIONS, key=lambda item: abs(item[0] - phase)
    )
    if abs(closest_frac - phase) > 0.22:
        closest_label = "offgrid"

    return closest_label, SUBDIVISION_TO_LANE.get(closest_label, 1)


def _percentile(sorted_values, q):
    if not sorted_values:
        return 0.0
    if len(sorted_values) == 1:
        return float(sorted_values[0])
    pos = (len(sorted_values) - 1) * q
    lo = int(pos)
    hi = min(lo + 1, len(sorted_values) - 1)
    frac = pos - lo
    return float(sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * frac)


def _intensity_confidence(intensity):
    return {"low": 0.33, "medium": 0.66, "high": 1.0}.get(str(intensity), None)


def _candidate_fraction_labels(mode):
    if mode == "quarter":
        return [(0.0, "downbeat"), (1.0, "downbeat")]
    if mode == "eighth":
        return [(0.0, "downbeat"), (0.5, "eighth"), (1.0, "downbeat")]
    if mode == "triplet":
        return [(0.0, "downbeat"), (1.0 / 3.0, "triplet"), (2.0 / 3.0, "triplet"), (1.0, "downbeat")]
    raise ValueError(f"Unsupported candidate mode: {mode}")


def snap_events_to_grid_candidates(events, beats, mode):
    """Snap each event to the nearest grid point on quarter/eighth/triplet grids."""
    if len(beats) < 2:
        return []

    snapped = []
    fractions = _candidate_fraction_labels(mode)

    for event_idx, ev in enumerate(events):
        event_time = float(ev.get("t", 0.0))
        beat_idx = bisect_right(beats, event_time) - 1
        beat_idx = min(max(beat_idx, 0), len(beats) - 2)
        left = float(beats[beat_idx])
        right = float(beats[beat_idx + 1])
        span = max(1e-6, right - left)

        best_fraction, subdivision = min(
            fractions,
            key=lambda item: abs((left + (span * item[0])) - event_time),
        )
        grid_time = left + (span * best_fraction)
        residual_ms = (event_time - grid_time) * 1000.0

        snapped.append({
            "candidate": mode,
            "event_idx": event_idx,
            "event_time": round(event_time, 6),
            "beat_idx": int(beat_idx),
            "grid_time": round(grid_time, 6),
            "subdivision": subdivision,
            "residual_ms": round(residual_ms, 3),
            "abs_residual_ms": round(abs(residual_ms), 3),
            "flux": ev.get("flux"),
            "intensity": ev.get("intensity"),
            "intensity_confidence": _intensity_confidence(ev.get("intensity")),
            "pass_count": len(ev.get("passes", [])) if isinstance(ev.get("passes"), list) else None,
        })

    return snapped


def _residual_summary(snapped_rows):
    abs_residuals = sorted(float(row["abs_residual_ms"]) for row in snapped_rows)
    if not abs_residuals:
        return {
            "count": 0,
            "median_abs_ms": 0.0,
            "p90_abs_ms": 0.0,
            "mean_abs_ms": 0.0,
            "within_20ms": 0,
            "within_35ms": 0,
            "within_50ms": 0,
            "within_80ms": 0,
        }

    return {
        "count": len(abs_residuals),
        "median_abs_ms": round(_percentile(abs_residuals, 0.5), 3),
        "p90_abs_ms": round(_percentile(abs_residuals, 0.9), 3),
        "mean_abs_ms": round(sum(abs_residuals) / len(abs_residuals), 3),
        "within_20ms": sum(1 for x in abs_residuals if x <= 20.0),
        "within_35ms": sum(1 for x in abs_residuals if x <= 35.0),
        "within_50ms": sum(1 for x in abs_residuals if x <= 50.0),
        "within_80ms": sum(1 for x in abs_residuals if x <= 80.0),
    }


def _gap_histogram(snapped_rows):
    if not snapped_rows:
        return {}
    times = sorted(float(row["grid_time"]) for row in snapped_rows)
    if len(times) < 2:
        return {}
    buckets = defaultdict(int)
    for i in range(1, len(times)):
        gap_ms = (times[i] - times[i - 1]) * 1000.0
        bucket_floor = int(gap_ms // 50) * 50
        bucket = f"{bucket_floor:04d}-{bucket_floor + 49:04d}"
        buckets[bucket] += 1
    return dict(sorted(buckets.items()))


def _same_shape_run_metrics(obstacles):
    longest = 0
    runs_ge_3 = 0
    current_shape = None
    current_len = 0
    for obs in sorted(obstacles, key=lambda x: x["beat"]):
        if obs.get("kind") != "shape_gate":
            current_shape = None
            current_len = 0
            continue
        shape = obs.get("shape")
        if shape == current_shape:
            current_len += 1
        else:
            current_shape = shape
            current_len = 1
        if current_len >= 3:
            runs_ge_3 += 1
        longest = max(longest, current_len)
    return {"longest_run": longest, "run_steps_ge_3": runs_ge_3}


def _baseline_same_shape_metrics(analysis):
    metrics = {}
    for diff in ("easy", "medium", "hard"):
        obstacles = design_level(analysis, diff, cleanup_enabled=True)
        metrics[diff] = _same_shape_run_metrics(obstacles)
    return metrics


def write_snap_diagnostics(analysis, diagnostics_out_dir):
    """Emit subdivision-aware diagnostics artifacts without changing generation."""
    out_dir = Path(diagnostics_out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    events = analysis.get("events", [])
    beats = analysis.get("beats", [])
    if not events or not beats:
        raise ValueError("analysis must include non-empty events and beats for diagnostics")

    current_snap = snap_events_to_beats(events, beats)
    current_rows = []
    for i, row in enumerate(current_snap):
        event_time = float(row.get("t", 0.0))
        beat_time = float(row.get("beat_time", 0.0))
        residual_ms = (event_time - beat_time) * 1000.0
        current_rows.append({
            "candidate": "current_quarter_snap",
            "event_idx": int(row.get("source_event_idx", i)),
            "event_time": round(event_time, 6),
            "beat_idx": int(row.get("beat_idx", 0)),
            "grid_time": round(beat_time, 6),
            "subdivision": row.get("subdivision", "downbeat"),
            "residual_ms": round(residual_ms, 3),
            "abs_residual_ms": round(abs(residual_ms), 3),
            "flux": row.get("flux"),
            "intensity": row.get("intensity"),
            "intensity_confidence": _intensity_confidence(row.get("intensity")),
            "pass_count": len(row.get("passes", [])) if isinstance(row.get("passes"), list) else None,
        })

    candidate_rows = {
        "current_quarter_snap": current_rows,
        "quarter_grid": snap_events_to_grid_candidates(events, beats, "quarter"),
        "eighth_grid": snap_events_to_grid_candidates(events, beats, "eighth"),
        "triplet_grid": snap_events_to_grid_candidates(events, beats, "triplet"),
    }

    flat_rows = []
    for rows in candidate_rows.values():
        flat_rows.extend(rows)
    fields = [
        "candidate",
        "event_idx",
        "event_time",
        "beat_idx",
        "grid_time",
        "subdivision",
        "residual_ms",
        "abs_residual_ms",
        "flux",
        "intensity",
        "intensity_confidence",
        "pass_count",
    ]
    with (out_dir / "snap_candidate_events.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(flat_rows)

    residual_summary = {}
    subdivision_histogram = {}
    gap_histogram = {}
    for candidate, rows in candidate_rows.items():
        residual_summary[candidate] = _residual_summary(rows)
        subdivision_histogram[candidate] = dict(
            sorted(Counter(row["subdivision"] for row in rows).items())
        )
        gap_histogram[candidate] = _gap_histogram(rows)

    summary = {
        "song_id": analysis.get("title", "unknown"),
        "residual_summary": residual_summary,
        "subdivision_histogram": subdivision_histogram,
        "gap_histogram_50ms_bins": gap_histogram,
        "same_shape_run_metrics": _baseline_same_shape_metrics(analysis),
    }

    with (out_dir / "snap_diagnostics_summary.json").open("w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    return out_dir


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


def deterministic_seed(analysis, difficulty):
    """Stable per-song/per-difficulty seed for bounded random choices."""
    beats = analysis.get("beats") or []
    seed_material = "|".join([
        str(analysis.get("song_id", analysis.get("title", "unknown"))),
        str(difficulty),
        str(analysis.get("bpm", "")),
        str(len(beats)),
        f"{float(beats[0]):.6f}" if beats else "0.000000",
    ])
    digest = hashlib.sha256(seed_material.encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big")


def lane_of(obs, fallback=1):
    """What lane does this obstacle put the player in?"""
    if obs["kind"] == "shape_gate":
        return obs.get("lane", 1)
    return fallback


def is_shape_gate(obs):
    return obs.get("kind") == "shape_gate"


def is_unreadable_kind(obs):
    return obs.get("kind") in UNREADABLE_KINDS


def clamp_lane(lane):
    return min(max(int(lane), 0), 2)


def set_shape_gate(obs, shape):
    """Mutate an obstacle into a canonical shape_gate."""
    if shape not in SHAPE_TO_LANE:
        shape = "square"
    obs["kind"] = "shape_gate"
    obs["shape"] = shape
    obs["lane"] = SHAPE_TO_LANE[shape]


def nearest_shape_lane(obstacles, index):
    """Find the nearest authored shape/lane around index."""
    for j in range(index - 1, -1, -1):
        if is_shape_gate(obstacles[j]):
            shape = obstacles[j].get("shape", "square")
            if shape in SHAPE_TO_LANE:
                return shape, SHAPE_TO_LANE[shape]
    for j in range(index + 1, len(obstacles)):
        if is_shape_gate(obstacles[j]):
            shape = obstacles[j].get("shape", "square")
            if shape in SHAPE_TO_LANE:
                return shape, SHAPE_TO_LANE[shape]
    if 0 <= index < len(obstacles):
        lane = clamp_lane(lane_of(obstacles[index], 1))
    else:
        lane = 1
    return LANE_TO_SHAPE[lane], lane


def shape_gate_clusters(obstacles):
    """Shape gates connected by <3-beat gaps must share one shape."""
    clusters = []
    current = []
    prev_beat = None
    for i, obs in enumerate(obstacles):
        if not is_shape_gate(obs):
            continue
        beat = obs["beat"]
        if prev_beat is None or beat - prev_beat >= MIN_SHAPE_CHANGE_GAP:
            if current:
                clusters.append(current)
            current = [i]
        else:
            current.append(i)
        prev_beat = beat
    if current:
        clusters.append(current)
    return clusters


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
                    prev_lane, prev_shapes, allowed_kinds, rng, shape_state):
    """Assign obstacle type from beat-local context only (no onset pass mapping)."""
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
        # Flow-based shape picker with bounded randomness:
        # 1. Follow section-local cycling as the baseline
        # 2. Blend in subdivision / groove hints with small deterministic jitter
        # 3. Cap repeated same-shape runs for readability
        palette = section_shape_palette_for_difficulty(section_name, difficulty)
        subdivision_lane = event.get("subdivision_lane") if event else None
        prev_shape_lane = SHAPE_TO_LANE.get(prev_shapes[-1], prev_lane) if prev_shapes else prev_lane
        max_same_shape_run = 2

        if len(palette) == 1:
            lane = palette[0]
        else:
            cycle_idx = shape_state.get("cycle_idx", 0)
            cycle_lane = palette[cycle_idx % len(palette)]
            candidates = [cycle_lane]

            if subdivision_lane in palette and (gap_to_prev is None or gap_to_prev >= 2):
                candidates.append(subdivision_lane)

            if prev_shapes and gap_to_prev is not None and gap_to_prev <= 2:
                hold_weight = 0.75 if gap_to_prev <= 1 else 0.35
                if rng.random() < hold_weight:
                    candidates.insert(0, prev_shape_lane)

            if len(palette) > 1 and rng.random() < 0.25:
                candidates.append(palette[(cycle_idx + 1) % len(palette)])

            deduped = []
            for candidate in candidates:
                if candidate in palette and candidate not in deduped:
                    deduped.append(candidate)
            lane = deduped[0] if deduped else cycle_lane

            same_shape_run = shape_state.get("same_shape_run", 0)
            if prev_shapes and lane == prev_shape_lane and same_shape_run >= max_same_shape_run:
                lane = next((p for p in palette if p != prev_shape_lane), cycle_lane)

            shape_state["cycle_idx"] = (cycle_idx + 1) % len(palette)

        # Keep lane adjacent to previous (no 2-lane jumps)
        if abs(lane - prev_lane) > 1:
            if lane > prev_lane:
                lane = prev_lane + 1
            else:
                lane = prev_lane - 1

        same_shape_run = shape_state.get("same_shape_run", 0)
        if prev_shapes and lane == prev_shape_lane and same_shape_run >= max_same_shape_run:
            adjacent = [p for p in palette if p != prev_shape_lane and abs(p - prev_lane) <= 1]
            if adjacent:
                lane = adjacent[0]
            elif difficulty in ("medium", "hard"):
                non_adjacent = [p for p in palette if p != prev_shape_lane]
                if non_adjacent:
                    lane = non_adjacent[0]

        shape = LANE_TO_SHAPE[lane]

        if prev_shapes and shape == prev_shapes[-1]:
            shape_state["same_shape_run"] = shape_state.get("same_shape_run", 0) + 1
        else:
            shape_state["same_shape_run"] = 1

        obs["shape"] = shape
        obs["lane"] = lane

    return obs


def section_shape_palette_for_difficulty(section_name, difficulty):
    """Return section palette with minimal difficulty-aware variety safeguards."""
    base = list(SECTION_SHAPE_PALETTE.get(section_name, [0, 1, 2]))
    if difficulty == "hard":
        return [0, 1, 2]
    return base


def assign_obstacles(selected_beats, event_map, analysis, difficulty):
    """Assign obstacle types to all selected beats."""
    del event_map
    structure = analysis["structure"]
    allowed = DIFFICULTY_KINDS[difficulty]

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
    rng = random.Random(deterministic_seed(analysis, difficulty))
    shape_state = {"cycle_idx": 0, "same_shape_run": 0}

    for i, bi in enumerate(sorted_beats):
        event = selected_beats[bi]
        gap = (bi - sorted_beats[i-1]) if i > 0 else None
        sec = section_at(bi)

        # Restrict kinds based on section role AND difficulty
        role = SECTION_ROLE.get(sec, SECTION_ROLE["verse"])
        sec_kinds = set(role["types"]) & allowed

        obs = assign_obstacle(
            bi,
            event,
            gap,
            sec,
            difficulty,
            prev_lane,
            prev_shapes,
            sec_kinds,
            rng,
            shape_state,
        )

        obstacles.append(obs)
        prev_lane = lane_of(obs, prev_lane)
        if obs["kind"] == "shape_gate":
            prev_shapes.append(obs.get("shape", "square"))
        else:
            prev_shapes = []
            shape_state["same_shape_run"] = 0

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


def clean_minimum_gap(obstacles, difficulty):
    """RULE: Minimum beat gap between any two obstacles.
    Easy keeps gap≥2 for accessibility. Medium/hard allow gap=1
    since collision timing uses BeatInfo.arrival_time (BPM-independent)."""
    MIN_GAP = {"easy": 2, "medium": 1, "hard": 1}
    min_gap = MIN_GAP.get(difficulty, 2)
    if not obstacles or min_gap <= 1:
        return obstacles
    result = [obstacles[0]]
    for obs in obstacles[1:]:
        gap = obs["beat"] - result[-1]["beat"]
        if gap < min_gap:
            continue
        result.append(obs)
    return result


def clean_shape_change_gap(obstacles):
    """RULE: Different shape gates must be at least 3 beats apart.

    Consecutive shape gates separated by 1-2 beats form a readability cluster.
    The runtime validator allows those dense clusters only when all gates use
    the same shape, so normalize each cluster to its majority shape.
    """
    if not obstacles:
        return obstacles

    for cluster in shape_gate_clusters(obstacles):
        shapes = [
            obstacles[i].get("shape", "square")
            for i in cluster
            if obstacles[i].get("shape", "square") in SHAPE_TO_LANE
        ]
        shape = Counter(shapes).most_common(1)[0][0] if shapes else "square"
        for i in cluster:
            set_shape_gate(obstacles[i], shape)
    return obstacles


def is_readable_gap_one_family(left, right):
    """gap=1 is readable only for identical shape_gate shape/lane pairs."""
    return (
        is_shape_gate(left)
        and is_shape_gate(right)
        and left.get("shape") == right.get("shape")
        and left.get("lane") == right.get("lane")
    )


def has_readable_gap_one_neighbors(previous, left, right, following):
    """No bar obstacle may sit within 2 beats around a gap=1 pair."""
    if previous and left["beat"] - previous["beat"] <= 2 and is_unreadable_kind(previous):
        return False
    if following and following["beat"] - right["beat"] <= 2 and is_unreadable_kind(following):
        return False
    return True


def clean_gap_one_early(obstacles, difficulty):
    """RULE: gap=1 pairs must obey per-difficulty readability policy."""
    if len(obstacles) < 2:
        return obstacles
    if difficulty not in ("easy", "medium", "hard"):
        return obstacles

    obstacles = sorted(obstacles, key=lambda obs: obs["beat"])
    for _ in range(len(obstacles)):
        changed = False
        last_beat = max(1, max(obs["beat"] for obs in obstacles))
        result = [obstacles[0]]
        gap_one_run = 0

        for i in range(1, len(obstacles)):
            left = result[-1]
            right = obstacles[i]
            gap = right["beat"] - left["beat"]

            if gap <= 0:
                changed = True
                continue

            if gap != 1:
                result.append(right)
                gap_one_run = 0
                continue

            previous = result[-2] if len(result) > 1 else None
            following = obstacles[i + 1] if i + 1 < len(obstacles) else None

            violation = False
            if difficulty == "easy":
                violation = True
            elif difficulty == "medium":
                progress = left["beat"] / last_beat
                violation = (
                    progress <= GAP_ONE_MEDIUM_START_PROGRESS
                    or gap_one_run >= GAP_ONE_MAX_RUN["medium"]
                )
            else:
                violation = (
                    left["beat"] < GAP_ONE_HARD_MIN_BEAT
                    or gap_one_run >= GAP_ONE_MAX_RUN["hard"]
                )

            if not is_readable_gap_one_family(left, right):
                violation = True
            if not has_readable_gap_one_neighbors(previous, left, right, following):
                violation = True

            if violation:
                changed = True
                continue

            result.append(right)
            gap_one_run += 1

        obstacles = result
        if not changed:
            break

    return obstacles


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


def clean_gap_monotony(obstacles, difficulty):
    """Reduce dominant-gap monotony with deterministic local redistribution."""
    if len(obstacles) < 5:
        return obstacles

    cap = GAP_MONOTONY_CAP.get(difficulty)
    if cap is None:
        return obstacles

    max_diff = MAX_BEAT_DIFF.get(difficulty, 999)
    for _ in range(len(obstacles) * 3):
        gaps = []
        for i in range(1, len(obstacles)):
            gaps.append(obstacles[i]["beat"] - obstacles[i-1]["beat"])
        if not gaps:
            break

        gap_counts = Counter(gaps)
        total = len(gaps)
        worst_gap, worst_count = gap_counts.most_common(1)[0]
        if worst_count / total < cap:
            break

        changed = False

        # First prefer beat shifts that keep obstacle count unchanged.
        for i in range(1, len(obstacles) - 1):
            left = obstacles[i]["beat"] - obstacles[i - 1]["beat"]
            right = obstacles[i + 1]["beat"] - obstacles[i]["beat"]
            if left == worst_gap and right >= worst_gap + 2:
                candidate = obstacles[i]["beat"] + 1
                if candidate < obstacles[i + 1]["beat"] and (right - 1) != worst_gap:
                    obstacles[i]["beat"] = candidate
                    changed = True
                    break
            if right == worst_gap and left >= worst_gap + 2:
                candidate = obstacles[i]["beat"] - 1
                if candidate > obstacles[i - 1]["beat"] and (left - 1) != worst_gap:
                    obstacles[i]["beat"] = candidate
                    changed = True
                    break

        if changed:
            continue

        # Fallback: remove only the center of worst_gap + worst_gap pairs.
        for i in range(1, len(obstacles) - 1):
            left = obstacles[i]["beat"] - obstacles[i - 1]["beat"]
            right = obstacles[i + 1]["beat"] - obstacles[i]["beat"]
            if left == worst_gap and right == worst_gap:
                merged = obstacles[i + 1]["beat"] - obstacles[i - 1]["beat"]
                if merged > max_diff:
                    continue
                if merged <= max_diff:
                    del obstacles[i]
                    changed = True
                    break

        if not changed:
            break

    return obstacles


def clean_min_ioi(obstacles, difficulty, analysis):
    """Ensure authored inter-onset intervals clear per-difficulty floor."""
    floor = MIN_IOI_MS.get(difficulty)
    beats = analysis.get("beats", [])
    if floor is None or len(obstacles) < 2 or not beats:
        return obstacles

    result = list(obstacles)
    while len(result) >= 2:
        changed = False
        for i in range(1, len(result)):
            left = result[i - 1]["beat"]
            right = result[i]["beat"]
            if left < 0 or right < 0 or left >= len(beats) or right >= len(beats):
                continue
            dt_ms = (float(beats[right]) - float(beats[left])) * 1000.0
            if dt_ms < floor:
                del result[i]
                changed = True
                break
        if not changed:
            break
    return result


def clean_gap_one_share(obstacles, difficulty):
    """Trim excess gap=1 usage while preserving beat-grid semantics."""
    cap = GAP_ONE_SHARE_CAP.get(difficulty)
    max_diff = MAX_BEAT_DIFF.get(difficulty, 999)
    if cap is None or len(obstacles) < 3:
        return obstacles

    result = list(obstacles)
    for _ in range(len(result)):
        beats = [obs["beat"] for obs in result]
        gaps = [beats[i] - beats[i - 1] for i in range(1, len(beats))]
        if not gaps:
            break
        gap_counts = Counter(gaps)
        if (gap_counts.get(1, 0) / len(gaps)) <= cap:
            break

        changed = False
        for i in range(1, len(result) - 1):
            left = result[i]["beat"] - result[i - 1]["beat"]
            if left != 1:
                continue
            merged = result[i + 1]["beat"] - result[i - 1]["beat"]
            if merged <= max_diff:
                del result[i]
                changed = True
                break
        if not changed:
            break

    return result


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
    1. Minimum gap
    2. Breathing room (section boundaries)
    3. Two-lane jumps (worst violations)
    4. Lane change gap (depends on lane tracking)
    5. Type transition (action family switches)
    6. Shape-change gap
    7. gap=1 readability
    8. Gap monotony (variety enforcement)
    """
    obstacles = clean_minimum_gap(obstacles, difficulty)
    obstacles = clean_breathing_room(obstacles, boundary_beats, difficulty)
    obstacles = clean_two_lane_jumps(obstacles)
    obstacles = clean_lane_change_gap(obstacles)
    obstacles = clean_type_transition(obstacles)
    obstacles = clean_shape_change_gap(obstacles)
    obstacles = clean_gap_one_early(obstacles, difficulty)
    obstacles = clean_gap_monotony(obstacles, difficulty)
    return obstacles


def fill_max_gaps(obstacles, difficulty, analysis):
    """Insert shape gates so consecutive authored beats stay under max-gap."""
    del analysis
    max_diff = MAX_BEAT_DIFF.get(difficulty)
    if not max_diff or len(obstacles) < 2:
        return obstacles

    obstacles = sorted(obstacles, key=lambda obs: obs["beat"])
    result = []
    for i, obs in enumerate(obstacles[:-1]):
        next_obs = obstacles[i + 1]
        result.append(obs)

        diff = next_obs["beat"] - obs["beat"]
        if diff <= max_diff:
            continue

        shape, lane = nearest_shape_lane(obstacles, i)
        segments = (diff + max_diff - 1) // max_diff
        for segment in range(1, segments):
            beat = obs["beat"] + int(round(diff * segment / segments))
            result.append({
                "beat": beat,
                "kind": "shape_gate",
                "shape": shape,
                "lane": lane,
            })

    result.append(obstacles[-1])
    return result


def medium_shape_counts(obstacles):
    counts = {0: 0, 1: 0, 2: 0}
    for obs in obstacles:
        if not is_shape_gate(obs):
            continue
        shape = obs.get("shape", "square")
        lane = SHAPE_TO_LANE.get(shape, clamp_lane(obs.get("lane", 1)))
        counts[lane] += 1
    total = sum(counts.values())
    return counts, total


def longest_same_shape_run_from_clusters(cluster_lanes, cluster_sizes):
    longest = 0
    run = 0
    prev_lane = None
    for lane, size in zip(cluster_lanes, cluster_sizes):
        if lane == prev_lane:
            run += size
        else:
            run = size
            prev_lane = lane
        if run > longest:
            longest = run
    return longest


def target_count_range(shape_idx, total):
    min_pct, max_pct = MEDIUM_SHAPE_TARGETS[shape_idx]
    return (min_pct * total + 99) // 100, (max_pct * total) // 100


def shape_balance_score(counts, total):
    score = 0
    for shape_idx in range(3):
        low, high = target_count_range(shape_idx, total)
        if counts[shape_idx] < low:
            score += low - counts[shape_idx]
        elif counts[shape_idx] > high:
            score += counts[shape_idx] - high
    return score


def shape_cluster_lane(obstacles, cluster):
    shape = obstacles[cluster[0]].get("shape", "square")
    return SHAPE_TO_LANE.get(shape, 1)


def easy_shape_score(counts, total):
    if total == 0:
        return 0
    max_allowed = (65 * total) // 100
    missing = sum(1 for shape_idx in range(3) if counts[shape_idx] == 0)
    dominant = sum(max(0, counts[shape_idx] - max_allowed) for shape_idx in range(3))
    return missing + dominant


def rebalance_easy_shapes(obstacles, difficulty):
    """Keep easy shape variety valid after dense clusters are normalized."""
    if difficulty != "easy" or not obstacles:
        return obstacles

    obstacles = clean_shape_change_gap(obstacles)
    seen = set()
    for _ in range(200):
        counts, total = medium_shape_counts(obstacles)
        if total == 0 or easy_shape_score(counts, total) == 0:
            break

        key = tuple(counts[i] for i in range(3))
        if key in seen:
            break
        seen.add(key)

        max_allowed = (65 * total) // 100
        donors = [
            shape_idx for shape_idx in range(3)
            if counts[shape_idx] > max_allowed
        ]
        if not donors:
            donors = [
                max(range(3), key=lambda shape_idx: counts[shape_idx])
            ]

        recipients = [
            shape_idx for shape_idx in range(3)
            if shape_idx not in donors and counts[shape_idx] < max_allowed
        ]
        recipients.sort(key=lambda shape_idx: counts[shape_idx])
        if not recipients:
            break

        current_score = easy_shape_score(counts, total)
        best = None
        best_score = current_score
        clusters = shape_gate_clusters(obstacles)

        for donor in donors:
            donor_excess = max(1, counts[donor] - max_allowed)
            for under in recipients:
                for cluster in clusters:
                    if shape_cluster_lane(obstacles, cluster) != donor:
                        continue
                    size = len(cluster)
                    new_counts = dict(counts)
                    new_counts[donor] -= size
                    new_counts[under] += size
                    new_score = easy_shape_score(new_counts, total)
                    tie = (abs(size - donor_excess), size)
                    if (
                        new_score < best_score
                        or (new_score == best_score and best and tie < best[2])
                    ):
                        best = (cluster, under, tie)
                        best_score = new_score

        if best is None:
            break

        cluster, shape_idx, _ = best
        shape = LANE_TO_SHAPE[shape_idx]
        for i in cluster:
            set_shape_gate(obstacles[i], shape)

    return clean_shape_change_gap(obstacles)


def rebalance_medium_shapes(obstacles, difficulty):
    """Mutate medium shape clusters until shape/lane counts enter targets."""
    if difficulty != "medium" or not obstacles:
        return obstacles

    obstacles = clean_shape_change_gap(obstacles)
    seen = set()
    for _ in range(200):
        counts, total = medium_shape_counts(obstacles)
        if total == 0 or shape_balance_score(counts, total) == 0:
            break

        key = tuple(counts[i] for i in range(3))
        if key in seen:
            break
        seen.add(key)

        under_shapes = []
        donor_shapes = []
        for shape_idx in range(3):
            low, high = target_count_range(shape_idx, total)
            if counts[shape_idx] < low:
                under_shapes.append((low - counts[shape_idx], shape_idx))
            if counts[shape_idx] > high:
                donor_shapes.append((counts[shape_idx] - high, shape_idx))

        under_shapes.sort(reverse=True)
        donor_shapes.sort(reverse=True)
        if under_shapes:
            under = under_shapes[0][1]
            under_need = under_shapes[0][0]
        elif donor_shapes:
            donor_set = {shape_idx for _, shape_idx in donor_shapes}
            recipients = []
            for shape_idx in range(3):
                if shape_idx in donor_set:
                    continue
                _, high = target_count_range(shape_idx, total)
                headroom = high - counts[shape_idx]
                if headroom > 0:
                    recipients.append((headroom, shape_idx))
            if not recipients:
                break
            recipients.sort(reverse=True)
            under = recipients[0][1]
            under_need = min(donor_shapes[0][0], recipients[0][0])
        else:
            break

        if donor_shapes:
            donors = [shape_idx for _, shape_idx in donor_shapes]
        else:
            donors = [
                shape_idx for shape_idx in range(3)
                if shape_idx != under
                and counts[shape_idx] > target_count_range(shape_idx, total)[0]
            ]

        clusters = shape_gate_clusters(obstacles)
        cluster_lanes = [shape_cluster_lane(obstacles, cluster) for cluster in clusters]
        cluster_sizes = [len(cluster) for cluster in clusters]
        current_score = shape_balance_score(counts, total)
        current_run = longest_same_shape_run_from_clusters(cluster_lanes, cluster_sizes)
        best = None
        best_obj = (current_score, current_run)
        best_tie = None

        for donor in donors:
            for cluster_idx, cluster in enumerate(clusters):
                if shape_cluster_lane(obstacles, cluster) != donor:
                    continue
                size = len(cluster)
                new_counts = dict(counts)
                new_counts[donor] -= size
                new_counts[under] += size
                candidate_lanes = list(cluster_lanes)
                candidate_lanes[cluster_idx] = under
                new_run = longest_same_shape_run_from_clusters(candidate_lanes, cluster_sizes)
                new_obj = (shape_balance_score(new_counts, total), new_run)
                tie = (abs(size - under_need), size)
                if new_obj < best_obj or (new_obj == best_obj and best_tie and tie < best_tie):
                    best = (cluster, under)
                    best_obj = new_obj
                    best_tie = tie

        if best is None:
            break

        cluster, shape_idx = best
        shape = LANE_TO_SHAPE[shape_idx]
        for i in cluster:
            set_shape_gate(obstacles[i], shape)

    return clean_shape_change_gap(obstacles)


def hard_shape_score(counts, total):
    if total == 0:
        return 0
    triangle_min = (HARD_TRIANGLE_FLOOR_PCT * total + 99) // 100
    circle_max = (HARD_CIRCLE_CEIL_PCT * total) // 100
    return max(0, triangle_min - counts[2]) + max(0, counts[0] - circle_max)


def rebalance_hard_shapes(obstacles, difficulty):
    """Mutate hard shape clusters to satisfy triangle floor / circle ceiling."""
    if difficulty != "hard" or not obstacles:
        return obstacles

    obstacles = clean_shape_change_gap(obstacles)
    seen = set()
    for _ in range(200):
        counts, total = medium_shape_counts(obstacles)
        if total == 0 or hard_shape_score(counts, total) == 0:
            break

        key = tuple(counts[i] for i in range(3))
        if key in seen:
            break
        seen.add(key)

        triangle_min = (HARD_TRIANGLE_FLOOR_PCT * total + 99) // 100
        circle_max = (HARD_CIRCLE_CEIL_PCT * total) // 100
        triangle_need = max(0, triangle_min - counts[2])
        circle_excess = max(0, counts[0] - circle_max)

        recipient = 2 if triangle_need > 0 else 1
        donors = [0, 1] if circle_excess > 0 else [1, 0]

        clusters = shape_gate_clusters(obstacles)
        cluster_lanes = [shape_cluster_lane(obstacles, cluster) for cluster in clusters]
        cluster_sizes = [len(cluster) for cluster in clusters]
        current_score = hard_shape_score(counts, total)
        current_run = longest_same_shape_run_from_clusters(cluster_lanes, cluster_sizes)
        best = None
        best_obj = (current_score, current_run)
        best_tie = None

        for donor in donors:
            if donor == recipient:
                continue
            for cluster_idx, cluster in enumerate(clusters):
                if shape_cluster_lane(obstacles, cluster) != donor:
                    continue
                size = len(cluster)
                new_counts = dict(counts)
                new_counts[donor] -= size
                new_counts[recipient] += size
                candidate_lanes = list(cluster_lanes)
                candidate_lanes[cluster_idx] = recipient
                new_run = longest_same_shape_run_from_clusters(candidate_lanes, cluster_sizes)
                new_obj = (hard_shape_score(new_counts, total), new_run)
                tie = (abs(size - triangle_need), size)
                if (
                    new_obj < best_obj
                    or (new_obj == best_obj and (best_tie is None or tie < best_tie))
                ):
                    best = (cluster, recipient)
                    best_obj = new_obj
                    best_tie = tie

        if best is None:
            break

        cluster, shape_idx = best
        shape = LANE_TO_SHAPE[shape_idx]
        for i in cluster:
            set_shape_gate(obstacles[i], shape)

    return clean_shape_change_gap(obstacles)


# ═══════════════════════════════════════════════════════════════
# PIPELINE: SELECT → ASSIGN → CLEAN
# ═══════════════════════════════════════════════════════════════

def design_level(analysis, difficulty, cleanup_enabled=True):
    """Full pipeline: select beats, assign obstacles, clean violations."""
    selected, event_map = select_beats(analysis, difficulty)
    obstacles = assign_obstacles(selected, event_map, analysis, difficulty)
    if not cleanup_enabled:
        return obstacles
    boundary_beats = get_section_boundary_beats(analysis)
    obstacles = clean_level(obstacles, difficulty, boundary_beats)
    # Issue #175 — guarantee per-difficulty first-collision reaction floor.
    obstacles = enforce_first_collision_floor(obstacles, difficulty, analysis)
    obstacles = fill_max_gaps(obstacles, difficulty, analysis)
    obstacles = clean_shape_change_gap(obstacles)
    obstacles = clean_gap_one_early(obstacles, difficulty)
    obstacles = rebalance_easy_shapes(obstacles, difficulty)
    obstacles = rebalance_medium_shapes(obstacles, difficulty)
    obstacles = rebalance_hard_shapes(obstacles, difficulty)
    obstacles = clean_min_ioi(obstacles, difficulty, analysis)
    obstacles = clean_gap_monotony(obstacles, difficulty)
    obstacles = clean_shape_change_gap(obstacles)
    obstacles = clean_gap_one_early(obstacles, difficulty)
    obstacles = clean_gap_one_share(obstacles, difficulty)
    obstacles = clean_gap_monotony(obstacles, difficulty)
    obstacles = clean_shape_change_gap(obstacles)
    obstacles = clean_gap_one_early(obstacles, difficulty)
    return obstacles


def enforce_first_collision_floor(obstacles, difficulty, analysis):
    """Issue #175 — first obstacle must clear the per-difficulty reaction floor.

    Runtime uses analysis-derived beat timestamps as source of truth:
        first_collision = beats[first.beat]
    We operate on the analysis beat grid here, since beats[i] is the
    runtime arrival time of beat_index i.

    Strategy:
      1. Locate the smallest beat index whose grid time clears the floor.
      2. If the leading obstacle's beat is below that index, postpone it.
      3. If postponing would collide with the next obstacle (overlap or
         < MIN_GAP), drop the leading obstacle instead and re-evaluate.

    All subsequent obstacles are untouched, preserving the rhythm grid.
    """
    floor = MIN_FIRST_COLLISION_SEC.get(difficulty)
    if floor is None or not obstacles:
        return obstacles

    beats = analysis.get("beats") or []
    if not beats:
        return obstacles

    min_beat = next((i for i, t in enumerate(beats) if t >= floor), None)
    if min_beat is None:
        return obstacles

    MIN_GAP = {"easy": 2, "medium": 1, "hard": 1}
    min_gap = MIN_GAP.get(difficulty, 2)

    while obstacles and obstacles[0]["beat"] < min_beat:
        if len(obstacles) > 1 and obstacles[1]["beat"] - min_beat < min_gap:
            obstacles = obstacles[1:]
            continue
        obstacles[0]["beat"] = min_beat
        break

    return obstacles


def build_beatmap(analysis, difficulties, cleanup_enabled=True):
    """Build the full beatmap JSON."""
    beats = analysis["beats"]
    if not beats:
        raise ValueError("analysis.beats is required and must not be empty")

    def attach_and_validate_timing(obstacles, diff_name):
        timed = []
        for obs in obstacles:
            beat_idx = obs.get("beat")
            if not isinstance(beat_idx, int):
                raise ValueError(f"{diff_name}: obstacle beat index must be int, got {beat_idx!r}")
            if beat_idx < 0 or beat_idx >= len(beats):
                raise ValueError(
                    f"{diff_name}: beat index {beat_idx} out of range for beats[{len(beats)}]"
                )
            timed_obs = dict(obs)
            timed_obs["time_sec"] = round(float(beats[beat_idx]), 6)
            timed.append(timed_obs)

        # Internal consistency check: every authored obstacle must map back to
        # the same timestamp used by runtime.
        for obs in timed:
            beat_idx = obs["beat"]
            expected = float(beats[beat_idx])
            if abs(obs["time_sec"] - expected) > 1e-6:
                raise ValueError(
                    f"{diff_name}: time_sec mismatch at beat {beat_idx} "
                    f"(time_sec={obs['time_sec']}, expected={expected})"
                )
        return timed

    diff_data = {}
    for diff in difficulties:
        obs = design_level(analysis, diff, cleanup_enabled=cleanup_enabled)
        timed = attach_and_validate_timing(obs, diff)
        diff_data[diff] = {"beats": timed, "count": len(timed)}
    return {
        "song_id": analysis.get("title", "unknown"),
        "title": analysis.get("title", "unknown"),
        "bpm": analysis["bpm"],
        "offset": round(beats[0], 3) if beats else 0.0,
        "lead_beats": 4,
        "beat_times": [round(float(t), 6) for t in beats],
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
    parser.add_argument(
        "--diagnostics-out",
        default=None,
        help="If set, write subdivision snap diagnostics artifacts to this directory.",
    )
    parser.add_argument(
        "--diagnostics-only",
        action="store_true",
        help="Run diagnostics only (skip beatmap generation). Requires --diagnostics-out.",
    )
    parser.add_argument(
        "--no-cleanup",
        action="store_true",
        help="Disable cleanup and post-processing passes (raw generation only).",
    )
    args = parser.parse_args()

    if not Path(args.input).exists():
        print(f"Error: {args.input} not found", file=sys.stderr)
        sys.exit(1)

    with open(args.input) as f:
        analysis = json.load(f)

    if args.diagnostics_only and not args.diagnostics_out:
        print("Error: --diagnostics-only requires --diagnostics-out", file=sys.stderr)
        sys.exit(1)

    if args.diagnostics_out:
        out_dir = write_snap_diagnostics(analysis, args.diagnostics_out)
        print(f"✓ diagnostics: {out_dir}")
        if args.diagnostics_only:
            return

    diffs = ["easy","medium","hard"] if args.difficulty == "all" else [args.difficulty]

    print("=" * 60)
    print(f"  LEVEL DESIGNER | {analysis.get('title','?')} | BPM {analysis['bpm']}")
    print("=" * 60)

    beatmap = build_beatmap(analysis, diffs, cleanup_enabled=not args.no_cleanup)

    for diff in diffs:
        print_stats(beatmap["difficulties"][diff]["beats"], diff)

    out_path = args.output or f"{analysis.get('title','beatmap')}_beatmap.json"
    with open(out_path, "w") as f:
        json.dump(beatmap, f, indent=2)

    print(f"\n✓ {out_path}")
    print("=" * 60)


if __name__ == "__main__":
    main()
