"""validate_difficulty_ramp.py — Validate difficulty ramp quality for issue #135.

Checks shipped beatmaps for:
  EASY  — shape variety: all 3 shapes present; no single shape > 65% of shape_gates.
  ALL   — removed/non-shipping push-lane kinds must never appear.

Exit codes:
  0 — all checks pass
  1 — one or more violations found

Run after any beatmap regeneration:
    python3 tools/validate_difficulty_ramp.py
"""

import json
import sys
from collections import Counter
from pathlib import Path

BEATMAP_DIR = Path(__file__).parent.parent / "content" / "beatmaps"

# ── Thresholds ─────────────────────────────────────────────────────────────

EASY_MAX_DOMINANT_SHAPE_PCT = 65   # no single shape > this % of all shape_gates
EASY_MIN_DISTINCT_SHAPES    = 3    # all three shapes must appear at least once

REMOVED_PUSH_KINDS = {"lane_push_left", "lane_push_right"}
DEPRECATED_KINDS = {"lane_block"}
# ── Helpers ─────────────────────────────────────────────────────────────────

def load_beats(beatmap_path: Path, difficulty: str) -> list:
    with open(beatmap_path) as f:
        bm = json.load(f)
    return bm.get("difficulties", {}).get(difficulty, {}).get("beats", [])


def check_easy_shape_gate_only(name: str, beats: list) -> list[str]:
    """Return violations if easy has any non-shape obstacle kind (#125 contract)."""
    violations = []
    for b in beats:
        kind = b.get("kind", "")
        if kind != "shape_gate":
            violations.append(
                f"{name} easy: forbidden obstacle '{kind}' found "
                f"(easy must be shape_gate only per #125)"
            )
    return violations


def check_easy_variety(name: str, beats: list) -> list[str]:
    """Return list of violation strings (empty = pass)."""
    violations = []
    if not beats:
        violations.append(f"{name} easy: no beats found")
        return violations

    shapes = [b.get("shape", "") for b in beats if b.get("kind") == "shape_gate"]
    if not shapes:
        violations.append(f"{name} easy: no shape_gate obstacles found")
        return violations

    counts = Counter(shapes)
    distinct = len([s for s in counts if s])
    if distinct < EASY_MIN_DISTINCT_SHAPES:
        violations.append(
            f"{name} easy: only {distinct} distinct shape(s) used "
            f"(need ≥{EASY_MIN_DISTINCT_SHAPES}); counts={dict(counts)}"
        )

    total = len(shapes)
    for shape, count in counts.items():
        pct = 100 * count / total
        if pct > EASY_MAX_DOMINANT_SHAPE_PCT:
            violations.append(
                f"{name} easy: '{shape}' dominates at {pct:.1f}% "
                f"(limit {EASY_MAX_DOMINANT_SHAPE_PCT}%); total shape_gates={total}"
            )

    return violations


def check_no_removed_push_kinds(name: str, difficulty: str, beats: list) -> list[str]:
    """Return violations if removed/non-shipping push kinds appear."""
    violations = []
    for b in beats:
        kind = b.get("kind", "")
        if kind in REMOVED_PUSH_KINDS:
            violations.append(
                f"{name} {difficulty}: removed obstacle '{kind}' found"
            )
    return violations


def check_no_deprecated_lane_block(name: str, difficulty: str, beats: list) -> list[str]:
    """Return violations if deprecated/non-shipping lane_block appears."""
    violations = []
    for b in beats:
        kind = b.get("kind", "")
        if kind in DEPRECATED_KINDS:
            violations.append(
                f"{name} {difficulty}: deprecated obstacle '{kind}' found "
                "(do not use lane-blocking/push-lane legacy kinds)"
            )
    return violations


# ── Main ─────────────────────────────────────────────────────────────────────

def main() -> int:
    beatmaps = sorted(BEATMAP_DIR.glob("*_beatmap.json"))
    if not beatmaps:
        print(f"ERROR: no beatmaps found in {BEATMAP_DIR}", file=sys.stderr)
        return 1

    all_violations: list[str] = []

    for bm_path in beatmaps:
        name = bm_path.stem.replace("_beatmap", "")

        easy_beats   = load_beats(bm_path, "easy")
        medium_beats = load_beats(bm_path, "medium")
        hard_beats   = load_beats(bm_path, "hard")

        all_violations.extend(check_easy_shape_gate_only(name, easy_beats))
        all_violations.extend(check_easy_variety(name, easy_beats))
        all_violations.extend(check_no_removed_push_kinds(name, "easy", easy_beats))
        all_violations.extend(check_no_removed_push_kinds(name, "medium", medium_beats))
        all_violations.extend(check_no_removed_push_kinds(name, "hard", hard_beats))
        all_violations.extend(check_no_deprecated_lane_block(name, "easy", easy_beats))
        all_violations.extend(check_no_deprecated_lane_block(name, "medium", medium_beats))
        all_violations.extend(check_no_deprecated_lane_block(name, "hard", hard_beats))

    if all_violations:
        print("DIFFICULTY RAMP VIOLATIONS (#135):", file=sys.stderr)
        for v in all_violations:
            print(f"  ✗ {v}", file=sys.stderr)
        return 1

    print(f"OK: difficulty ramp checks passed for {len(beatmaps)} beatmap(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
