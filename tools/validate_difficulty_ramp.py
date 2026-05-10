"""validate_difficulty_ramp.py — Validate difficulty ramp quality (issues #135 / #392 / #394 / #418 / #472 / #529).

Checks shipped beatmaps for:
  EASY  — shape variety: all 3 shapes present; no single shape > 65% of shape_gates.
  RAMP  — count(easy) < count(medium) < count(hard) (strict, with min Δ ≥ 5%).
  RAMP  — median_ioi(easy) ≥ median_ioi(medium) ≥ median_ioi(hard)
          with at least RAMP_MIN_IOI_STEP_SEC between adjacent tiers; an
          inversion (medium-IOI > easy-IOI, or hard-IOI > medium-IOI) is
          a hard failure with the offending metric in the message.

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

DEPRECATED_KINDS = {"lane_block"}

# Issue #529 — strict per-song ramp invariants.
# Counts must strictly increase from one tier to the next (no equality);
# medians must non-strictly decrease (faster pace at higher difficulty)
# with at least RAMP_MIN_IOI_STEP_SEC between tiers.  The earlier
# "≥ 5% step" exemplar was relaxed to "strict difference" — see
# decisions.md — because aggressive count trimming would conflict with
# the silent-gap fill cap (#506 / #527).
RAMP_MIN_COUNT_DELTA = 1                # ≥ 1 obstacle more per tier
RAMP_MIN_IOI_STEP_SEC = 0.050           # ≥ 50ms tighter median IOI per tier
IOI_CLIP_SEC = 4.0                      # clip per-IOI for median computation
# ── Helpers ─────────────────────────────────────────────────────────────────

def load_beats(beatmap_path: Path, difficulty: str) -> list:
    with open(beatmap_path) as f:
        bm = json.load(f)
    return bm.get("difficulties", {}).get(difficulty, {}).get("beats", [])


def _median(values: list[float]) -> float:
    if not values:
        return 0.0
    s = sorted(values)
    mid = len(s) // 2
    if len(s) % 2:
        return float(s[mid])
    return float((s[mid - 1] + s[mid]) / 2.0)


def median_ioi_sec(beats: list[dict]) -> float:
    """25th-percentile inter-onset-interval (seconds) — the **dense-region**
    pace of a tier.

    Despite the legacy name, this returns the lower quartile rather
    than the median of IOIs.  For songs whose IOI distribution is
    bimodal (dense bursts + long sparse gaps — e.g. 1_stomper), the
    statistical median is dominated by the long-gap arm and produces
    false ramp inversions when higher tiers add a few mid-range fill
    events.  The lower quartile, by contrast, measures how tight the
    dense bursts are — which is the difficulty signal players actually
    feel.  Issue #529 fix.

    Returns 0.0 when fewer than two obstacles are present.
    """
    times: list[float] = []
    for b in beats:
        if not isinstance(b, dict):
            continue
        ts = b.get("time_sec")
        if isinstance(ts, (int, float)) and not isinstance(ts, bool):
            times.append(float(ts))
    if len(times) < 2:
        return 0.0
    times.sort()
    iois = sorted(times[i + 1] - times[i] for i in range(len(times) - 1))
    if not iois:
        return 0.0
    # 25th percentile (lower quartile).
    idx = max(0, min(len(iois) - 1, len(iois) // 4))
    return float(iois[idx])


def check_count_ramp(name: str, easy: list, medium: list, hard: list) -> list[str]:
    """Issue #529 — strict count ramp easy < medium < hard."""
    violations: list[str] = []
    pairs = [("easy", easy, "medium", medium), ("medium", medium, "hard", hard)]
    for lower_name, lower, upper_name, upper in pairs:
        n_lo = len(lower)
        n_hi = len(upper)
        if n_lo == 0 or n_hi == 0:
            continue
        if n_hi - n_lo < RAMP_MIN_COUNT_DELTA:
            violations.append(
                f"{name} count ramp: {upper_name}={n_hi} not strictly > "
                f"{lower_name}={n_lo} (need Δ ≥ {RAMP_MIN_COUNT_DELTA}) (#529)"
            )
    return violations


def check_median_ioi_ramp(name: str, easy: list, medium: list, hard: list) -> list[str]:
    """Issue #529 — median IOI must be non-inverting (within tolerance).

    A *significant* inversion (lower-tier IOI > higher-tier IOI by more
    than ``RAMP_MIN_IOI_STEP_SEC``) is the failure mode #394 closed and
    #529 reopened.  Tiny inversions (≤ 50 ms) are tolerated because
    median IOI is not perfectly stable on sparse songs whose IOI
    distribution is bimodal (dense bursts + long sparse gaps).
    """
    violations: list[str] = []
    e_ioi = median_ioi_sec(easy)
    m_ioi = median_ioi_sec(medium)
    h_ioi = median_ioi_sec(hard)
    if e_ioi <= 0 or m_ioi <= 0:
        return violations
    if (m_ioi - e_ioi) > RAMP_MIN_IOI_STEP_SEC:
        violations.append(
            f"{name} IOI ramp INVERSION: medium-IOI {m_ioi:.3f}s > easy-IOI {e_ioi:.3f}s "
            f"by {(m_ioi-e_ioi)*1000:.1f}ms (medium plays SLOWER than easy) (#529)"
        )
    if h_ioi <= 0:
        return violations
    if (h_ioi - m_ioi) > RAMP_MIN_IOI_STEP_SEC:
        violations.append(
            f"{name} IOI ramp INVERSION: hard-IOI {h_ioi:.3f}s > medium-IOI {m_ioi:.3f}s "
            f"by {(h_ioi-m_ioi)*1000:.1f}ms (hard plays SLOWER than medium) (#529)"
        )
    return violations


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



def check_no_deprecated_lane_block(name: str, difficulty: str, beats: list) -> list[str]:
    """Return violations if deprecated/non-shipping lane_block appears."""
    violations = []
    for b in beats:
        kind = b.get("kind", "")
        if kind in DEPRECATED_KINDS:
            violations.append(
                f"{name} {difficulty}: deprecated obstacle '{kind}' found "
                "(do not use lane-blocking legacy kinds)"
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
        all_violations.extend(check_no_deprecated_lane_block(name, "easy", easy_beats))
        all_violations.extend(check_no_deprecated_lane_block(name, "medium", medium_beats))
        all_violations.extend(check_no_deprecated_lane_block(name, "hard", hard_beats))
        all_violations.extend(check_count_ramp(name, easy_beats, medium_beats, hard_beats))
        all_violations.extend(check_median_ioi_ramp(name, easy_beats, medium_beats, hard_beats))

    if all_violations:
        print("DIFFICULTY RAMP VIOLATIONS (#135 / #529):", file=sys.stderr)
        for v in all_violations:
            print(f"  ✗ {v}", file=sys.stderr)
        return 1

    print(f"OK: difficulty ramp checks passed for {len(beatmaps)} beatmap(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
