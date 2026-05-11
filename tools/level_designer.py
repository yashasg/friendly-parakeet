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
    python level_designer.py analysis.json --experimental-onset-timing
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

SHAPE_TO_LANE = {"triangle": 0, "square": 1, "circle": 2}
LANE_TO_SHAPE = {0: "triangle", 1: "square", 2: "circle"}
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
ONSET_CLASS_TO_OBSTACLE = {
    "percussive": {"lane": 0, "shape": "triangle"},
    "harmonic": {"lane": 2, "shape": "circle"},
    "full-spectrum": {"lane": 1, "shape": "square"},
}
SHAPE_TO_ONSET_CLASS = {
    mapping["shape"]: onset_class
    for onset_class, mapping in ONSET_CLASS_TO_OBSTACLE.items()
}
LEGACY_PASS_TO_PUBLIC_LAYER = {
    "kick": "percussive",
    "snare": "percussive",
    "hihat": "percussive",
    "percussive_bass": "percussive",
    "percussive_broadband": "percussive",
    "percussive_high_mid": "percussive",
    "melody": "harmonic",
    "harmonic_low_mid": "harmonic",
    "flux": "full-spectrum",
    "full_spectrum_flux": "full-spectrum",
}
ONSET_MOTIF_DIFFICULTY_ROLES = {
    "easy": {"skeleton"},
    "medium": {"skeleton", "motif_core"},
    "hard": {"skeleton", "motif_core", "ornament", "fill"},
}
ONSET_MOTIF_DISABLED_LEGACY_RULES = [
    "legacy lane balancing",
    "legacy shape balancing",
    "fixed beat snapping selection",
    "random thinning",
    "legacy difficulty curves",
    "shape-gap enforcement",
    "legacy obstacle selection",
]
MOTIF_MIN_TOKEN_LEN = 3
MOTIF_MAX_TOKEN_LEN = 12
MOTIF_MIN_REPEAT_COUNT = 2

# Segment-focus selector: fraction of ranked focus-class events included per difficulty.
SEGMENT_FOCUS_DIFFICULTY_FRACTION = {"easy": 0.40, "medium": 0.65, "hard": 0.90}
# How many consecutive identical focus-class choices before anti-repetition penalty kicks in.
SEGMENT_FOCUS_ANTI_REPEAT_MAX = 2
PROTECTED_CROSS_LAYER_WINDOW_MS = 50.0
# Issue #528 — playability minimum on emitted shape_gate obstacles.  Mirrors
# rhythm-spec.md ``MORPH_DURATION``.  Two shape_gates with different
# (lane, shape) within this window are physically unsatisfiable; the
# selection-side ``_collapse_simultaneous_shape_gates`` pass keeps the
# higher-flux event (deterministic tie-break) and records the dropped
# onset under ``playability_collapsed_pairs`` so musical fidelity loss
# is visible.  No filler; no beat fallback.
PLAYABILITY_MORPH_WINDOW_SEC = 0.120
# Issue #532 — strict ceiling on dense same-shape clusters (mirrors the
# new gate in ``tools/validate_loop2_content_gates.py``).  Same-shape
# obstacles within ``SHAPE_CLUSTER_GAP_BEATS`` of each other form a
# cluster; cluster size > cap is thinned by dropping the lowest-flux
# events in the cluster (real onsets only — never reordered, never
# replaced with filler).  Easy is unconstrained (very sparse already).
MAX_SHAPE_CLUSTER_SIZE = {"medium": 4, "hard": 5}
SHAPE_CLUSTER_GAP_BEATS = 3
MIN_SUBDIVISION_LABEL_KINDS = {"medium": 2, "hard": 2}

PUBLIC_ONSET_CLASSES = {"percussive", "harmonic", "full-spectrum"}


def _is_protected_obstacle_pair(a, b):
    a_cls = a.get("source_onset_class") or a.get("onset_class")
    b_cls = b.get("source_onset_class") or b.get("onset_class")
    if a_cls not in PUBLIC_ONSET_CLASSES or b_cls not in PUBLIC_ONSET_CLASSES:
        return False
    if a_cls == b_cls:
        return False
    try:
        delta_ms = abs(float(a.get("time_sec", 0.0)) - float(b.get("time_sec", 0.0))) * 1000.0
    except (TypeError, ValueError):
        return False
    return delta_ms <= PROTECTED_CROSS_LAYER_WINDOW_MS


def _is_protected_by_neighbor(obstacle, obstacles):
    return any(other is not obstacle and _is_protected_obstacle_pair(obstacle, other)
               for other in obstacles)

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
MAX_EMPTY_GAP = {"easy": 40, "medium": 33, "hard": 32}
MAX_BEAT_DIFF = {diff: gap + 1 for diff, gap in MAX_EMPTY_GAP.items()}
MIN_SHAPE_CHANGE_GAP = 3
GAP_ONE_MEDIUM_START_PROGRESS = 0.30
GAP_ONE_HARD_MIN_BEAT = 11
GAP_ONE_MAX_RUN = {"medium": 1, "hard": 2}
# Issue #468 — these caps apply ONLY to the legacy beat-grid path
# (``design_level`` / ``clean_gap_one_share`` / ``clean_gap_monotony``).
# Under the active onset-only path (``design_level_segment_focus``,
# directive 2026-05-10) the per-obstacle ``beat`` field is the snapped
# musical-beat index with collision-bumping, so beat-ordinal gaps no
# longer encode the original "musical beats between hits" semantics.
# The validator demotes these gates to advisories in onset_timed mode
# (issue #443) — see ``tools/validate_loop2_content_gates.py``.  Keep
# the constants here so the legacy path and shared validator constants
# stay in lockstep.
GAP_ONE_SHARE_CAP = {"medium": 0.20, "hard": 0.20}
GAP_MONOTONY_CAP = {"medium": 0.40, "hard": 0.35}
MIN_IOI_MS = {"easy": 500.0, "medium": 350.0, "hard": 280.0}
# Issue #506 — bound mid-song silent stretches to the same per-difficulty
# beat caps that ``tools/validate_max_beat_gap.py`` enforces (issue #138).
# Values mirror ``validate_max_beat_gap.MAX_GAP`` exactly so the generator
# and validator cannot drift apart.  Caps are converted to seconds at the
# call site using the analysis BPM (``cap_sec = beats * 60 / bpm``) since
# the active onset-only path measures gaps in seconds.
MAX_SILENT_GAP_BEATS = {"easy": 40, "medium": 33, "hard": 32}
# Issues #392, #394 — enforce monotonic, song-independent difficulty pacing.
# Median IOI target ceilings (seconds). When the segment-focus selection
# leaves a difficulty above its ceiling, additional high-flux candidate
# events are promoted until the median IOI is at or below the target.
MEDIAN_IOI_TARGET_SEC = {"easy": 0.850, "medium": 0.680, "hard": 0.540}
# Issue #391 — cap consecutive same-lane runs by difficulty.  Lanes are
# rotated within the segment-focus path so timing/onset alignment is
# preserved while breaking same-lane walls.
MAX_SAME_LANE_RUN = {"easy": 4, "medium": 5, "hard": 6}
# Issue #396 — subdivision-aware snap tolerance (seconds).  Events whose
# distance from the nearest subdivision grid point (downbeat/eighth/triplet)
# is within this window are accepted with their proper subdivision label.
SUBDIVISION_SNAP_TOLERANCE_SEC = 0.060
DENSE_CLUSTER_SOFT_CAP = {"medium": 6, "hard": 10}
MEDIUM_SHAPE_TARGETS = {
    0: (25, 45),  # Triangle / lane 0
    1: (45, 60),  # Square / lane 1
    2: (10, 20),  # Circle / lane 2
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

def snap_events_to_beats(events, beats, tolerance=0.08, subdivision_tolerance=None):
    """Snap each event to nearest beat grid (with subdivision awareness).

    Each event is anchored to a ``beat_idx`` (the surrounding beat) and labeled
    with the closest subdivision in ``{downbeat, eighth, triplet}`` based on
    its phase within the beat span.

    Two acceptance gates:
        1. ``tolerance`` — legacy near-beat window (preserves existing snap
           semantics for downbeat events).
        2. ``subdivision_tolerance`` — Issue #396: events that fall within
           this window of a non-downbeat subdivision (eighth or triplet)
           grid point are also accepted, carrying the matching label.

    Deduplication rule (directive 2026-05-10, extended for #563):
        At most one event per ``(beat_idx, subdivision phase, onset_class)`` triple
        is kept.  This preserves cross-layer onsets at the same beat and also
        allows distinct subdivision grid phases (including 1/3 and 2/3
        triplets) at the same beat to survive.
    """
    if subdivision_tolerance is None:
        subdivision_tolerance = SUBDIVISION_SNAP_TOLERANCE_SEC
    snapped = []
    used: dict = {}  # (beat_idx, subdivision) -> set of onset_classes already placed

    for idx, ev in enumerate(events):
        event_time = float(ev["t"])
        # Locate surrounding beats.
        right = bisect_right(beats, event_time)
        left_idx = max(0, right - 1)
        right_idx = min(len(beats) - 1, right)

        # Decide anchor beat (nearest in time) for snap-distance check.
        nearest_idx = left_idx
        nearest_dist = abs(beats[left_idx] - event_time) if beats else float("inf")
        if right_idx != left_idx:
            d = abs(beats[right_idx] - event_time)
            if d < nearest_dist:
                nearest_idx = right_idx
                nearest_dist = d

        if not beats:
            continue

        # Subdivision classification anchored to the surrounding span.
        subdivision, _lane_hint = classify_subdivision(event_time, beats, nearest_idx)

        # Compute distance to the actual subdivision grid point.
        subdivision_phase = 0.0
        if right_idx == left_idx or right_idx >= len(beats):
            sub_dist = nearest_dist
            beat_anchor = nearest_idx
        else:
            span = max(1e-6, beats[right_idx] - beats[left_idx])
            phase = min(max((event_time - beats[left_idx]) / span, 0.0), 1.0)
            closest_frac, _ = min(
                SUBDIVISION_FRACTIONS, key=lambda item: abs(item[0] - phase)
            )
            grid_time = beats[left_idx] + span * closest_frac
            sub_dist = abs(event_time - grid_time)
            subdivision_phase = closest_frac
            # Anchor the event to the beat owning this subdivision's left edge,
            # except a phase-1.0 grid point belongs to the next beat.
            beat_anchor = left_idx if closest_frac < 0.999 else right_idx

        # Acceptance gate.
        if subdivision == "downbeat":
            if nearest_dist > tolerance:
                continue
            beat_anchor = nearest_idx
        else:
            if sub_dist > subdivision_tolerance:
                continue

        onset_class = classify_onset_class(ev)
        slot = (beat_anchor, subdivision, round(subdivision_phase, 3))
        if slot not in used:
            used[slot] = set()
        if onset_class in used[slot]:
            continue
        used[slot].add(onset_class)

        # Re-fetch lane hint for the resolved subdivision (kept for downstream
        # subdivision_lane consumers).
        _, lane_hint = classify_subdivision(event_time, beats, beat_anchor)
        snapped.append({
            **ev,
            "source_event_idx": idx,
            "beat_idx": beat_anchor,
            "beat_time": beats[beat_anchor],
            "subdivision": subdivision,
            "subdivision_phase": subdivision_phase,
            "subdivision_lane": lane_hint,
        })

    snapped.sort(key=lambda e: (e["beat_idx"], float(e.get("t", 0.0))))
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


def classify_onset_class(event):
    """Map onset layer/passes into one of the approved lane-class families.

    Checks the 'layer' field first (set by the pipeline after the 2026-05-10
    layer-aware merge fix).  Falls back to inferring from 'passes' for
    analysis files generated before that change.

    Pass-name fallback supports both:
      * the current non-instrumental subpass IDs
        (``percussive_*``, ``harmonic_*``, ``full_spectrum_*``), and
      * the legacy raw instrument names (``kick``/``snare``/``hihat``/``melody``/
        ``flux``) so previously shipped ``*_analysis.json`` files keep
        resolving correctly.
    """
    # New format: authoritative layer set by rhythm_pipeline.
    layer = event.get("layer")
    if layer in ("percussive", "harmonic", "full-spectrum"):
        return layer

    passes = set(event.get("passes", [])) if isinstance(event.get("passes"), list) else set()

    # Non-instrumental subpass IDs encode their layer as a prefix.
    has_percussive_prefix = any(p.startswith("percussive_") for p in passes)
    has_harmonic_prefix   = any(p.startswith("harmonic_")   for p in passes)
    has_full_prefix       = any(p.startswith("full_spectrum") for p in passes)

    # Legacy fallback: infer from named passes.
    legacy_percussive = bool({"kick", "snare", "hihat"} & passes)
    legacy_harmonic = "melody" in passes
    legacy_full = "flux" in passes

    percussive = has_percussive_prefix or legacy_percussive
    harmonic = has_harmonic_prefix or legacy_harmonic
    full_spectrum = has_full_prefix or legacy_full

    if (percussive and harmonic) or full_spectrum or not passes:
        return "full-spectrum"
    if percussive:
        return "percussive"
    if harmonic:
        return "harmonic"
    return "full-spectrum"


def _normalize_raw_per_pass_keys(raw_per_pass):
    normalized = {}
    if not isinstance(raw_per_pass, dict):
        return normalized
    for key, value in raw_per_pass.items():
        public_key = LEGACY_PASS_TO_PUBLIC_LAYER.get(key, key)
        normalized[public_key] = normalized.get(public_key, 0) + value
    return normalized


def event_grid_position(event_time, beats):
    """Project event time onto beat + subdivision coordinates."""
    if len(beats) < 2:
        return 0, float(event_time), 0.0, "downbeat"

    left_idx = bisect_right(beats, float(event_time)) - 1
    left_idx = min(max(left_idx, 0), len(beats) - 2)
    left = float(beats[left_idx])
    right = float(beats[left_idx + 1])
    span = max(1e-6, right - left)
    phase = min(max((float(event_time) - left) / span, 0.0), 1.0)
    subdivision, _ = classify_subdivision(float(event_time), beats, left_idx)
    subdivision_fraction = {
        "downbeat": 0.0 if phase < 0.5 else 1.0,
        "triplet": 1.0 / 3.0 if phase < 0.5 else 2.0 / 3.0,
        "eighth": 0.5,
        "offgrid": round(phase * 6.0) / 6.0,
    }.get(subdivision, 0.0)
    return left_idx, left_idx + subdivision_fraction, subdivision_fraction, subdivision


def tokenize_onset_events(events, beats, intro_rest):
    """Create motif tokens over beat/subdivision coordinates."""
    token_events = []
    for event_idx, event in enumerate(events):
        event_time = float(event.get("t", 0.0))
        beat_idx, beat_pos, subdivision_fraction, subdivision = event_grid_position(event_time, beats)
        if beat_idx < intro_rest:
            continue
        onset_class = classify_onset_class(event)
        token_events.append({
            "event_idx": event_idx,
            "t": round(event_time, 6),
            "beat_idx": int(min(max(round(beat_idx), 0), len(beats) - 1)),
            "beat_pos": float(beat_pos),
            "subdivision_fraction": float(subdivision_fraction),
            "subdivision": subdivision,
            "onset_class": onset_class,
            "token": f"{onset_class}:{subdivision}",
            "flux": float(event.get("flux", 0.0)),
            "passes": list(event.get("passes", [])) if isinstance(event.get("passes"), list) else [],
        })
    token_events.sort(key=lambda row: (row["t"], row["event_idx"]))
    return token_events


def detect_variable_length_onset_motifs(token_events):
    """Find repeated onset-token motifs at natural repeated lengths."""
    motifs = []
    n = len(token_events)
    if n < MOTIF_MIN_TOKEN_LEN:
        return motifs

    fingerprints = {}
    max_len = min(MOTIF_MAX_TOKEN_LEN, n)
    for length in range(MOTIF_MIN_TOKEN_LEN, max_len + 1):
        for start in range(0, n - length + 1):
            window = token_events[start:start + length]
            token_seq = tuple(row["token"] for row in window)
            rel = tuple(round(window[i + 1]["beat_pos"] - window[i]["beat_pos"], 3) for i in range(length - 1))
            fingerprint = (length, token_seq, rel)
            fingerprints.setdefault(fingerprint, []).append(start)

    ranked = []
    for fingerprint, starts in fingerprints.items():
        if len(starts) < MOTIF_MIN_REPEAT_COUNT:
            continue
        length = fingerprint[0]
        non_overlapping = []
        for start in sorted(starts):
            if non_overlapping and start < (non_overlapping[-1] + length):
                continue
            non_overlapping.append(start)
        if len(non_overlapping) < MOTIF_MIN_REPEAT_COUNT:
            continue
        ranked.append((len(non_overlapping) * length, len(non_overlapping), length, fingerprint, non_overlapping))

    ranked.sort(reverse=True)
    used_windows = set()
    motif_counter = 1
    for _, repeat_count, length, fingerprint, starts in ranked:
        fresh_starts = []
        for start in starts:
            key = (start, length)
            if key in used_windows:
                continue
            fresh_starts.append(start)
        if len(fresh_starts) < MOTIF_MIN_REPEAT_COUNT:
            continue

        token_seq = fingerprint[1]
        lengths = []
        for start in fresh_starts:
            end = start + length - 1
            span_beats = token_events[end]["beat_pos"] - token_events[start]["beat_pos"]
            lengths.append(max(0.0, span_beats))
            used_windows.add((start, length))

        motif_id = f"M{motif_counter:03d}"
        motif_counter += 1
        motifs.append({
            "motif_id": motif_id,
            "token_length": int(length),
            "repeat_count": int(len(fresh_starts)),
            "fingerprint": "|".join(token_seq),
            "length_beats": round(sum(lengths) / len(lengths), 3) if lengths else 0.0,
            "starts": fresh_starts,
        })
    return motifs


def annotate_motif_roles(token_events, motifs):
    """Attach motif role metadata to tokenized events."""
    for event in token_events:
        event["motif_id"] = None
        event["motif_repeat_count"] = 0
        event["motif_token_length"] = 0
        event["motif_length_beats"] = 0.0
        event["motif_fingerprint"] = ""
        event["motif_event_role"] = "ornament"
        event["motif_support_score"] = 0.0

    for motif in motifs:
        token_length = motif["token_length"]
        starts = motif["starts"]
        if not starts:
            continue

        idx_flux = [0.0] * token_length
        idx_count = [0] * token_length
        for start in starts:
            for local_idx in range(token_length):
                ev = token_events[start + local_idx]
                idx_flux[local_idx] += ev.get("flux", 0.0)
                idx_count[local_idx] += 1
        avg_flux = [
            (idx_flux[i] / idx_count[i]) if idx_count[i] > 0 else 0.0
            for i in range(token_length)
        ]
        skeleton_count = max(1, round(token_length * 0.4))
        ranked_indices = sorted(
            range(token_length),
            key=lambda i: (avg_flux[i], -i),
            reverse=True,
        )
        skeleton_indices = set(ranked_indices[:skeleton_count])

        motif_strength = motif["repeat_count"] * token_length
        for start in starts:
            for local_idx in range(token_length):
                event = token_events[start + local_idx]
                role = "skeleton" if local_idx in skeleton_indices else "motif_core"
                score = motif_strength + event.get("flux", 0.0)
                if score >= event["motif_support_score"]:
                    event["motif_id"] = motif["motif_id"]
                    event["motif_repeat_count"] = motif["repeat_count"]
                    event["motif_token_length"] = token_length
                    event["motif_length_beats"] = motif["length_beats"]
                    event["motif_fingerprint"] = motif["fingerprint"]
                    event["motif_event_role"] = role
                    event["motif_support_score"] = score

    motif_positions = sorted(
        event["beat_pos"]
        for event in token_events
        if event["motif_id"]
    )
    if motif_positions:
        flux_values = sorted(event.get("flux", 0.0) for event in token_events)
        flux_floor = _percentile(flux_values, 0.5)
        for event in token_events:
            if event["motif_id"]:
                continue
            nearest = min(abs(event["beat_pos"] - pos) for pos in motif_positions)
            if nearest <= 0.75 and event.get("flux", 0.0) >= flux_floor:
                event["motif_event_role"] = "ornament"
            else:
                event["motif_event_role"] = "fill"


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


def _ioi_stats_from_times(times):
    if len(times) < 2:
        return {
            "count": 0,
            "min_ms": 0.0,
            "median_ms": 0.0,
            "p90_ms": 0.0,
            "mean_ms": 0.0,
            "max_ms": 0.0,
        }
    iois = sorted((times[i] - times[i - 1]) * 1000.0 for i in range(1, len(times)))
    return {
        "count": len(iois),
        "min_ms": round(iois[0], 3),
        "median_ms": round(_percentile(iois, 0.5), 3),
        "p90_ms": round(_percentile(iois, 0.9), 3),
        "mean_ms": round(sum(iois) / len(iois), 3),
        "max_ms": round(iois[-1], 3),
    }


def _dense_cluster_stats_from_times(times, threshold_ms):
    if len(times) < 2:
        return {
            "threshold_ms": round(threshold_ms, 3),
            "short_ioi_count": 0,
            "short_ioi_share": 0.0,
            "cluster_count": 0,
            "max_cluster_len": 0,
        }

    iois = [(times[i] - times[i - 1]) * 1000.0 for i in range(1, len(times))]
    short_flags = [ioi < threshold_ms for ioi in iois]
    short_count = sum(1 for flag in short_flags if flag)
    total = len(iois)
    cluster_count = 0
    max_cluster_len = 0
    current = 0
    for flag in short_flags:
        if flag:
            current += 1
            max_cluster_len = max(max_cluster_len, current + 1)
        else:
            if current > 0:
                cluster_count += 1
                current = 0
    if current > 0:
        cluster_count += 1

    return {
        "threshold_ms": round(threshold_ms, 3),
        "short_ioi_count": short_count,
        "short_ioi_share": round((short_count / total) if total else 0.0, 4),
        "cluster_count": cluster_count,
        "max_cluster_len": max_cluster_len,
    }


def _residual_delta_summary(residuals_ms):
    if not residuals_ms:
        return {
            "count": 0,
            "median_abs_ms": 0.0,
            "p90_abs_ms": 0.0,
            "mean_abs_ms": 0.0,
            "max_abs_ms": 0.0,
        }
    abs_residuals = sorted(abs(v) for v in residuals_ms)
    return {
        "count": len(abs_residuals),
        "median_abs_ms": round(_percentile(abs_residuals, 0.5), 3),
        "p90_abs_ms": round(_percentile(abs_residuals, 0.9), 3),
        "mean_abs_ms": round(sum(abs_residuals) / len(abs_residuals), 3),
        "max_abs_ms": round(abs_residuals[-1], 3),
    }


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


def _final_same_shape_metrics(beatmap):
    return {
        diff: _same_shape_run_metrics(payload.get("beats", []))
        for diff, payload in beatmap.get("difficulties", {}).items()
        if isinstance(payload, dict)
    }


def _load_shipped_beatmap_for_analysis(analysis):
    song_id = analysis.get("title") or analysis.get("song_id")
    if not song_id:
        return None
    beatmap_path = Path(__file__).resolve().parent.parent / "content" / "beatmaps" / f"{song_id}_beatmap.json"
    if not beatmap_path.exists():
        return None
    with beatmap_path.open(encoding="utf-8") as handle:
        return json.load(handle)


def _nearest_subdivision_grid_time(event_time, beats, beat_idx):
    if not beats:
        return float(event_time)
    candidates: list[float] = []
    fractions = (0.0, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0)
    left_min = max(0, int(beat_idx) - 4)
    left_max = min(len(beats) - 1, int(beat_idx) + 4)
    for left_idx in range(left_min, left_max + 1):
        left = float(beats[left_idx])
        right = float(beats[left_idx + 1]) if left_idx + 1 < len(beats) else left
        span = right - left
        for fraction in fractions:
            candidates.append(left + span * fraction)
    if not candidates:
        return float(beats[min(max(int(beat_idx), 0), len(beats) - 1)])
    event_time = float(event_time)
    return min(candidates, key=lambda grid_time: abs(event_time - grid_time))


def _build_obstacle_timing_rows(analysis, difficulty, experimental_onset_timing=False):
    beats = analysis.get("beats", [])
    selection_summary = {"segments_total": 0, "events_total": 0, "events_selected": 0}
    if experimental_onset_timing:
        # Active onset-only path: segment-focus selection, cleanup disabled.
        obstacles, selected, seg_diagnostics = design_level_segment_focus(
            analysis,
            difficulty,
            cleanup_enabled=False,  # cleanup is always off for this path
        )
        selection_summary = {
            "generation_path": seg_diagnostics.get("generation_path", "segment_focus"),
            "segments_total": int(seg_diagnostics.get("segments_total", 0)),
            "events_total": int(seg_diagnostics.get("events_total", 0)),
            "events_selected": int(seg_diagnostics.get("events_selected", 0)),
            "segment_details": seg_diagnostics.get("segments", []),
        }
    else:
        selected, _ = select_beats(analysis, difficulty)
        obstacles = design_level(analysis, difficulty, cleanup_enabled=True)
    selected_by_source: dict[int, dict] = {}
    selected_by_beat: dict[int, list[dict]] = {}
    for event in selected.values():
        if not isinstance(event, dict):
            continue
        source_event_idx = event.get("source_event_idx")
        beat_idx = event.get("beat_idx")
        if isinstance(source_event_idx, int):
            selected_by_source[source_event_idx] = event
        if isinstance(beat_idx, int):
            selected_by_beat.setdefault(beat_idx, []).append(event)
    rows = []
    for order, obs in enumerate(sorted(obstacles, key=lambda item: item["beat"])):
        beat_idx = obs["beat"]
        if beat_idx < 0 or beat_idx >= len(beats):
            continue

        beat_time = float(beats[beat_idx])
        source_event_idx = obs.get("source_event_idx")
        event = selected_by_source.get(source_event_idx) if isinstance(source_event_idx, int) else None
        if event is None:
            beat_events = selected_by_beat.get(beat_idx, [])
            if len(beat_events) == 1:
                event = beat_events[0]
        if isinstance(event, dict):
            onset_time = float(event.get("t", beat_time))
            source = "onset"
            subdivision = event.get("subdivision", "downbeat")
            source_event_idx = event.get("source_event_idx")
        else:
            raise ValueError(
                f"{difficulty}: segment-focus diagnostics require source onset event for beat {beat_idx}"
            )

        grid_time = _nearest_subdivision_grid_time(onset_time, beats, beat_idx)

        rows.append({
            "difficulty": difficulty,
            "event_order": order,
            "beat_idx": beat_idx,
            "beat_time": round(beat_time, 6),
            "onset_time": round(onset_time, 6),
            "residual_ms": round((onset_time - grid_time) * 1000.0, 3),
            "timing_source": source,
            "subdivision": subdivision,
            "source_event_idx": source_event_idx,
            "onset_class": event.get("onset_class") if isinstance(event, dict) else obs.get("onset_class"),
            "segment_idx": (event.get("segment_idx") if isinstance(event, dict) else None) or obs.get("segment_idx"),
            "segment_focus": (event.get("segment_focus") if isinstance(event, dict) else None) or obs.get("segment_focus"),
            # Keep motif_* fields as None for CSV schema stability.
            "motif_id": None,
            "motif_length_beats": None,
            "motif_token_length": None,
            "motif_repeat_count": None,
            "motif_fingerprint": None,
            "event_role": obs.get("difficulty_inclusion") or (event.get("difficulty_inclusion") if isinstance(event, dict) else None),
            "difficulty_inclusion": obs.get("difficulty_inclusion") or (event.get("difficulty_inclusion") if isinstance(event, dict) else None),
        })
    return rows, selection_summary


def _build_final_obstacle_timing_rows(analysis, difficulty, beatmap):
    beats = analysis.get("beats", [])
    events_by_source = {
        index: event
        for index, event in enumerate(analysis.get("events", []))
        if isinstance(event, dict)
    }
    diff_payload = beatmap.get("difficulties", {}).get(difficulty, {})
    obstacles = diff_payload.get("beats", []) if isinstance(diff_payload, dict) else []
    rows = []
    for order, obs in enumerate(obstacles):
        if not isinstance(obs, dict):
            continue
        beat_idx = obs.get("beat")
        if not isinstance(beat_idx, int) or beat_idx < 0 or beat_idx >= len(beats):
            continue

        source_event_idx = obs.get("source_event_idx")
        event = events_by_source.get(source_event_idx) if isinstance(source_event_idx, int) else None
        beat_time = float(obs.get("beat_time_sec", beats[beat_idx]))
        onset_time = float(obs.get("onset_time_sec", obs.get("time_sec", beat_time)))
        grid_time = _nearest_subdivision_grid_time(onset_time, beats, beat_idx)
        subdivision = obs.get("subdivision_label") or obs.get("subdivision")
        if not subdivision and isinstance(event, dict):
            subdivision = event.get("subdivision")
        onset_class = obs.get("onset_class") or obs.get("source_onset_class")
        if not onset_class and isinstance(event, dict):
            onset_class = event.get("onset_class") or event.get("layer")

        rows.append({
            "difficulty": difficulty,
            "event_order": order,
            "beat_idx": beat_idx,
            "beat_time": round(beat_time, 6),
            "onset_time": round(onset_time, 6),
            "residual_ms": round((onset_time - grid_time) * 1000.0, 3),
            "timing_source": obs.get("timing_source", "onset"),
            "subdivision": subdivision or "downbeat",
            "source_event_idx": source_event_idx,
            "onset_class": onset_class or "unknown",
            "segment_idx": obs.get("segment_idx") if obs.get("segment_idx") is not None else (
                event.get("segment_idx") if isinstance(event, dict) else None
            ),
            "segment_focus": obs.get("segment_focus") if obs.get("segment_focus") is not None else (
                event.get("segment_focus") if isinstance(event, dict) else None
            ),
            "motif_id": None,
            "motif_length_beats": None,
            "motif_token_length": None,
            "motif_repeat_count": None,
            "motif_fingerprint": None,
            "event_role": obs.get("difficulty_inclusion"),
            "difficulty_inclusion": obs.get("difficulty_inclusion"),
        })
    selection_summary = {
        "generation_path": "final_shipped_beatmap",
        "segments_total": len(analysis.get("structure", [])),
        "events_total": len(analysis.get("events", [])),
        "events_selected": len(rows),
        "final_obstacle_count": len(rows),
        "segment_details": [],
    }
    return rows, selection_summary


def _onset_timing_comparison_summary(rows, difficulty):
    beat_times = [float(row["beat_time"]) for row in rows]
    onset_times = [float(row["onset_time"]) for row in rows]
    residuals = [float(row["residual_ms"]) for row in rows]
    subdivision_hist = dict(sorted(Counter(row["subdivision"] for row in rows).items()))
    source_hist = dict(sorted(Counter(row["timing_source"] for row in rows).items()))
    onset_class_hist = dict(sorted(Counter((row.get("onset_class") or "unknown") for row in rows).items()))
    role_hist = dict(sorted(Counter((row.get("event_role") or "unknown") for row in rows).items()))
    motif_rows = [row for row in rows if row.get("motif_id")]
    motif_ids = sorted({row.get("motif_id") for row in motif_rows if row.get("motif_id")})
    motif_repeat_counts = [int(row.get("motif_repeat_count", 0) or 0) for row in motif_rows]
    motif_lengths = [float(row.get("motif_length_beats", 0.0) or 0.0) for row in motif_rows]
    threshold_ms = MIN_IOI_MS.get(difficulty, 350.0)
    return {
        "event_counts": {
            "obstacles": len(rows),
            "timing_source_histogram": source_hist,
        },
        "residuals_onset_minus_beat_ms": _residual_delta_summary(residuals),
        "ioi_stats_ms": {
            "beat_snap": _ioi_stats_from_times(beat_times),
            "onset_timing": _ioi_stats_from_times(onset_times),
        },
        "dense_cluster_stats": {
            "beat_snap": _dense_cluster_stats_from_times(beat_times, threshold_ms),
            "onset_timing": _dense_cluster_stats_from_times(onset_times, threshold_ms),
        },
        "subdivision_label_distribution": subdivision_hist,
        "onset_class_distribution": onset_class_hist,
        "event_role_distribution": role_hist,
        "motif_stats": {
            "motif_ids": motif_ids,
            "motif_count": len(motif_ids),
            "rows_with_motif": len(motif_rows),
            "max_repeat_count": max(motif_repeat_counts) if motif_repeat_counts else 0,
            "max_length_beats": round(max(motif_lengths), 3) if motif_lengths else 0.0,
        },
    }


def write_snap_diagnostics(
    analysis,
    diagnostics_out_dir,
    experimental_onset_timing=False,
    prefer_shipped_beatmap=True,
):
    """Emit subdivision-aware diagnostics artifacts without changing generation."""
    out_dir = Path(diagnostics_out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for artifact_name in (
        "snap_candidate_events.csv",
        "onset_timing_events.csv",
        "snap_diagnostics_summary.json",
    ):
        artifact_path = out_dir / artifact_name
        if artifact_path.exists():
            artifact_path.unlink()

    events = analysis.get("events", [])
    beats = analysis.get("beats", [])
    if not events or not beats:
        raise ValueError("analysis must include non-empty events and beats for diagnostics")

    current_snap = snap_events_to_beats(events, beats)
    current_rows = []
    # Issue #471 — residuals must be measured against the actual
    # subdivision grid point the event was labelled with, not the bare
    # beat anchor.  ``snap_events_to_beats`` records ``beat_time`` =
    # beats[beat_anchor] (the surrounding beat), so for events tagged
    # ``eighth``/``triplet`` the previous formula
    # ``residual = event_time - beat_time`` actually reported the
    # event's distance from the *downbeat*, not from the eighth/triplet
    # grid line — making p90 and within_Xms numbers structurally
    # over-count residuals for off-beat subdivisions.  Compute the true
    # subdivision grid time here so the diagnostic semantics match the
    # field name (``snap_residual_stats``).
    _SUB_FRACTION = {
        "downbeat": 0.0,
        "eighth": 0.5,
        "triplet": 1.0 / 3.0,  # closest-of-{1/3, 2/3}; refined below
    }
    for i, row in enumerate(current_snap):
        event_time = float(row.get("t", 0.0))
        beat_idx = int(row.get("beat_idx", 0))
        subdivision = row.get("subdivision", "downbeat")

        # Resolve the surrounding beat span so triplet phase can pick
        # between 1/3 and 2/3.
        left = float(beats[beat_idx]) if 0 <= beat_idx < len(beats) else 0.0
        right_idx = beat_idx + 1 if beat_idx + 1 < len(beats) else beat_idx
        right = float(beats[right_idx]) if right_idx < len(beats) else left
        span = max(1e-6, right - left)

        if subdivision == "triplet":
            phase = (event_time - left) / span
            frac = (1.0 / 3.0) if abs(phase - 1.0 / 3.0) <= abs(phase - 2.0 / 3.0) else (2.0 / 3.0)
        else:
            frac = _SUB_FRACTION.get(subdivision, 0.0)
        grid_time = left + span * frac

        residual_ms = (event_time - grid_time) * 1000.0
        current_rows.append({
            "candidate": "current_quarter_snap",
            "event_idx": int(row.get("source_event_idx", i)),
            "event_time": round(event_time, 6),
            "beat_idx": beat_idx,
            "grid_time": round(grid_time, 6),
            "subdivision": subdivision,
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

    # ── Onset pool summary (always emitted, not gated on experimental) ────────
    duration_sec = float(analysis.get("duration", 1.0))
    events_per_min = round(len(events) / max(1.0, duration_sec) * 60.0, 1)
    structure = analysis.get("structure", [])
    seg_event_counts: list[dict] = []
    empty_seg_count = 0
    for sec in structure:
        t0 = float(sec.get("start", 0.0))
        t1 = float(sec.get("end", duration_sec))
        n_in = sum(1 for e in events if t0 <= float(e.get("t", 0.0)) < t1)
        empty = n_in == 0
        if empty:
            empty_seg_count += 1
        seg_event_counts.append({
            "section": sec.get("section", "?"),
            "start": t0,
            "end": t1,
            "event_count": n_in,
            "empty": empty,
        })

    snap_candidate_count = len(current_rows)
    snap_residual_stats = _residual_summary(current_rows)

    onset_pool_summary = {
        "total_events": len(events),
        "events_per_minute": events_per_min,
        "snapped_events": snap_candidate_count,
        "snap_residual_stats": snap_residual_stats,
        "segment_count": len(structure),
        "empty_segment_count": empty_seg_count,
        "segment_coverage": seg_event_counts,
        # from analysis.onset_diagnostics if present
        "raw_per_pass": _normalize_raw_per_pass_keys(
            analysis.get("onset_diagnostics", {}).get("raw_per_pass", {})
        ),
        "raw_total": analysis.get("onset_diagnostics", {}).get("raw_total", None),
    }

    final_beatmap = None
    final_beatmap_source = None
    if experimental_onset_timing:
        final_beatmap = _load_shipped_beatmap_for_analysis(analysis) if prefer_shipped_beatmap else None
        if final_beatmap is not None:
            final_beatmap_source = "shipped_beatmap"
        if final_beatmap is None:
            final_beatmap = build_beatmap(
                analysis,
                ["easy", "medium", "hard"],
                cleanup_enabled=True,
                experimental_onset_timing=True,
            )
            final_beatmap_source = "generated_final_beatmap"

    summary = {
        "song_id": analysis.get("title", "unknown"),
        "onset_pool_summary": onset_pool_summary,
        "residual_summary": residual_summary,
        "subdivision_histogram": subdivision_histogram,
        "gap_histogram_50ms_bins": gap_histogram,
        "same_shape_run_metrics": (
            _final_same_shape_metrics(final_beatmap)
            if final_beatmap is not None
            else _baseline_same_shape_metrics(analysis)
        ),
    }

    if experimental_onset_timing:
        onset_rows = []
        onset_summary = {}
        selection_rollup = {}
        obstacle_counts: dict[str, int] = {}
        for difficulty in ("easy", "medium", "hard"):
            rows, selection_summary = _build_final_obstacle_timing_rows(
                analysis, difficulty, final_beatmap
            )
            onset_rows.extend(rows)
            onset_summary[difficulty] = _onset_timing_comparison_summary(rows, difficulty)
            selection_rollup[difficulty] = selection_summary
            obstacle_counts[difficulty] = len(rows)

        onset_fields = [
            "difficulty",
            "event_order",
            "beat_idx",
            "beat_time",
            "onset_time",
            "residual_ms",
            "timing_source",
            "subdivision",
            "source_event_idx",
            "onset_class",
            "segment_idx",
            "segment_focus",
            # Kept for CSV schema stability (null values).
            "motif_id",
            "motif_length_beats",
            "motif_token_length",
            "motif_repeat_count",
            "motif_fingerprint",
            "event_role",
            "difficulty_inclusion",
        ]
        with (out_dir / "onset_timing_events.csv").open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=onset_fields, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(onset_rows)

        summary["experimental_onset_timing"] = {
            "enabled": True,
            "generation_path": "final_shipped_beatmap"
            if final_beatmap_source == "shipped_beatmap"
            else "final_generated_beatmap",
            "diagnostics_source": final_beatmap_source,
            "legacy_rule_influence_disabled": True,
            "disabled_legacy_rules": list(ONSET_MOTIF_DISABLED_LEGACY_RULES),
            "comparison_by_difficulty": onset_summary,
            "selection_summary_by_difficulty": selection_rollup,
            "obstacle_counts_by_difficulty": obstacle_counts,
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
    source_class = obs.get("source_onset_class", obs.get("onset_class"))
    if source_class in ONSET_CLASS_TO_OBSTACLE:
        shape = ONSET_CLASS_TO_OBSTACLE[source_class]["shape"]
    if shape not in SHAPE_TO_LANE:
        shape = "square"
    previous_class = obs.get("onset_class")
    if previous_class in ONSET_CLASS_TO_OBSTACLE and "source_onset_class" not in obs:
        obs["source_onset_class"] = previous_class
    obs["kind"] = "shape_gate"
    obs["shape"] = shape
    obs["lane"] = SHAPE_TO_LANE[shape]
    obs["onset_class"] = SHAPE_TO_ONSET_CLASS[shape]


def nearest_shape_lane(obstacles, index):
    """Find the nearest authored shape/lane around index."""
    for j in range(index - 1, -1, -1):
        if is_shape_gate(obstacles[j]):
            shape = obstacles[j].get("shape", "square")
            if shape in SHAPE_TO_LANE:
                lane = clamp_lane(obstacles[j].get("lane", SHAPE_TO_LANE[shape]))
                return shape, lane
    for j in range(index + 1, len(obstacles)):
        if is_shape_gate(obstacles[j]):
            shape = obstacles[j].get("shape", "square")
            if shape in SHAPE_TO_LANE:
                lane = clamp_lane(obstacles[j].get("lane", SHAPE_TO_LANE[shape]))
                return shape, lane
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
    """Diagnostics-only legacy beat-grid selector.

    Shipping beatmap generation uses segment-focus onset selection instead.
    This legacy selector is retained for diagnostics that compare historical
    beat-grid behavior against the onset-only authoring path.
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


def _event_section_name(event_time, structure):
    for section in structure:
        if section["start"] <= event_time < section["end"]:
            return section.get("section", "verse")
    return "verse"


def _motif_priority_score(event, difficulty):
    flux_score = event.get("flux", 0.0) * 100.0
    repeat_score = event.get("motif_repeat_count", 0) * 12.0
    class_bias = {"full-spectrum": 18.0, "percussive": 12.0, "harmonic": 10.0}.get(
        event.get("onset_class"),
        0.0,
    )
    role_weights = {
        "easy": {"skeleton": 400.0, "motif_core": 170.0, "ornament": 60.0, "fill": 20.0},
        "medium": {"skeleton": 320.0, "motif_core": 250.0, "ornament": 120.0, "fill": 40.0},
        "hard": {"skeleton": 280.0, "motif_core": 260.0, "ornament": 200.0, "fill": 120.0},
    }
    role_score = role_weights[difficulty].get(event.get("motif_event_role", "fill"), 0.0)
    return role_score + flux_score + repeat_score + class_bias


def select_onset_motif_beats(analysis, difficulty):
    """Experimental selector: pick events from repeated onset motifs first."""
    beats = analysis["beats"]
    events = analysis["events"]
    intro_rest = DIFFICULTY_INTRO_REST[difficulty]

    token_events = tokenize_onset_events(events, beats, intro_rest)
    motifs = detect_variable_length_onset_motifs(token_events)
    annotate_motif_roles(token_events, motifs)
    event_map = {event["event_idx"]: event for event in token_events}
    allowed_roles = ONSET_MOTIF_DIFFICULTY_ROLES[difficulty]
    selected_events = {}
    diagnostics = {
        "motifs": motifs,
        "events_total": len(token_events),
        "events_with_motif": sum(1 for event in token_events if event.get("motif_id")),
        "difficulty_allowed_roles": sorted(allowed_roles),
        "legacy_rule_influence_disabled": True,
        "disabled_legacy_rules": list(ONSET_MOTIF_DISABLED_LEGACY_RULES),
    }

    primary_candidates = [
        event
        for event in token_events
        if event.get("motif_id") and event.get("motif_event_role", "fill") in allowed_roles
    ]
    fallback_candidates = [
        event for event in token_events
        if event.get("motif_event_role", "fill") in allowed_roles
    ]
    chosen_events = primary_candidates or fallback_candidates

    for event in chosen_events:
        beat_idx = event["beat_idx"]
        previous = selected_events.get(beat_idx)
        if previous is None:
            selected_events[beat_idx] = event
            continue
        previous_key = (
            _motif_priority_score(previous, difficulty),
            previous.get("flux", 0.0),
            -previous.get("event_idx", 0),
        )
        candidate_key = (
            _motif_priority_score(event, difficulty),
            event.get("flux", 0.0),
            -event.get("event_idx", 0),
        )
        if candidate_key > previous_key:
            selected_events[beat_idx] = event

    role_hist = Counter(event.get("motif_event_role", "fill") for event in selected_events.values())
    diagnostics["selected_events"] = len(selected_events)
    diagnostics["selected_role_distribution"] = dict(sorted(role_hist.items()))

    return selected_events, event_map, diagnostics


# ═══════════════════════════════════════════════════════════════
# SEGMENT-FOCUS ONSET SELECTION
# Replaces motif n-gram selection as the active experimental path.
# ═══════════════════════════════════════════════════════════════

def _segment_class_stats(events):
    """Compute per-onset-class event count and total flux for a list of snapped events."""
    stats = {}
    for ev in events:
        cls = classify_onset_class(ev)
        if cls not in stats:
            stats[cls] = {"count": 0, "total_flux": 0.0, "events": []}
        stats[cls]["count"] += 1
        stats[cls]["total_flux"] += float(ev.get("flux", 0.0))
        stats[cls]["events"].append(ev)
    return stats


def _choose_segment_focus(class_stats, prev_focuses):
    """Pick the dominant focus class with anti-repetition bias.

    Returns (focus_class, fallback_reason_or_None).
    focus_class is one of "percussive", "harmonic", "full-spectrum".
    """
    if not class_stats:
        return "full-spectrum", "no_events_in_segment"

    # Score: total_flux primary, count tiebreaker.
    scores = {
        cls: stats["total_flux"] + stats["count"] * 0.1
        for cls, stats in class_stats.items()
    }

    # Issue #420 — force rotation across all available broad onset
    # layers so harmonic (lane 2 / circle) and other underrepresented
    # layers actually get picked when they have non-empty events in a
    # segment.  Forbid up to ``SEGMENT_FOCUS_ANTI_REPEAT_MAX`` of the
    # most recently used classes from the eligible pool whenever doing
    # so still leaves at least one candidate; this drives a strict
    # round-robin across the broad layers rather than the previous
    # halving heuristic, which a high-flux class (e.g. percussive)
    # could still dominate after the halving.
    fallback_reason = None
    if scores:
        forbid_window = min(max(len(scores) - 1, 0), SEGMENT_FOCUS_ANTI_REPEAT_MAX)
        forbidden = set(prev_focuses[-forbid_window:]) if forbid_window > 0 else set()
        eligible = {c: s for c, s in scores.items() if c not in forbidden}
        if eligible and forbidden:
            scores = eligible
            fallback_reason = "rotate_skip_" + ",".join(sorted(forbidden))

    focus = max(scores, key=lambda c: (scores[c], c))  # c as stable tiebreak
    return focus, fallback_reason


def _is_protected_cross_layer_pair(left, right):
    """Issue #507 — purely temporal cross-layer protection.

    The 2026-05-10 directive (encoded in ``PROTECTED_CROSS_LAYER_WINDOW_MS``)
    keeps any two broad-layer onsets distinct when their audio timestamps
    are within 50 ms.  Earlier revisions added an extra ``beat_idx``
    equality check, which silently flipped protection off whenever the
    snapper landed two simultaneous onsets on opposite sides of a beat
    boundary; ``_clears_min_ioi`` then dropped the second event under the
    per-difficulty IOI floor.  That coupling is removed: protection is
    determined purely by broad layer (``percussive`` / ``harmonic`` /
    ``full-spectrum``) and ``|Δt| <= PROTECTED_CROSS_LAYER_WINDOW_MS``.

    Same-layer IOI enforcement (``MIN_IOI_MS``) is unchanged.
    """
    left_class = left.get("onset_class") or classify_onset_class(left)
    right_class = right.get("onset_class") or classify_onset_class(right)
    if left_class == right_class:
        return False
    dt_ms = abs(float(left.get("t", 0.0)) - float(right.get("t", 0.0))) * 1000.0
    return dt_ms <= PROTECTED_CROSS_LAYER_WINDOW_MS


def _clears_min_ioi(candidate, selected_events, difficulty):
    floor_ms = MIN_IOI_MS.get(difficulty)
    if floor_ms is None:
        return True
    candidate_time = float(candidate.get("t", 0.0))
    for event in selected_events:
        dt_ms = abs(candidate_time - float(event.get("t", 0.0))) * 1000.0
        if dt_ms < floor_ms and not _is_protected_cross_layer_pair(candidate, event):
            return False
    return True


def _event_key(event):
    onset_class = event.get("onset_class") or classify_onset_class(event)
    subdivision = event.get("subdivision", "downbeat")
    return (
        int(event["beat_idx"]),
        subdivision,
        onset_class,
        int(event.get("source_event_idx", -1)),
    )


def _ranked_events_for_ioi(events):
    return sorted(
        events,
        key=lambda ev: (-float(ev.get("flux", 0.0)), float(ev.get("t", 0.0))),
    )


def _thin_selected_events_for_min_ioi(selected_events, difficulty):
    """Apply difficulty IOI floors during selection, not as a cleanup pass."""
    if len(selected_events) < 2:
        return selected_events

    kept = []
    for event in _ranked_events_for_ioi(selected_events.values()):
        if _clears_min_ioi(event, kept, difficulty):
            kept.append(event)

    return {
        _event_key(event): event
        for event in sorted(kept, key=lambda ev: (int(ev.get("beat_idx", -1)), float(ev.get("t", 0.0))))
    }


def _promote_subdivision_coverage(selected_events, candidate_events, difficulty):
    """Add sparse non-downbeat candidates when strict medium/hard coverage needs them."""
    required = MIN_SUBDIVISION_LABEL_KINDS.get(difficulty)
    if not required or len(selected_events) < 2:
        return selected_events

    present = {event.get("subdivision", "downbeat") for event in selected_events.values()}
    if len(present) >= required:
        return selected_events

    selected_list = list(selected_events.values())
    updated = dict(selected_events)
    for candidate in _ranked_events_for_ioi(candidate_events):
        label = candidate.get("subdivision", "downbeat")
        key = _event_key(candidate)
        if label in present or key in updated:
            continue
        if not _clears_min_ioi(candidate, selected_list, difficulty):
            continue
        # Issue #414 — when a higher-tier-tagged candidate is promoted into
        # a lower-difficulty array for coverage, retag it so the shipped
        # difficulty array no longer leaks a higher inclusion tier.
        candidate = {
            **candidate,
            "difficulty_inclusion": difficulty,
        }
        updated[key] = candidate
        selected_list.append(candidate)
        present.add(label)
        if len(present) >= required:
            break

    return dict(
        sorted(
            updated.items(),
            key=lambda item: (int(item[1].get("beat_idx", -1)), float(item[1].get("t", 0.0))),
        )
    )


def _median(values):
    if not values:
        return 0.0
    s = sorted(values)
    mid = len(s) // 2
    if len(s) % 2:
        return float(s[mid])
    return float((s[mid - 1] + s[mid]) / 2.0)


def _ioi_seconds(events):
    times = sorted(float(ev.get("t", 0.0)) for ev in events)
    return [times[i + 1] - times[i] for i in range(len(times) - 1)]


def _clamp_difficulty_inclusion(selected_events, difficulty):
    """Issue #414 — ensure no event in a shipped difficulty array carries a
    higher inclusion tier than that difficulty.

    The shipped contract is: ``difficulty_inclusion`` names the *minimum*
    difficulty at which the event is included.  An event sitting in the
    ``easy`` array therefore must be tagged ``easy``; an event sitting in
    ``medium`` may be ``easy`` or ``medium``; ``hard`` permits all three.

    The segment-focus rank-based tagging can leave higher-tier tags on
    events that survive into a lower-difficulty selection because of
    segment-size rounding (e.g. ``n_take = n_easy`` may include the
    boundary event whose rank-based tag was ``medium``).  Clamping the
    tag to the current difficulty is correct: by virtue of being included
    here, ``difficulty`` IS this event's minimum-inclusion tier.
    """
    order = {"easy": 0, "medium": 1, "hard": 2}
    cap = order.get(difficulty)
    if cap is None:
        return selected_events
    clamped = {}
    for key, event in selected_events.items():
        tag = event.get("difficulty_inclusion")
        if tag in order and order[tag] > cap:
            event = {**event, "difficulty_inclusion": difficulty}
        clamped[key] = event
    return clamped


def _resolve_onset_class(event):
    cls = event.get("onset_class") or "full-spectrum"
    if cls not in ONSET_CLASS_TO_OBSTACLE:
        cls = "full-spectrum"
    return cls


def _candidate_respects_run_cap(candidate, selected_list, difficulty):
    """Return True if inserting ``candidate`` into the time-ordered
    ``selected_list`` would not extend a same-onset-class run past the
    configured ``MAX_SAME_LANE_RUN`` cap (issue #391).

    The check matches the resolution used by
    :func:`_enforce_same_class_run_cap` and the obstacle emitter so the
    median-IOI promotion pass cannot stage events that the run-cap pass
    must immediately remove (issue #418).
    """
    cap = MAX_SAME_LANE_RUN.get(difficulty)
    if not cap:
        return True
    cand_t = float(candidate.get("t", 0.0))
    cand_cls = _resolve_onset_class(candidate)

    ordered = sorted(selected_list, key=lambda ev: float(ev.get("t", 0.0)))
    insert_idx = 0
    for ev in ordered:
        if float(ev.get("t", 0.0)) <= cand_t:
            insert_idx += 1
        else:
            break

    left_run = 0
    for ev in reversed(ordered[:insert_idx]):
        if _resolve_onset_class(ev) == cand_cls:
            left_run += 1
        else:
            break
    right_run = 0
    for ev in ordered[insert_idx:]:
        if _resolve_onset_class(ev) == cand_cls:
            right_run += 1
        else:
            break
    return left_run + 1 + right_run <= cap


def _resolve_segment_for_time(t, segment_ranges):
    """Issue #481 — return ``(segment_idx, segment_focus)`` for ``t``.

    ``segment_ranges`` is a list of ``(start_t, end_t, segment_idx, focus)``
    tuples produced by :func:`select_segment_focus_beats`.  Lookup is linear
    (segments are O(10) per song) and falls back to the last segment for
    times slightly past the final boundary so promoted candidates always
    inherit a non-null focus label.
    """
    if not segment_ranges:
        return None, None
    for start_t, end_t, seg_idx, focus in segment_ranges:
        if start_t <= t < end_t:
            return seg_idx, focus
    last = segment_ranges[-1]
    return last[2], last[3]


def _enforce_median_ioi_target(
    selected_events,
    candidate_events,
    difficulty,
    fallback_pool=None,
    respect_run_cap=False,
    segment_ranges=None,
):
    """Issues #392 / #394 / #418 / #472 / #481 — drive median IOI to the
    per-difficulty ceiling, preserving ``segment_focus`` metadata on
    promoted candidates.

    ``candidate_events`` are the per-segment focus-class events.  When that
    pool is exhausted ``fallback_pool`` (typically every snapped event) is
    mined for additional high-flux placements.  The ``MIN_IOI_MS`` floor
    is still respected.

    When ``respect_run_cap`` is True, candidates that would push a
    same-onset-class run past ``MAX_SAME_LANE_RUN[difficulty]`` are also
    rejected (issue #418).

    Promoted events are tagged with ``difficulty_inclusion = difficulty``
    (issue #414) and stamped with the surrounding segment's
    ``segment_focus`` / ``segment_idx`` (issue #481) so downstream
    diagnostics can attribute them to the originating section.
    """
    target = MEDIAN_IOI_TARGET_SEC.get(difficulty)
    if not target or len(selected_events) < 2:
        return selected_events

    def median_ioi(events):
        return _median(_ioi_seconds(events))

    selected_list = list(selected_events.values())
    if median_ioi(selected_list) <= target:
        return selected_events

    selected_keys = set(selected_events.keys())
    updated = dict(selected_events)

    def _try_promote(pool, enrich=False):
        nonlocal selected_list
        for candidate in _ranked_events_for_ioi(pool):
            key = _event_key(candidate)
            if key in selected_keys:
                continue
            if not _clears_min_ioi(candidate, selected_list, difficulty):
                continue
            if respect_run_cap and not _candidate_respects_run_cap(
                candidate, selected_list, difficulty
            ):
                continue
            if enrich and "onset_class" not in candidate:
                candidate = {
                    **candidate,
                    "onset_class": classify_onset_class(candidate),
                }
            # Issue #481 — stamp segment_focus / segment_idx on promoted
            # candidates whose source pool did not already carry them
            # (specifically the global fallback pool).
            if segment_ranges is not None and (
                candidate.get("segment_focus") is None
                or candidate.get("segment_idx") is None
            ):
                seg_idx, focus = _resolve_segment_for_time(
                    float(candidate.get("t", 0.0)), segment_ranges
                )
                stamp = {}
                if candidate.get("segment_focus") is None and focus is not None:
                    stamp["segment_focus"] = focus
                if candidate.get("segment_idx") is None and seg_idx is not None:
                    stamp["segment_idx"] = seg_idx
                if stamp:
                    candidate = {**candidate, **stamp}
            candidate = {
                **candidate,
                "difficulty_inclusion": difficulty,
            }
            updated[key] = candidate
            selected_keys.add(key)
            selected_list.append(candidate)
            if median_ioi(selected_list) <= target:
                return True
        return False

    if not _try_promote(candidate_events):
        # Issue #472 — every difficulty (not just hard) may mine the
        # global fallback pool here so easy/medium can converge to their
        # IOI ceilings.  The previous easy/medium-skip caused medians of
        # 1.0-1.5 s vs targets of 0.68-0.85 s on every shipped beatmap.
        # Difficulty-ramp monotony is preserved post-hoc by
        # _enforce_difficulty_count_ramp on the materialized obstacles.
        if fallback_pool:
            _try_promote(fallback_pool, enrich=True)

    return dict(
        sorted(
            updated.items(),
            key=lambda item: (int(item[1].get("beat_idx", -1)), float(item[1].get("t", 0.0))),
        )
    )


def _fill_silent_gaps(
    selected_events,
    fallback_pool,
    difficulty,
    bpm,
    segment_ranges=None,
    duration_sec=None,
):
    """Issue #506 / #527 — bound silent stretches by promoting real onsets.

    Mirrors ``tools/validate_max_beat_gap.py`` per-difficulty caps
    (``MAX_SILENT_GAP_BEATS``) for mid-song gaps, and the new lead-in /
    trail-out caps from issue #527 for the song boundaries.  Mines
    ``fallback_pool`` (real analysis onsets snapped to beats) for events
    that fall inside any oversized gap — including ``0 → first_t`` and
    ``last_t → duration_sec``.  No filler / synthetic events are ever
    produced — when a gap has no eligible real onset candidates (true
    silence in the audio) it is left intact and recorded so the loop
    does not spin on it.

    Constraints honored:
      * Onset-only:        candidates come from ``fallback_pool``.
      * IOI floor:         ``_clears_min_ioi`` (which now respects the
                           cross-layer 50 ms protection from issue #507).
      * Run cap:           ``_candidate_respects_run_cap``.
      * Segment metadata:  ``segment_focus`` / ``segment_idx`` stamped
                           via ``_resolve_segment_for_time`` (issue #481).
      * Broad-layer:       ``onset_class`` filled via
                           ``classify_onset_class`` only — no raw
                           instrument-pass leakage.
    """
    cap_beats = MAX_SILENT_GAP_BEATS.get(difficulty)
    if not cap_beats or not bpm or bpm <= 0 or len(selected_events) < 2:
        return selected_events
    cap_sec = cap_beats * 60.0 / float(bpm)
    # Issue #527 — lead-in / trail-out caps mirror
    # ``validate_max_beat_gap.MAX_LEAD_IN_SEC`` / ``MAX_TRAIL_OUT_SEC``.
    lead_caps = {"easy": 8.0, "medium": 6.0, "hard": 4.0}
    trail_caps = {"easy": 12.0, "medium": 10.0, "hard": 8.0}
    lead_cap = lead_caps.get(difficulty)
    trail_cap = trail_caps.get(difficulty)

    selected_keys = set(selected_events.keys())
    selected_list = sorted(
        selected_events.values(), key=lambda ev: float(ev.get("t", 0.0))
    )
    pool_ranked = _ranked_events_for_ioi(fallback_pool or [])
    updated = dict(selected_events)

    # (lo_t, hi_t) gap windows we have proven we cannot fill — skip on
    # subsequent iterations so the loop terminates on true silences.
    skipped: set[tuple[float, float]] = set()

    while True:
        gaps = []
        # Lead-in (0 → first event).
        first_t = float(selected_list[0].get("t", 0.0)) if selected_list else 0.0
        if lead_cap is not None and first_t > lead_cap:
            key = (0.0, round(first_t, 6))
            if key not in skipped:
                gaps.append((first_t, 0.0, first_t))
        # Mid-song.
        for i in range(len(selected_list) - 1):
            lo = float(selected_list[i].get("t", 0.0))
            hi = float(selected_list[i + 1].get("t", 0.0))
            dt = hi - lo
            if dt <= cap_sec:
                continue
            key = (round(lo, 6), round(hi, 6))
            if key in skipped:
                continue
            gaps.append((dt, lo, hi))
        # Trail-out (last → duration_sec).
        if (
            trail_cap is not None
            and duration_sec is not None
            and selected_list
        ):
            last_t = float(selected_list[-1].get("t", 0.0))
            tail_dt = float(duration_sec) - last_t
            if tail_dt > trail_cap:
                key = (round(last_t, 6), round(float(duration_sec), 6))
                if key not in skipped:
                    gaps.append((tail_dt, last_t, float(duration_sec)))
        if not gaps:
            break

        # Attack the widest gap first.
        gaps.sort(reverse=True)
        _dt, lo, hi = gaps[0]

        placed = False
        for candidate in pool_ranked:
            ct = float(candidate.get("t", 0.0))
            if ct <= lo or ct >= hi:
                continue
            cand_key = _event_key(candidate)
            if cand_key in selected_keys:
                continue

            # Stamp onset_class up-front so _clears_min_ioi /
            # _candidate_respects_run_cap see the correct broad layer
            # (raw fallback events carry ``layer`` but no ``onset_class``,
            # which would otherwise resolve to "full-spectrum" and block
            # legitimate harmonic / percussive candidates).
            stamped = dict(candidate)
            if "onset_class" not in stamped or stamped["onset_class"] is None:
                stamped["onset_class"] = classify_onset_class(stamped)

            if not _clears_min_ioi(stamped, selected_list, difficulty):
                continue
            if not _candidate_respects_run_cap(
                stamped, selected_list, difficulty
            ):
                continue

            if segment_ranges is not None and (
                stamped.get("segment_focus") is None
                or stamped.get("segment_idx") is None
            ):
                seg_idx, focus = _resolve_segment_for_time(ct, segment_ranges)
                if stamped.get("segment_focus") is None and focus is not None:
                    stamped["segment_focus"] = focus
                if stamped.get("segment_idx") is None and seg_idx is not None:
                    stamped["segment_idx"] = seg_idx
            stamped["difficulty_inclusion"] = difficulty

            updated[cand_key] = stamped
            selected_keys.add(cand_key)
            insert_at = 0
            while (
                insert_at < len(selected_list)
                and float(selected_list[insert_at].get("t", 0.0)) < ct
            ):
                insert_at += 1
            selected_list.insert(insert_at, stamped)
            placed = True
            break

        if not placed:
            skipped.add((round(lo, 6), round(hi, 6)))

    return dict(
        sorted(
            updated.items(),
            key=lambda item: (
                int(item[1].get("beat_idx", -1)),
                float(item[1].get("t", 0.0)),
            ),
        )
    )


def _collapse_simultaneous_obstacles(obstacles, collapsed_pairs=None):
    """Issue #528 — obstacle-level twin of :func:`_collapse_simultaneous_shape_gates`.

    Operates after onset_class → (lane, shape) mapping has been
    resolved.  Distinct public broad-layer onsets within the protected
    50 ms window remain distinct here; the morph-window playability
    collapse only applies outside that protected cross-layer window.
    """
    if not obstacles or len(obstacles) < 2:
        return obstacles
    ordered = sorted(
        ((i, o) for i, o in enumerate(obstacles)
         if o.get("kind") == "shape_gate" and isinstance(o.get("time_sec"), (int, float))),
        key=lambda io: float(io[1]["time_sec"]),
    )
    drop_idx: set = set()
    n = len(ordered)
    for i in range(n):
        ai, a = ordered[i]
        if ai in drop_idx:
            continue
        a_t = float(a["time_sec"])
        a_ls = (a.get("lane"), a.get("shape"))
        for j in range(i + 1, n):
            bi, b = ordered[j]
            if bi in drop_idx:
                continue
            b_t = float(b["time_sec"])
            if (b_t - a_t) > PLAYABILITY_MORPH_WINDOW_SEC:
                break
            if (b.get("lane"), b.get("shape")) == a_ls:
                continue
            a_class = a.get("onset_class")
            b_class = b.get("onset_class")
            if (
                a_class in ONSET_CLASS_TO_OBSTACLE
                and b_class in ONSET_CLASS_TO_OBSTACLE
                and a_class != b_class
                and (b_t - a_t) * 1000.0 <= PROTECTED_CROSS_LAYER_WINDOW_MS
            ):
                continue
            # Tie-break: highest flux wins; earliest t breaks tie.
            def prio(o):
                return (-float(o.get("flux", 0.0)),
                        float(o.get("time_sec", 0.0)))
            if prio(a) <= prio(b):
                kept, dropped = a, b
                drop_idx.add(bi)
            else:
                kept, dropped = b, a
                drop_idx.add(ai)
            if collapsed_pairs is not None:
                collapsed_pairs.append({
                    "t_kept": round(float(kept["time_sec"]), 6),
                    "t_dropped": round(float(dropped["time_sec"]), 6),
                    "delta_ms": round(abs(float(dropped["time_sec"]) - float(kept["time_sec"])) * 1000.0, 3),
                    "kept_onset_class": kept.get("onset_class"),
                    "dropped_onset_class": dropped.get("onset_class"),
                    "kept_lane_shape": [kept.get("lane"), kept.get("shape")],
                    "dropped_lane_shape": [dropped.get("lane"), dropped.get("shape")],
                    "kept_flux": float(kept.get("flux", 0.0)),
                    "dropped_flux": float(dropped.get("flux", 0.0)),
                })
            if a is dropped:
                break
    if not drop_idx:
        return obstacles
    return [o for i, o in enumerate(obstacles) if i not in drop_idx]


def _collapse_simultaneous_shape_gates(selected_events, collapsed_pairs=None):
    """Issue #528 — drop one event when two would emit shape_gate obstacles
    at different ``(lane, shape)`` within ``PLAYABILITY_MORPH_WINDOW_SEC``.

    Cross-layer onsets within 50 ms (``PROTECTED_CROSS_LAYER_WINDOW_MS``)
    are protected here as distinct musical events.  The morph-window
    playability collapse only applies outside that protected window.

    Tie-break (deterministic, per acceptance #528):
      1. Highest ``flux``.
      2. Earliest ``t``.
      3. ``segment_focus`` class as final tiebreak (alphabetical) so
         choice is reproducible across runs.

    The dropped event is recorded in ``collapsed_pairs`` (caller-provided
    list) so the analysis report can surface lost musical detail under
    ``playability_collapsed_pairs``.

    No filler / synthetic events are inserted; this is purely a
    selection-side de-conflict between real onsets.
    """
    if not selected_events or len(selected_events) < 2:
        return selected_events

    def lane_shape(ev):
        cls = ev.get("onset_class") or "full-spectrum"
        if cls not in ONSET_CLASS_TO_OBSTACLE:
            cls = "full-spectrum"
        m = ONSET_CLASS_TO_OBSTACLE[cls]
        return (m["lane"], m["shape"])

    def event_priority(ev):
        # Higher flux wins; on tie earlier t wins; on tie segment_focus
        # alphabetical wins.  Negate flux so max() picks largest.
        return (
            -float(ev.get("flux", 0.0)),
            float(ev.get("t", 0.0)),
            str(ev.get("segment_focus") or ""),
        )

    ordered = sorted(
        selected_events.items(),
        key=lambda kv: (
            float(kv[1].get("t", 0.0)),
            int(kv[1].get("beat_idx", -1)),
            int(kv[1].get("source_event_idx", -1)),
        ),
    )
    drop_keys: set = set()
    n = len(ordered)
    for i in range(n):
        if ordered[i][0] in drop_keys:
            continue
        a_key, a = ordered[i]
        a_t = float(a.get("t", 0.0))
        a_ls = lane_shape(a)
        for j in range(i + 1, n):
            b_key, b = ordered[j]
            if b_key in drop_keys:
                continue
            b_t = float(b.get("t", 0.0))
            if (b_t - a_t) > PLAYABILITY_MORPH_WINDOW_SEC:
                break
            if lane_shape(b) == a_ls:
                # Same playability target — let upstream IOI / cluster
                # passes handle same-lane same-shape stacking.
                continue
            a_class = a.get("onset_class")
            b_class = b.get("onset_class")
            if (
                a_class in ONSET_CLASS_TO_OBSTACLE
                and b_class in ONSET_CLASS_TO_OBSTACLE
                and a_class != b_class
                and (b_t - a_t) * 1000.0 <= PROTECTED_CROSS_LAYER_WINDOW_MS
            ):
                continue
            if event_priority(a) <= event_priority(b):
                kept_key, kept_ev = a_key, a
                drop_key, drop_ev = b_key, b
            else:
                kept_key, kept_ev = b_key, b
                drop_key, drop_ev = a_key, a
            drop_keys.add(drop_key)
            if collapsed_pairs is not None:
                collapsed_pairs.append({
                    "t_kept": round(float(kept_ev.get("t", 0.0)), 6),
                    "t_dropped": round(float(drop_ev.get("t", 0.0)), 6),
                    "delta_ms": round(abs(float(drop_ev.get("t", 0.0)) - float(kept_ev.get("t", 0.0))) * 1000.0, 3),
                    "kept_onset_class": kept_ev.get("onset_class"),
                    "dropped_onset_class": drop_ev.get("onset_class"),
                    "kept_segment_focus": kept_ev.get("segment_focus"),
                    "dropped_segment_focus": drop_ev.get("segment_focus"),
                    "kept_flux": float(kept_ev.get("flux", 0.0)),
                    "dropped_flux": float(drop_ev.get("flux", 0.0)),
                })
    if not drop_keys:
        return selected_events
    return {k: v for k, v in selected_events.items() if k not in drop_keys}


def _enforce_max_shape_cluster_size(selected_events, difficulty):
    """Issue #532 — thin same-(lane, shape) clusters whose size exceeds
    ``MAX_SHAPE_CLUSTER_SIZE[difficulty]``.

    A cluster is a run of consecutive events that share ``(lane, shape)``
    AND whose neighbour ``beat_idx`` distance is ≤ ``SHAPE_CLUSTER_GAP_BEATS``.
    Within an oversized cluster, the lowest-flux events are dropped until
    the cluster is at the cap.  Real onsets only — no replacement, no
    filler.
    """
    cap = MAX_SHAPE_CLUSTER_SIZE.get(difficulty)
    if not cap or not selected_events:
        return selected_events

    def lane_shape(ev):
        cls = ev.get("onset_class") or "full-spectrum"
        if cls not in ONSET_CLASS_TO_OBSTACLE:
            cls = "full-spectrum"
        m = ONSET_CLASS_TO_OBSTACLE[cls]
        return (m["lane"], m["shape"])

    while True:
        ordered = sorted(
            selected_events.items(),
            key=lambda kv: (
                int(kv[1].get("beat_idx", -1)),
                float(kv[1].get("t", 0.0)),
            ),
        )
        # Build clusters: same lane/shape AND consecutive beat_idx gap ≤ cap.
        clusters: list[list[tuple]] = []
        current: list[tuple] = []
        prev_key: tuple | None = None
        prev_beat: int | None = None
        for key, ev in ordered:
            ls = lane_shape(ev)
            beat_idx = int(ev.get("beat_idx", -1))
            if (
                prev_key is not None
                and ls == prev_key
                and prev_beat is not None
                and (beat_idx - prev_beat) < SHAPE_CLUSTER_GAP_BEATS
            ):
                current.append((key, ev))
            else:
                if current:
                    clusters.append(current)
                current = [(key, ev)]
            prev_key = ls
            prev_beat = beat_idx
        if current:
            clusters.append(current)

        oversized = [c for c in clusters if len(c) > cap]
        if not oversized:
            return selected_events
        # Drop the single lowest-flux event in the largest oversized cluster
        # then re-evaluate (cluster boundaries may shift after a drop).
        target = max(oversized, key=len)
        worst_key, _ = min(target, key=lambda kv: float(kv[1].get("flux", 0.0)))
        del selected_events[worst_key]


def _enforce_same_class_run_cap(selected_events, difficulty):
    """Issue #391 — cap consecutive same-onset-class (==same-lane) runs.

    The canonical ``onset_class → lane`` invariant (see
    ``ONSET_CLASS_TO_OBSTACLE`` and
    ``test_experimental_mode_applies_class_lane_shape_mapping``) means a
    same-lane run is exactly a same-onset-class run.  We enforce the cap
    here, on the selected_events dict produced by the segment-focus
    selector, BEFORE ``design_level_segment_focus`` materializes
    obstacles — so the cleanup-not-invoked invariant
    (``test_cleanup_not_invoked_in_segment_focus``) continues to hold:
    selected events and emitted obstacles still match 1:1.

    Events are scanned in (beat_idx, t) order; an event whose onset_class
    would extend the run past ``MAX_SAME_LANE_RUN[difficulty]`` is dropped.
    """
    cap = MAX_SAME_LANE_RUN.get(difficulty)
    if not cap or not selected_events:
        return selected_events

    ordered_keys = sorted(
        selected_events.keys(),
        key=lambda k: (
            int(selected_events[k].get("beat_idx", -1)),
            float(selected_events[k].get("t", 0.0)),
            int(selected_events[k].get("source_event_idx", -1)),
        ),
    )

    kept = {}
    run_class = None
    run_len = 0
    for key in ordered_keys:
        ev = selected_events[key]
        # Match the obstacle-emitter's class resolution exactly (see
        # design_level_segment_focus): prefer the stored onset_class field,
        # falling back to "full-spectrum" — this is what determines the
        # final lane via ONSET_CLASS_TO_OBSTACLE.
        cls = ev.get("onset_class") or "full-spectrum"
        if cls not in ONSET_CLASS_TO_OBSTACLE:
            cls = "full-spectrum"
        if cls == run_class:
            if run_len >= cap:
                continue
            run_len += 1
        else:
            run_class = cls
            run_len = 1
        kept[key] = ev

    return kept


def _rebalance_medium_by_onset_selection(selected_events, all_snapped, segment_ranges=None):
    """Select/drop real medium onsets to satisfy shipped shape-share gates."""
    if not selected_events or len(selected_events) < 24:
        return selected_events

    def event_class(ev):
        return ev.get("onset_class") if ev.get("onset_class") in ONSET_CLASS_TO_OBSTACLE else classify_onset_class(ev)

    def counts(events):
        c = Counter()
        for ev in events.values():
            c[event_class(ev)] += 1
        return c

    def pct(c, cls, total):
        return (c.get(cls, 0) * 100.0 / total) if total else 0.0

    updated = dict(selected_events)
    selected_keys = set(updated.keys())

    def promote_class(cls, floor_pct):
        nonlocal updated, selected_keys
        candidates = [
            ev for ev in all_snapped
            if event_class(ev) == cls and _event_key(ev) not in selected_keys
        ]
        candidates.sort(key=lambda ev: (-float(ev.get("flux", 0.0)), float(ev.get("t", 0.0))))
        for candidate in candidates:
            c = counts(updated)
            total = sum(c.values())
            if pct(c, cls, total) >= floor_pct:
                return
            enriched = {
                **candidate,
                "onset_class": cls,
                "difficulty_inclusion": "medium",
                "segment_focus": cls,
            }
            if not _candidate_respects_run_cap(enriched, list(updated.values()), "medium"):
                continue
            seg_idx, seg_focus = _resolve_segment_for_time(float(enriched.get("t", 0.0)), segment_ranges or [])
            enriched["segment_idx"] = seg_idx
            enriched["segment_focus"] = seg_focus or cls
            key = _event_key(enriched)
            updated[key] = enriched
            selected_keys.add(key)

    def drop_class(cls, ceiling_pct):
        nonlocal updated, selected_keys
        while True:
            c = counts(updated)
            total = sum(c.values())
            if pct(c, cls, total) <= ceiling_pct:
                return
            class_items = [
                (key, ev) for key, ev in updated.items()
                if event_class(ev) == cls
            ]
            if not class_items:
                return
            key, _ev = min(
                class_items,
                key=lambda item: (float(item[1].get("flux", 0.0)), -float(item[1].get("t", 0.0))),
            )
            del updated[key]
            selected_keys.discard(key)

    promote_class("full-spectrum", MEDIUM_SHAPE_TARGETS[1][0])
    drop_class("harmonic", MEDIUM_SHAPE_TARGETS[2][1] - 1)
    promote_class("harmonic", MEDIUM_SHAPE_TARGETS[2][0])
    promote_class("full-spectrum", MEDIUM_SHAPE_TARGETS[1][0])
    return updated


def select_segment_focus_beats(analysis, difficulty):
    """Segment-level onset focus selector — the active experimental generation path.

    Algorithm per segment:
      1. Classify all snapped events into percussive / harmonic / full-spectrum.
      2. Choose dominant class (focus) based on total flux, with anti-repeat bias.
      3. Rank focus-class events by flux descending (time ascending as tiebreak).
      4. Take the top fraction based on difficulty:
           easy   → top 40%  (strongest onsets only)
           medium → top 65%
           hard   → top 90%
      5. Map each selected event's onset_class to the segment focus (not individual class).

    NOTE (directive 2026-05-10): intro-rest skipping is NOT applied in this path.
    Obstacles are authored only from onset events; every onset in the song is
    eligible regardless of its beat index.  Beat-fallback placement is also
    disabled — if a segment has no onset events it produces no obstacles.

    Returns:
      selected_events  — {(beat_idx, onset_class, source_event_idx): enriched_event_dict}
      all_snapped_map  — {(beat_idx, onset_class, source_event_idx): snapped_event}
      diagnostics      — dict with per-segment details
    """
    beats = analysis["beats"]
    events = analysis["events"]
    structure = analysis.get("structure", [])
    fraction = SEGMENT_FOCUS_DIFFICULTY_FRACTION[difficulty]

    # Snap all events to the beat grid once.
    all_snapped = snap_events_to_beats(events, beats)

    # Issue #476 — enforce the per-difficulty first-collision reaction
    # floor at the SELECTOR level (pre-materialization), analogous to
    # _enforce_same_class_run_cap.  Drop snapped onsets whose audio time
    # falls inside the floor so no obstacle (selected or fallback-promoted)
    # can spawn before the player has time to register the song.  This is
    # selection, not cleanup, and uses onset events only — no filler is
    # inserted.
    floor_sec = MIN_FIRST_COLLISION_SEC.get(difficulty)
    if floor_sec is not None:
        all_snapped = [
            ev for ev in all_snapped if float(ev.get("t", 0.0)) >= floor_sec
        ]

    # Index snapped events for efficient lookup.
    all_snapped_map = {}
    for ev in all_snapped:
        all_snapped_map[_event_key(ev)] = ev

    # Fall back to a single whole-song segment if structure is absent.
    sections = list(structure) if structure else [
        {"section": "verse", "start": 0.0, "end": float(beats[-1]) + 1.0}
    ]

    selected_events = {}   # (beat_idx, onset_class, source_event_idx) → enriched event dict
    candidate_events = []  # enriched ranked focus events available for coverage promotion
    prev_focuses = []      # rolling focus history for anti-repetition
    segment_diagnostics = []
    # Issue #481 — list of (start_t, end_t, segment_idx, focus) ranges so
    # _enforce_median_ioi_target can stamp segment metadata onto candidates
    # promoted from the global fallback pool.
    segment_ranges: list[tuple[float, float, int, str]] = []

    for seg_idx, section in enumerate(sections):
        sec_start = float(section["start"])
        sec_end = float(section["end"])
        sec_name = section.get("section", "verse")

        # All snapped events whose beat timestamp falls in this segment.
        # No intro-rest filter: every onset event is eligible.
        seg_events = [
            ev for ev in all_snapped
            if sec_start <= float(ev["beat_time"]) < sec_end
        ]

        class_stats = _segment_class_stats(seg_events)

        # Choose focus class.
        focus, fallback_reason = _choose_segment_focus(class_stats, prev_focuses)
        prev_focuses.append(focus)
        # Issue #481 — record this segment's resolved focus so the IOI
        # promoter can stamp metadata onto candidates pulled from the
        # global fallback pool.
        segment_ranges.append((sec_start, sec_end, seg_idx, focus))

        # Events of the chosen focus class; fall back to all events if none.
        focus_events = class_stats.get(focus, {}).get("events", [])
        if not focus_events:
            focus_events = seg_events
            fallback_reason = (fallback_reason or "") + f"|no_{focus}_events_using_all"

        if not focus_events:
            segment_diagnostics.append({
                "segment_idx": seg_idx,
                "section": sec_name,
                "start": sec_start,
                "end": sec_end,
                "focus_class": focus,
                "fallback_reason": fallback_reason or "empty_segment",
                "class_counts": {},
                "class_flux": {},
                "focus_events_total": 0,
                "difficulty_selected": 0,
                "ranked_flux_p50": 0.0,
                "ranked_flux_max": 0.0,
            })
            continue

        # Rank by flux descending, onset time ascending (deterministic).
        ranked = sorted(
            focus_events,
            key=lambda ev: (-float(ev.get("flux", 0.0)), float(ev.get("t", 0.0))),
        )

        # Compute difficulty thresholds across all three levels for diagnostics.
        n_easy = max(1, round(len(ranked) * SEGMENT_FOCUS_DIFFICULTY_FRACTION["easy"]))
        n_medium = max(1, round(len(ranked) * SEGMENT_FOCUS_DIFFICULTY_FRACTION["medium"]))
        n_hard = max(1, round(len(ranked) * SEGMENT_FOCUS_DIFFICULTY_FRACTION["hard"]))
        n_take = {"easy": n_easy, "medium": n_medium, "hard": n_hard}.get(difficulty, n_hard)

        # Tag each ranked event with its minimum-difficulty inclusion level.
        for rank_i, ev in enumerate(ranked):
            if rank_i < n_easy:
                ev["difficulty_inclusion"] = "easy"
            elif rank_i < n_medium:
                ev["difficulty_inclusion"] = "medium"
            else:
                ev["difficulty_inclusion"] = "hard"

        chosen = ranked[:n_take]

        flux_vals = sorted(float(ev.get("flux", 0.0)) for ev in ranked)
        segment_diagnostics.append({
            "segment_idx": seg_idx,
            "section": sec_name,
            "start": sec_start,
            "end": sec_end,
            "focus_class": focus,
            "fallback_reason": fallback_reason,
            "class_counts": {cls: s["count"] for cls, s in class_stats.items()},
            "class_flux": {cls: round(s["total_flux"], 4) for cls, s in class_stats.items()},
            "focus_events_total": len(focus_events),
            "difficulty_selected": n_take,
            "ranked_flux_p50": round(_percentile(flux_vals, 0.5), 4),
            "ranked_flux_max": round(flux_vals[-1], 4) if flux_vals else 0.0,
            "n_easy": n_easy,
            "n_medium": n_medium,
            "n_hard": n_hard,
        })

        enriched_ranked = []
        for ev in ranked:
            event_onset_class = classify_onset_class(ev)
            enriched_ranked.append({
                **ev,
                "onset_class": event_onset_class,
                "segment_idx": seg_idx,
                "segment_focus": focus,
                "difficulty_inclusion": ev.get("difficulty_inclusion", difficulty),
                # Keep source_event_idx for CSV diagnostics (snap_events_to_beats sets it).
                "source_event_idx": ev.get("source_event_idx"),
            })
        candidate_events.extend(enriched_ranked)

        # Insert chosen events keyed by beat/layer/source to preserve same-beat layers.
        for event in enriched_ranked[:n_take]:
            selected_events[_event_key(event)] = event

    selected_events = _thin_selected_events_for_min_ioi(selected_events, difficulty)
    # Issue #472 / #418 — first IOI pass mirrors the second-pass policy:
    # medium SKIPS the global fallback pool here.  Mining the fallback
    # for medium on dense songs (drama) over-promotes by ~110 events,
    # which the run-cap then cannot fully reverse, leaving medium denser
    # than hard and inverting the difficulty ramp (acceptance test
    # `test_drama_medium_to_hard_ioi_step_is_perceptible`).  Easy and
    # hard still mine the fallback so sparse songs (Stomper) can
    # converge toward their IOI ceilings.
    selected_events = _enforce_median_ioi_target(
        selected_events,
        candidate_events,
        difficulty,
        fallback_pool=all_snapped if difficulty != "medium" else None,
        segment_ranges=segment_ranges,
    )
    selected_events = _promote_subdivision_coverage(selected_events, candidate_events, difficulty)
    # Issue #391 — cap consecutive same-onset-class (==same-lane) runs.
    # Done here on selected_events (not on obstacles) so the segment-focus
    # cleanup-not-invoked invariant continues to hold.
    selected_events = _enforce_same_class_run_cap(selected_events, difficulty)
    # Issue #418 — the run-cap pass can drop large numbers of events on
    # single-class-dominant songs (e.g. drama, percussive-dominant), which
    # inflates median IOI back above the difficulty target.  Re-run the
    # IOI promotion pass with the run-cap respected so promoted events
    # cannot be removed by the next run-cap call, and apply a final cap
    # pass as a safety net.
    selected_events = _enforce_median_ioi_target(
        selected_events,
        candidate_events,
        difficulty,
        # Issue #472 — second-pass fallback usage:
        # easy/hard get the global fallback so easy can converge to its
        # IOI target after run-cap drops, and hard can recover from the
        # single-class-dominant case (issue #418).  Medium intentionally
        # SKIPS the fallback in the second pass — including it produced
        # medium counts above hard for dense songs (drama), inverting
        # the medium↔hard density step (#418 acceptance test) and
        # breaking the difficulty-ramp monotony invariant.
        fallback_pool=all_snapped if difficulty != "medium" else None,
        respect_run_cap=True,
        segment_ranges=segment_ranges,
    )
    selected_events = _enforce_same_class_run_cap(selected_events, difficulty)
    # Issue #506 — bound mid-song silent stretches to the per-difficulty
    # caps from validate_max_beat_gap.py.  Runs AFTER the IOI/run-cap
    # passes (so it cannot promote events the run cap would just drop)
    # and BEFORE _clamp_difficulty_inclusion (so promoted onsets keep
    # their stamped difficulty_inclusion = `difficulty` and survive the
    # clamp).  All three difficulties mine the global ``all_snapped``
    # fallback pool: this fix is exclusively about silences, never about
    # raising overall density.  Cross-layer 50 ms protection is preserved
    # via the issue #507 fix to ``_is_protected_cross_layer_pair``.
    selected_events = _fill_silent_gaps(
        selected_events,
        all_snapped,
        difficulty,
        analysis.get("bpm"),
        segment_ranges=segment_ranges,
        duration_sec=analysis.get("duration"),
    )
    if difficulty == "medium":
        selected_events = _rebalance_medium_by_onset_selection(selected_events, all_snapped, segment_ranges)
        selected_events = _thin_selected_events_for_min_ioi(selected_events, difficulty)
        selected_events = _enforce_same_class_run_cap(selected_events, difficulty)
        selected_events = _rebalance_medium_by_onset_selection(selected_events, all_snapped, segment_ranges)
    # Issue #532 — strict cap on dense same-shape clusters (mirrors the
    # validator gate in tools/validate_loop2_content_gates.py).  Real
    # onsets only — drops the lowest-flux event(s) in any oversized
    # cluster.  Easy is unconstrained (already very sparse).
    selected_events = _enforce_max_shape_cluster_size(selected_events, difficulty)
    if difficulty == "medium":
        selected_events = _rebalance_medium_by_onset_selection(selected_events, all_snapped, segment_ranges)
    selected_events = _fill_silent_gaps(
        selected_events,
        all_snapped,
        difficulty,
        analysis.get("bpm"),
        segment_ranges=segment_ranges,
        duration_sec=analysis.get("duration"),
    )
    # Issue #528 — collapse runs at the OBSTACLE level inside
    # design_level_segment_focus (after onset_class → (lane, shape)
    # mapping has been resolved).  Cross-layer onsets at the analysis
    # level are preserved here so directive 2026-05-10 invariants stay
    # green; the playability minimum is enforced on emitted obstacles.
    playability_collapsed_pairs: list[dict] = []
    # Issue #414 — clamp difficulty_inclusion so each shipped difficulty
    # array contains only events whose inclusion tier is <= the array's
    # tier.  Promoted/coverage events are already retagged at insertion;
    # this final clamp also covers any focus-class events whose
    # rank-based tag was strictly higher than the current difficulty
    # (which only happens when n_take pulls in higher-tier ranks because
    # of segment-size rounding).
    selected_events = _clamp_difficulty_inclusion(selected_events, difficulty)

    diagnostics = {
        "generation_path": "segment_focus",
        "difficulty": difficulty,
        "segments_total": len(sections),
        "events_total": len(all_snapped),
        "events_selected": len(selected_events),
        "focus_fraction": fraction,
        "segments": segment_diagnostics,
        # Issue #528 — list of cross-layer onset pairs that were collapsed
        # to satisfy the MORPH_DURATION playability minimum.  Empty list
        # means no near-simultaneous distinct shape_gate pair survived
        # generation.  Raw instrument names are never included.
        "playability_collapsed_pairs": playability_collapsed_pairs,
    }
    return selected_events, all_snapped_map, diagnostics


def design_level_segment_focus(analysis, difficulty, cleanup_enabled=False):
    """Active experimental path: segment-level focus class drives lane/shape selection.

    Cleanup passes are intentionally DISABLED for this onset-only path
    (directive 2026-05-10).  The parameter is accepted for call-site
    compatibility but its value is always ignored — passing True has no effect.

    Motif n-gram detection is NOT used for obstacle selection here.
    Class mapping: percussive→lane 0+triangle, harmonic→lane 2+circle,
                   full-spectrum→lane 1+square.
    Difficulty controls how many onset events per segment are included.
    """
    # Cleanup is explicitly disabled for this path regardless of the parameter.
    _ = cleanup_enabled  # noqa: F841 — intentionally unused; no cleanup runs here
    selected, all_snapped_map, seg_diagnostics = select_segment_focus_beats(
        analysis, difficulty
    )
    obstacles = []
    used_beat_indices: set[int] = set()
    beats_len = len(analysis.get("beats", [])) or 0
    for event in sorted(
        selected.values(),
        key=lambda item: (
            int(item.get("beat_idx", -1)),
            float(item.get("t", 0.0)),
            int(item.get("source_event_idx", -1)),
        ),
    ):
        focus = event.get("onset_class", "full-spectrum")
        mapping = ONSET_CLASS_TO_OBSTACLE.get(focus, ONSET_CLASS_TO_OBSTACLE["full-spectrum"])
        # Issue #416 — strict ordinal monotonicity.  Multiple onset
        # events (e.g. a downbeat and a triplet ~300ms later) can snap
        # to the same beat-grid index; emitting both with the same
        # ``beat`` value yields a non-strictly-increasing beats array
        # that fails the shipped beatmap quality gates with min IOI 0.
        # Time ordering is preserved (the sort above is stable on
        # ``t``) and the runtime drives spawn timing from
        # ``time_sec``, not the beat ordinal.  Advance to the next
        # free integer (still within ``beat_times`` bounds so the C++
        # loader's beat_index lookup stays valid).
        beat_idx = int(event.get("beat_idx", -1))
        if beat_idx in used_beat_indices:
            limit = beats_len - 1 if beats_len else beat_idx + 64
            next_idx = beat_idx + 1
            while next_idx in used_beat_indices and next_idx <= limit:
                next_idx += 1
            if next_idx <= limit:
                beat_idx = next_idx
        used_beat_indices.add(beat_idx)
        obstacles.append({
            "beat": beat_idx,
            "kind": "shape_gate",
            "lane": mapping["lane"],
            "shape": mapping["shape"],
            "onset_class": focus,
            "segment_idx": event.get("segment_idx"),
            "segment_focus": event.get("segment_focus"),
            "difficulty_inclusion": event.get("difficulty_inclusion", difficulty),
            "source_event_idx": event.get("source_event_idx"),
            # #528 — flux retained on the obstacle so post-emit validators
            # / diagnostics can audit the playability collapse decisions.
            "flux": float(event.get("flux", 0.0)),
            # #528 — onset time for the obstacle-level collapse pass below.
            # Stripped before write_beatmap; build_beatmap re-derives
            # ``time_sec`` from beat_times anyway.
            "time_sec": float(event.get("t", 0.0)),
        })
    # Issue #528 — obstacle-level playability collapse.  Cross-layer
    # onsets at the analysis layer are preserved (directive 2026-05-10);
    # here we de-conflict pairs that map to distinct (lane, shape)
    # within ``PLAYABILITY_MORPH_WINDOW_SEC``.
    collapsed_pairs: list[dict] = []
    obstacles = _collapse_simultaneous_obstacles(obstacles, collapsed_pairs=collapsed_pairs)
    if difficulty == "medium":
        used_sources = {obs.get("source_event_idx") for obs in obstacles}
        used_beats = {int(obs.get("beat", -1)) for obs in obstacles}
        harmonic_candidates = [
            ev for ev in selected.values()
            if ev.get("onset_class") == "harmonic" and ev.get("source_event_idx") not in used_sources
        ]
        harmonic_candidates.sort(key=lambda ev: (-float(ev.get("flux", 0.0)), float(ev.get("t", 0.0))))
        for event in harmonic_candidates:
            total = len(obstacles)
            circles = sum(1 for obs in obstacles if obs.get("shape") == "circle")
            if total > 0 and circles / total >= (MEDIUM_SHAPE_TARGETS[2][0] / 100.0):
                break
            event_time = float(event.get("t", 0.0))
            if any(abs(event_time - float(obs.get("time_sec", 0.0))) < PLAYABILITY_MORPH_WINDOW_SEC for obs in obstacles):
                continue
            beat_idx = int(event.get("beat_idx", -1))
            limit = beats_len - 1 if beats_len else beat_idx + 64
            while beat_idx in used_beats and beat_idx <= limit:
                beat_idx += 1
            if beat_idx > limit:
                continue
            mapping = ONSET_CLASS_TO_OBSTACLE["harmonic"]
            used_beats.add(beat_idx)
            obstacles.append({
                "beat": beat_idx,
                "kind": "shape_gate",
                "lane": mapping["lane"],
                "shape": mapping["shape"],
                "onset_class": "harmonic",
                "segment_idx": event.get("segment_idx"),
                "segment_focus": event.get("segment_focus"),
                "difficulty_inclusion": event.get("difficulty_inclusion", difficulty),
                "source_event_idx": event.get("source_event_idx"),
                "flux": float(event.get("flux", 0.0)),
                "time_sec": event_time,
            })
        while obstacles:
            total = len(obstacles)
            circles = sum(1 for obs in obstacles if obs.get("shape") == "circle")
            if circles / total >= (MEDIUM_SHAPE_TARGETS[2][0] / 100.0):
                break
            removable = [obs for obs in obstacles if obs.get("shape") != "circle"]
            if not removable:
                break
            weakest = min(removable, key=lambda obs: (float(obs.get("flux", 0.0)), -float(obs.get("time_sec", 0.0))))
            obstacles.remove(weakest)
        obstacles.sort(key=lambda obs: (int(obs.get("beat", -1)), float(obs.get("time_sec", 0.0))))
    seg_diagnostics["playability_collapsed_pairs"] = collapsed_pairs
    # No cleanup: obstacles come directly from onsets; no post-processing passes.
    return obstacles, selected, seg_diagnostics


def design_level_onset_motif(analysis, difficulty, cleanup_enabled=True):
    """Motif-pipeline (kept for diagnostics). NOT the active generation path."""
    selected, event_map, motif_diagnostics = select_onset_motif_beats(analysis, difficulty)
    obstacles = []
    for beat_idx, event in sorted(selected.items()):
        mapping = ONSET_CLASS_TO_OBSTACLE.get(
            event.get("onset_class"),
            ONSET_CLASS_TO_OBSTACLE["full-spectrum"],
        )
        obstacles.append({
            "beat": int(beat_idx),
            "kind": "shape_gate",
            "lane": mapping["lane"],
            "shape": mapping["shape"],
            "onset_class": event.get("onset_class", "full-spectrum"),
            "motif_id": event.get("motif_id"),
            "motif_length_beats": event.get("motif_length_beats", 0.0),
            "motif_token_length": event.get("motif_token_length", 0),
            "motif_repeat_count": event.get("motif_repeat_count", 0),
            "motif_fingerprint": event.get("motif_fingerprint", ""),
            "motif_event_role": event.get("motif_event_role", "fill"),
            "difficulty_inclusion": event.get("motif_event_role", "fill"),
            "source_event_idx": event.get("event_idx"),
        })

    del analysis, difficulty, cleanup_enabled
    return obstacles, selected, motif_diagnostics


# ═══════════════════════════════════════════════════════════════
# PHASE 2: ASSIGN — what obstacle goes on each beat?
# ═══════════════════════════════════════════════════════════════

def assign_obstacle(beat_idx, event, gap_to_prev, section_name, difficulty,
                    prev_lane, prev_shapes, allowed_kinds, rng, shape_state):
    """Diagnostics-only legacy beat-grid obstacle assignment."""
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


def thin_oversized_shape_clusters(obstacles, difficulty):
    """Trim the minimum obstacles needed to cap dense cluster size per difficulty."""
    cap = DENSE_CLUSTER_SOFT_CAP.get(difficulty)
    max_diff = MAX_BEAT_DIFF.get(difficulty, 999)
    if cap is None or len(obstacles) < cap + 1:
        return obstacles

    result = list(obstacles)
    for _ in range(len(result)):
        clusters = shape_gate_clusters(result)
        oversized = next((cluster for cluster in clusters if len(cluster) > cap), None)
        if oversized is None:
            break

        target_idx = None
        best_key = None
        midpoint = (len(oversized) - 1) / 2.0
        for pos in range(1, len(oversized) - 1):
            idx = oversized[pos]
            left_gap = result[idx]["beat"] - result[idx - 1]["beat"]
            right_gap = result[idx + 1]["beat"] - result[idx]["beat"]
            merged_gap = result[idx + 1]["beat"] - result[idx - 1]["beat"]
            if merged_gap > max_diff:
                continue
            gap_one_relief = int(left_gap == 1) + int(right_gap == 1)
            key = (-gap_one_relief, merged_gap, abs(pos - midpoint), idx)
            if best_key is None or key < best_key:
                best_key = key
                target_idx = idx

        if target_idx is None:
            target_idx = oversized[len(oversized) // 2]
        del result[target_idx]

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
    """Diagnostics-only legacy filler for beat-grid obstacle gaps."""
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
    cap = MAX_SAME_LANE_RUN.get(difficulty, 0)
    for _ in range(200):
        counts, total = medium_shape_counts(obstacles)
        if total == 0:
            break

        key = tuple(counts[i] for i in range(3))
        if key in seen:
            break
        seen.add(key)

        clusters = shape_gate_clusters(obstacles)
        cluster_lanes = [shape_cluster_lane(obstacles, cluster) for cluster in clusters]
        cluster_sizes = [len(cluster) for cluster in clusters]
        current_score = shape_balance_score(counts, total)
        current_run = longest_same_shape_run_from_clusters(cluster_lanes, cluster_sizes)
        current_run_excess = max(0, current_run - cap) if cap else 0
        if current_score == 0 and current_run_excess == 0:
            break

        best = None
        best_obj = (current_score, current_run_excess, current_run)
        best_tie = None

        for cluster_idx, cluster in enumerate(clusters):
            donor = cluster_lanes[cluster_idx]
            size = len(cluster)
            donor_low, _ = target_count_range(donor, total)
            for recipient in range(3):
                if recipient == donor:
                    continue
                _, recipient_high = target_count_range(recipient, total)
                new_counts = dict(counts)
                new_counts[donor] -= size
                new_counts[recipient] += size
                if new_counts[donor] < donor_low or new_counts[recipient] > recipient_high:
                    continue
                candidate_lanes = list(cluster_lanes)
                candidate_lanes[cluster_idx] = recipient
                new_run = longest_same_shape_run_from_clusters(candidate_lanes, cluster_sizes)
                new_run_excess = max(0, new_run - cap) if cap else 0
                new_obj = (shape_balance_score(new_counts, total), new_run_excess, new_run)
                tie = (size, cluster_idx, recipient)
                if new_obj < best_obj or (new_obj == best_obj and best_tie and tie < best_tie):
                    best = (cluster, recipient)
                    best_obj = new_obj
                    best_tie = tie

        if best is None:
            break

        cluster, shape_idx = best
        shape = LANE_TO_SHAPE[shape_idx]
        for i in cluster:
            set_shape_gate(obstacles[i], shape)

    return optimize_medium_lane_runs(obstacles, difficulty)


def max_same_lane_run(obstacles):
    longest = 0
    run = 0
    prev_lane = None
    for obs in obstacles:
        if not is_shape_gate(obs):
            prev_lane = None
            run = 0
            continue
        lane = SHAPE_TO_LANE.get(obs.get("shape"), clamp_lane(obs.get("lane", 1)))
        if lane == prev_lane:
            run += 1
        else:
            prev_lane = lane
            run = 1
        longest = max(longest, run)
    return longest


def lane_run_excess(obstacles, cap):
    longest = 0
    total_excess = 0
    run = 0
    prev_lane = None
    for obs in obstacles:
        if not is_shape_gate(obs):
            if run > cap:
                total_excess += run - cap
            prev_lane = None
            run = 0
            continue
        lane = SHAPE_TO_LANE.get(obs.get("shape"), clamp_lane(obs.get("lane", 1)))
        if lane == prev_lane:
            run += 1
        else:
            if run > cap:
                total_excess += run - cap
            prev_lane = lane
            run = 1
        longest = max(longest, run)
    if run > cap:
        total_excess += run - cap
    return max(0, longest - cap), total_excess, longest


def optimize_medium_lane_runs(obstacles, difficulty):
    """Keep medium shape balance while breaking post-rebalance lane walls."""
    if difficulty != "medium" or not obstacles:
        return obstacles

    cap = MAX_SAME_LANE_RUN.get(difficulty)
    if not cap:
        return obstacles

    def objective():
        counts, total = medium_shape_counts(obstacles)
        max_excess, total_excess, run = lane_run_excess(obstacles, cap)
        return (max_excess, total_excess, shape_balance_score(counts, total), run)

    for _ in range(500):
        current = objective()
        if current[0] == 0 and current[2] == 0:
            break

        best = None
        best_obj = current
        best_tie = None
        for idx, obs in enumerate(obstacles):
            if not is_shape_gate(obs):
                continue
            original_shape = obs.get("shape", "square")
            original_lane = SHAPE_TO_LANE.get(original_shape)
            if original_lane is None:
                original_lane = clamp_lane(obs.get("lane", 1))
                original_shape = LANE_TO_SHAPE[original_lane]
            for lane, shape in LANE_TO_SHAPE.items():
                if lane == original_lane:
                    continue
                set_shape_gate(obs, shape)
                candidate = objective()
                tie = (idx, lane)
                if candidate < best_obj or (candidate == best_obj and best_tie and tie < best_tie):
                    best = (idx, original_shape, shape)
                    best_obj = candidate
                    best_tie = tie
                set_shape_gate(obs, original_shape)

        if best is None:
            break

        idx, _, shape = best
        set_shape_gate(obstacles[idx], shape)

    return obstacles


def hard_shape_score(counts, total):
    if total == 0:
        return 0
    triangle_min = (HARD_TRIANGLE_FLOOR_PCT * total + 99) // 100
    circle_max = (HARD_CIRCLE_CEIL_PCT * total) // 100
    triangle_lane = SHAPE_TO_LANE["triangle"]
    circle_lane = SHAPE_TO_LANE["circle"]
    return (
        max(0, triangle_min - counts[triangle_lane])
        + max(0, counts[circle_lane] - circle_max)
    )


def rebalance_hard_shapes(obstacles, difficulty):
    """Mutate hard shape clusters to satisfy triangle floor / circle ceiling."""
    if difficulty != "hard" or not obstacles:
        return obstacles

    obstacles = clean_shape_change_gap(obstacles)
    seen = set()
    triangle_lane = SHAPE_TO_LANE["triangle"]
    circle_lane = SHAPE_TO_LANE["circle"]
    square_lane = SHAPE_TO_LANE["square"]
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
        triangle_need = max(0, triangle_min - counts[triangle_lane])
        circle_excess = max(0, counts[circle_lane] - circle_max)

        recipient = triangle_lane if triangle_need > 0 else square_lane
        donors = (
            [circle_lane, square_lane] if circle_excess > 0
            else [square_lane, circle_lane]
        )

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
    obstacles = thin_oversized_shape_clusters(obstacles, difficulty)
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


def _thin_oversized_clusters_obstacles(obstacles, difficulty):
    """Issue #532 — obstacle-level safety net for the cluster size cap.

    Mirrors ``tools/validate_loop2_content_gates.py::_shape_gate_clusters``:
    a cluster is a run of consecutive ``shape_gate`` obstacles whose
    beat-ordinal distance to the previous obstacle is < ``SHAPE_CLUSTER_GAP``.
    The cluster is built from any shapes/lanes (it measures local
    density, not same-shape monotony).  Cluster size > cap is thinned by
    dropping the lowest-flux obstacle until the cap is satisfied.  Real
    onsets only — no replacement, no filler.
    """
    cap = MAX_SHAPE_CLUSTER_SIZE.get(difficulty)
    if not cap or not obstacles:
        return obstacles

    while True:
        ordered = sorted(obstacles, key=lambda o: int(o.get("beat", 0)))
        clusters: list[list[int]] = []
        current: list[int] = []
        prev_beat = None
        for idx, o in enumerate(ordered):
            if o.get("kind") != "shape_gate":
                if current:
                    clusters.append(current)
                    current = []
                prev_beat = None
                continue
            beat = int(o.get("beat", 0))
            if prev_beat is None or (beat - prev_beat) >= SHAPE_CLUSTER_GAP_BEATS:
                if current:
                    clusters.append(current)
                current = [idx]
            else:
                current.append(idx)
            prev_beat = beat
        if current:
            clusters.append(current)

        oversized = [c for c in clusters if len(c) > cap]
        if not oversized:
            return ordered
        target = max(oversized, key=len)
        candidates = [i for i in target if not _is_protected_by_neighbor(ordered[i], ordered)]
        if not candidates:
            return ordered
        worst_idx = min(candidates, key=lambda i: float(ordered[i].get("flux", 0.0)))
        worst_obs = ordered[worst_idx]
        obstacles = [o for o in ordered if o is not worst_obs]


def _enforce_cluster_chain_cap_obstacles(obstacles, difficulty):
    """Issue #532 — break runs of consecutive shape_gate clusters that share
    the same first-shape and exceed ``SAME_SHAPE_CLUSTER_CHAIN_CAP``.

    Mirrors the cluster definition used by
    ``tools/validate_loop2_content_gates.py`` (clusters are pure
    density runs: ``beat_idx[i] - beat_idx[i-1] < SHAPE_CLUSTER_GAP``).
    Same-shape cluster-chain runs > cap are broken by re-shaping (lane
    + shape mutation) the first obstacle of the cluster sitting at
    position ``cap`` of the run so the chain breaks without dropping
    real onsets.  This preserves the IOI ramp invariant (issue #529)
    that aggressive obstacle dropping would violate.

    No obstacles are removed; no filler is inserted.
    """
    cap = SAME_SHAPE_CLUSTER_CHAIN_CAP.get(difficulty)
    if not cap or not obstacles:
        return obstacles

    # Bound the iteration count so a pathological input cannot loop forever.
    max_iters = 4 * len(obstacles) + 8
    for _ in range(max_iters):
        ordered = sorted(obstacles, key=lambda o: int(o.get("beat", 0)))
        clusters: list[list[int]] = []
        current: list[int] = []
        prev_beat = None
        for idx, o in enumerate(ordered):
            if o.get("kind") != "shape_gate":
                if current:
                    clusters.append(current)
                    current = []
                prev_beat = None
                continue
            beat = int(o.get("beat", 0))
            if prev_beat is None or (beat - prev_beat) >= SHAPE_CLUSTER_GAP_BEATS:
                if current:
                    clusters.append(current)
                current = [idx]
            else:
                current.append(idx)
            prev_beat = beat
        if current:
            clusters.append(current)

        run_start = 0
        run_shape = None
        violating: tuple[int, int] | None = None
        for ci, cluster in enumerate(clusters):
            shape = ordered[cluster[0]].get("shape")
            if shape == run_shape:
                if (ci - run_start + 1) > cap:
                    violating = (run_start, ci)
                    break
            else:
                run_shape = shape
                run_start = ci
        if violating is None:
            return ordered
        # Drop the weakest real onset in the cluster that caused the chain
        # to overrun. Post-hoc shape rewrites are forbidden because shape/lane
        # must keep matching the source broad onset layer.
        target_cluster_idx = run_start + cap
        target_cluster = clusters[target_cluster_idx]
        candidates = [i for i in target_cluster if not _is_protected_by_neighbor(ordered[i], ordered)]
        if not candidates:
            return ordered
        drop_idx = min(candidates, key=lambda i: (float(ordered[i].get("flux", 0.0)), -float(ordered[i].get("time_sec", 0.0))))
        del ordered[drop_idx]
        obstacles = ordered
    return sorted(obstacles, key=lambda o: int(o.get("beat", 0)))


def _ensure_obstacle_class_floor(obstacles, selected_events, onset_class, floor_pct, analysis):
    if not obstacles:
        return obstacles
    mapping = ONSET_CLASS_TO_OBSTACLE[onset_class]
    shape = mapping["shape"]
    used_sources = {obs.get("source_event_idx") for obs in obstacles}
    used_beats = {int(obs.get("beat", -1)) for obs in obstacles}
    beats_len = len(analysis.get("beats", [])) if isinstance(analysis, dict) else 0
    candidates = [
        ev for ev in selected_events.values()
        if ev.get("onset_class") == onset_class and ev.get("source_event_idx") not in used_sources
    ]
    candidates.sort(key=lambda ev: (-float(ev.get("flux", 0.0)), float(ev.get("t", 0.0))))
    for event in candidates:
        if sum(1 for obs in obstacles if obs.get("shape") == shape) / len(obstacles) >= floor_pct:
            break
        event_time = float(event.get("t", 0.0))
        if any(abs(event_time - float(obs.get("time_sec", 0.0))) < PLAYABILITY_MORPH_WINDOW_SEC for obs in obstacles):
            continue
        beat_idx = int(event.get("beat_idx", -1))
        limit = beats_len - 1 if beats_len else beat_idx + 64
        while beat_idx in used_beats and beat_idx <= limit:
            beat_idx += 1
        if beat_idx > limit:
            continue
        used_beats.add(beat_idx)
        obstacles.append({
            "beat": beat_idx,
            "kind": "shape_gate",
            "lane": mapping["lane"],
            "shape": shape,
            "onset_class": onset_class,
            "segment_idx": event.get("segment_idx"),
            "segment_focus": event.get("segment_focus"),
            "difficulty_inclusion": event.get("difficulty_inclusion"),
            "source_event_idx": event.get("source_event_idx"),
            "flux": float(event.get("flux", 0.0)),
            "time_sec": event_time,
        })
    while obstacles and sum(1 for obs in obstacles if obs.get("shape") == shape) / len(obstacles) < floor_pct:
        removable = [obs for obs in obstacles if obs.get("shape") != shape]
        if not removable:
            break
        weakest = min(removable, key=lambda obs: (float(obs.get("flux", 0.0)), -float(obs.get("time_sec", 0.0))))
        obstacles.remove(weakest)
    return sorted(obstacles, key=lambda obs: (int(obs.get("beat", -1)), float(obs.get("time_sec", 0.0))))


SAME_SHAPE_CLUSTER_CHAIN_CAP = {"medium": 3, "hard": 3}


def build_beatmap(
    analysis,
    difficulties,
    cleanup_enabled=True,
    experimental_onset_timing=True,
    *,
    allow_diagnostics_legacy_grid=False,
):
    """Build the full beatmap JSON."""
    if not experimental_onset_timing and not allow_diagnostics_legacy_grid:
        raise ValueError(
            "build_beatmap legacy beat-grid generation is diagnostics-only; "
            "pass allow_diagnostics_legacy_grid=True for explicit debug output."
        )

    beats = analysis["beats"]
    if not beats:
        raise ValueError("analysis.beats is required and must not be empty")

    def attach_and_validate_timing(obstacles, diff_name, selected_events):
        selected_events_by_source: dict[int, dict] = {}
        selected_events_by_beat: dict[int, list[dict]] = {}
        for event in selected_events.values():
            if not isinstance(event, dict):
                continue
            source_event_idx = event.get("source_event_idx")
            beat_idx = event.get("beat_idx")
            if isinstance(source_event_idx, int):
                selected_events_by_source[source_event_idx] = event
            if isinstance(beat_idx, int):
                selected_events_by_beat.setdefault(beat_idx, []).append(event)

        timed = []
        for obs in obstacles:
            beat_idx = obs.get("beat")
            if not isinstance(beat_idx, int):
                raise ValueError(f"{diff_name}: obstacle beat index must be int, got {beat_idx!r}")
            if beat_idx < 0 or beat_idx >= len(beats):
                raise ValueError(
                    f"{diff_name}: beat index {beat_idx} out of range for beats[{len(beats)}]"
                )

            beat_time = float(beats[beat_idx])
            timed_obs = dict(obs)
            if experimental_onset_timing:
                source_event_idx = obs.get("source_event_idx")
                event = selected_events_by_source.get(source_event_idx) if isinstance(source_event_idx, int) else None
                if event is None:
                    beat_events = selected_events_by_beat.get(beat_idx, [])
                    if len(beat_events) == 1:
                        event = beat_events[0]
                if isinstance(event, dict) and "t" in event:
                    onset_time = float(event["t"])
                    timed_obs["time_sec"] = round(onset_time, 6)
                    timed_obs["timing_source"] = "onset"
                    timed_obs["onset_time_sec"] = round(onset_time, 6)
                    timed_obs["subdivision_label"] = event.get("subdivision", "downbeat")
                else:
                    raise ValueError(
                        f"{diff_name}: segment-focus path requires source onset event for beat {beat_idx}"
                    )
                timed_obs["beat_time_sec"] = round(beat_time, 6)
            else:
                timed_obs["time_sec"] = round(beat_time, 6)
            timed.append(timed_obs)

        if not experimental_onset_timing:
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
    playability_collapsed_pairs_per_diff: dict[str, list] = {}
    for diff in difficulties:
        if experimental_onset_timing:
            # Active onset-only path: cleanup is disabled regardless of the
            # top-level cleanup_enabled flag (directive 2026-05-10).
            obs, selected_events, seg_diag = design_level_segment_focus(
                analysis,
                diff,
                cleanup_enabled=False,
            )
            playability_collapsed_pairs_per_diff[diff] = (
                seg_diag.get("playability_collapsed_pairs", []) if isinstance(seg_diag, dict) else []
            )
            # Issue #449 — medium-tier shape distribution rebalance.
            # The onset-only path preserves beat selection from segment_focus
            # but mutates only the rendered shape/lane (via set_shape_gate)
            # so the medium tier honors validate_medium_balance.py targets
            # (circle 10-20%, square 45-60%, triangle 25-45%). Mutations keep
            # onset_class coherent with the rendered shape/lane and preserve the
            # original broad layer as source_onset_class for diagnostics.
            if diff == "medium":
                obs = rebalance_medium_shapes(obs, diff)
            # #532 — obstacle-level safety net (post-rebalance).
            obs = _thin_oversized_clusters_obstacles(obs, diff)
            obs = _enforce_cluster_chain_cap_obstacles(obs, diff)
            if diff == "medium":
                # Thinning can drop medium back below its exact distribution
                # floor, so rebalance once more and re-apply the same safety
                # gates. This is not cleanup/filler: selection remains onset-only.
                obs = rebalance_medium_shapes(obs, diff)
                obs = _thin_oversized_clusters_obstacles(obs, diff)
                obs = _enforce_cluster_chain_cap_obstacles(obs, diff)
        else:
            selected_events, _ = select_beats(analysis, diff)
            obs = design_level(analysis, diff, cleanup_enabled=cleanup_enabled)
        timed = attach_and_validate_timing(obs, diff, selected_events)
        diff_data[diff] = {"beats": timed, "count": len(timed)}

    if experimental_onset_timing:
        # Issue #472 — preserve count(easy) <= count(medium) <= count(hard)
        # ramp invariant after the relaxed IOI promotion rules.  Trim the
        # higher tier to match the lower tier when ordering inverts; the
        # trim drops the sparsest promoted onsets only (no filler).
        # Issue #506 — the ramp may not widen any silent stretch past the
        # per-difficulty cap; bpm is forwarded via a sentinel key so the
        # trim can compute the cap in seconds and refuse violating drops.
        diff_data["__bpm__"] = float(analysis.get("bpm") or 0.0)
        diff_data["__duration_sec__"] = float(analysis.get("duration") or 0.0)
        diff_data = _enforce_difficulty_count_ramp(diff_data)
        for diff in difficulties:
            beats_for_diff = diff_data.get(diff, {}).get("beats", [])
            beats_for_diff = _thin_oversized_clusters_obstacles(beats_for_diff, diff)
            beats_for_diff = _enforce_cluster_chain_cap_obstacles(beats_for_diff, diff)
            diff_data[diff] = {"beats": beats_for_diff, "count": len(beats_for_diff)}
        diff_data = _enforce_difficulty_count_ramp(diff_data)
        diff_data.pop("__bpm__", None)
        diff_data.pop("__duration_sec__", None)

    # Issue #505 — emit ``offset`` anchored to the first authored beat,
    # matching ``tools/validate_beatmap_offset.py`` (and beat_map.h /
    # GitHub #137).  Runtime computes
    #     arrival_time = offset + beat_index * beat_period
    # so the offset must back-project the earliest authored beat to t≈0
    # rather than parking on ``beats[0]`` (the audio's first onset
    # detection, which is unrelated to the authored grid phase).
    bpm_value = float(analysis.get("bpm") or 0.0)
    offset_value = round(beats[0], 4) if beats else 0.0
    if beats and bpm_value > 0:
        beat_period = 60.0 / bpm_value
        authored_beat_indices = [
            int(o["beat"])
            for d in diff_data.values()
            for o in d.get("beats", [])
            if isinstance(o, dict) and isinstance(o.get("beat"), int)
        ]
        if authored_beat_indices:
            anchor_idx = min(authored_beat_indices)
            if 0 <= anchor_idx < len(beats):
                offset_value = round(
                    float(beats[anchor_idx]) - anchor_idx * beat_period,
                    4,
                )

    return {
        "song_id": analysis.get("title", "unknown"),
        "title": analysis.get("title", "unknown"),
        "bpm": analysis["bpm"],
        "offset": offset_value,
        "lead_beats": 4,
        "beat_times": [round(float(t), 6) for t in beats],
        "duration_sec": analysis.get("duration", 180),
        "difficulties": diff_data,
        "structure": analysis["structure"],
        # Issue #528 — surfaces cross-layer onsets the playability
        # collapse pass dropped per difficulty.  Empty lists when no
        # collapse occurred.  Raw instrument names are intentionally not
        # included; only broad-layer labels (percussive / harmonic /
        # full-spectrum) and flux/segment-focus fields appear.
        "playability_collapsed_pairs": playability_collapsed_pairs_per_diff if experimental_onset_timing else {},
    }


def _enforce_difficulty_count_ramp(diff_data):
    """Issue #472 — preserve obstacle-count monotonicity across difficulties.

    The relaxed easy/medium fallback-pool promotion in
    :func:`_enforce_median_ioi_target` can occasionally produce
    ``len(easy) > len(medium)`` or ``len(medium) > len(hard)`` on
    songs whose harder-tier IOI ceiling is already met by sparse
    focus-class events (so the fallback pool is never mined).  The
    runtime contract is that easier difficulties ship at most as many
    obstacles as harder ones; we restore that here by trimming the
    lower-tier set to match the next-higher-tier count.

    Trim strategy: drop obstacles whose **maximum** neighbour gap is
    largest (the sparsest outliers).  Removing dense events would
    raise the median IOI past the per-difficulty ceiling that
    :func:`_enforce_median_ioi_target` just achieved; removing sparse
    outliers tends to leave the dense body intact and preserves the
    median.  Onset-only is preserved — we only drop, never insert.

    Issue #506 — never drop an obstacle if doing so would widen its
    surrounding silent stretch beyond the per-difficulty silent-gap
    cap (``MAX_SILENT_GAP_BEATS``).  The gap-fill pass and the count
    ramp are both correctness gates; when they conflict on a song, the
    silent-gap cap wins (the runtime contract is "no monotony invariant
    is preserved at the cost of a 27 s void mid-song").
    """
    order = ("easy", "medium", "hard")
    available = [d for d in order if d in diff_data]
    bpm_value = float(diff_data.get("__bpm__") or 0.0)
    duration_value = diff_data.get("__duration_sec__")
    try:
        duration_value = float(duration_value) if duration_value is not None else None
    except (TypeError, ValueError):
        duration_value = None
    # Lead-in cap mirrors tools/validate_max_beat_gap.MAX_LEAD_IN_SEC (#527).
    LEAD_IN_CAP_SEC = {"easy": 8.0, "medium": 6.0, "hard": 4.0}
    TRAIL_OUT_CAP_SEC = {"easy": 12.0, "medium": 10.0, "hard": 8.0}
    for i in range(len(available) - 1, 0, -1):
        upper = diff_data[available[i]]["count"]
        lower_name = available[i - 1]
        lower = diff_data[lower_name]["beats"]
        # Issue #529 — strict ramp easy < medium < hard.  Target the
        # lower tier at ``upper - 1`` so equality cannot survive.
        target_count = max(0, upper - 1)
        if len(lower) <= target_count:
            continue
        sorted_by_time = sorted(lower, key=lambda o: float(o.get("time_sec", 0.0)))
        # Per-difficulty silent-gap cap (seconds).  ``bpm`` is read from
        # any obstacle's parent context via the diff_data structure;
        # we use the analysis BPM lifted into diff_data by the caller.
        cap_beats = MAX_SILENT_GAP_BEATS.get(lower_name)
        cap_sec = (
            cap_beats * 60.0 / bpm_value
            if cap_beats and bpm_value > 0
            else None
        )
        lead_cap = LEAD_IN_CAP_SEC.get(lower_name)
        trail_cap = TRAIL_OUT_CAP_SEC.get(lower_name)

        # Issue #506 + #527 — iterative trim that re-evaluates gaps after
        # every drop.  Picks the sparsest still-droppable obstacle whose
        # removal does not push the merged neighbour gap past ``cap_sec``,
        # and never drops a leading/trailing obstacle if doing so would
        # widen the lead-in / trail-out past the per-difficulty caps.
        # Boundary positions use real song bounds (t=0 for the lead-in,
        # ``duration_sec`` for the trail-out) instead of ±inf, which
        # previously caused the very first/last events to be picked first
        # as the "sparsest outliers" and silently strip the intro
        # (regression #527).
        kept_list = list(sorted_by_time)
        target_drops = len(kept_list) - target_count
        dropped = 0
        while dropped < target_drops and len(kept_list) > 1:
            best_pos = None
            best_score = None
            for pos in range(len(kept_list)):
                t_here = float(kept_list[pos].get("time_sec", 0.0))
                if pos > 0:
                    t_prev = float(kept_list[pos - 1].get("time_sec", 0.0))
                else:
                    t_prev = 0.0  # bound by song start, not -inf
                if pos < len(kept_list) - 1:
                    t_next = float(kept_list[pos + 1].get("time_sec", 0.0))
                elif duration_value is not None:
                    t_next = float(duration_value)  # bound by song end
                else:
                    t_next = t_here
                prev_dt = t_here - t_prev
                next_dt = t_next - t_here
                # When pos==0 the merged "gap" after removing it is the
                # new lead-in == next obstacle's t.  When pos==last it is
                # duration_sec - prev obstacle's t.
                if pos == 0:
                    merged = t_next  # new lead-in if dropped
                    if lead_cap is not None and merged > lead_cap:
                        continue
                elif pos == len(kept_list) - 1:
                    merged = (
                        (duration_value - t_prev)
                        if duration_value is not None else 0.0
                    )
                    if trail_cap is not None and merged > trail_cap:
                        continue
                else:
                    merged = t_next - t_prev
                if cap_sec is not None and merged > cap_sec:
                    continue
                score = max(prev_dt, next_dt)
                if best_score is None or score > best_score:
                    best_score = score
                    best_pos = pos
            if best_pos is None:
                break  # no cap-safe drop available
            kept_list.pop(best_pos)
            dropped += 1
        # Restore beat-ordinal order for downstream consumers.
        kept = sorted(kept_list, key=lambda o: int(o.get("beat", 0)))
        diff_data[lower_name] = {"beats": kept, "count": len(kept)}
    return diff_data


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
    parser.add_argument(
        "--experimental-onset-timing",
        action="store_true",
        help="Compatibility flag; onset event timing is the default authoring path.",
    )
    parser.add_argument(
        "--legacy-beat-grid",
        action="store_true",
        help="Use legacy beat-grid timing for diagnostics only.",
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
    if args.legacy_beat_grid and not args.diagnostics_only:
        print(
            "Error: --legacy-beat-grid is diagnostics-only; beatmap generation is onset-only.",
            file=sys.stderr,
        )
        sys.exit(1)

    if args.diagnostics_out:
        out_dir = write_snap_diagnostics(
            analysis,
            args.diagnostics_out,
            experimental_onset_timing=not args.legacy_beat_grid,
            prefer_shipped_beatmap=args.diagnostics_only,
        )
        print(f"✓ diagnostics: {out_dir}")
        if args.diagnostics_only:
            return

    diffs = ["easy","medium","hard"] if args.difficulty == "all" else [args.difficulty]

    print("=" * 60)
    print(f"  LEVEL DESIGNER | {analysis.get('title','?')} | BPM {analysis['bpm']}")
    print("=" * 60)

    beatmap = build_beatmap(
        analysis,
        diffs,
        cleanup_enabled=not args.no_cleanup,
        experimental_onset_timing=not args.legacy_beat_grid,
    )

    for diff in diffs:
        print_stats(beatmap["difficulties"][diff]["beats"], diff)

    out_path = args.output or f"{analysis.get('title','beatmap')}_beatmap.json"
    with open(out_path, "w") as f:
        json.dump(beatmap, f, indent=2)

    print(f"\n✓ {out_path}")
    print("=" * 60)


if __name__ == "__main__":
    main()
