#!/usr/bin/env python3
"""Validate that no unprotected ``shape_gate`` obstacles in the same difficulty
are authored within ``MORPH_DURATION`` (120 ms) of each other unless they share
``(lane, shape)`` (issue #528).

Two ``shape_gate`` obstacles at near-zero ``Δt`` with different
``(lane, shape)`` are physically unsatisfiable: the player can occupy only
one lane and morphing between shapes takes 120 ms (see
``design-docs/rhythm-spec.md`` ``MORPH_DURATION``).  Distinct broad-layer
onsets within the protected 50 ms window are preserved as authored obstacles;
outside that protected window, obstacles still need a hard playability minimum
at the ``(lane, shape)`` level.

The check is "selection-side de-conflict only" — no beat fallback, no
synthetic filler.  Existing distinct cross-layer onsets that survive the
collapse pass remain in the beatmap; only one obstacle is emitted per
near-simultaneous distinct ``(lane, shape)`` pair.

Exit codes:
  0 — all beatmaps pass
  1 — one or more pairs found

Usage:
    python3 tools/validate_no_simultaneous_shape_gates.py
    python3 tools/validate_no_simultaneous_shape_gates.py path/to/beatmap.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO_ROOT / "content" / "beatmaps"

# rhythm-spec.md MORPH_DURATION = 120 ms.  Pairs at exactly the same instant
# are also caught (Δt = 0 ≤ 0.120).
MORPH_DURATION_SEC = 0.120
PROTECTED_CROSS_LAYER_SEC = 0.050
PUBLIC_ONSET_CLASSES = {"percussive", "harmonic", "full-spectrum"}


def is_protected_cross_layer_pair(a: dict, b: dict, dt: float) -> bool:
    a_class = a.get("onset_class")
    b_class = b.get("onset_class")
    return (
        isinstance(a_class, str)
        and isinstance(b_class, str)
        and a_class in PUBLIC_ONSET_CLASSES
        and b_class in PUBLIC_ONSET_CLASSES
        and a_class != b_class
        and dt <= PROTECTED_CROSS_LAYER_SEC
    )


def find_simultaneous_shape_gate_pairs(
    beats: list[dict],
    threshold_sec: float = MORPH_DURATION_SEC,
) -> list[tuple[dict, dict, float]]:
    """Return every ``(left, right, dt)`` pair of ``shape_gate`` obstacles
    within ``threshold_sec`` whose ``(lane, shape)`` differ.
    """
    rows = sorted(
        (b for b in beats
         if isinstance(b, dict)
         and b.get("kind") == "shape_gate"
         and isinstance(b.get("time_sec"), (int, float))
         and not isinstance(b.get("time_sec"), bool)),
        key=lambda b: float(b["time_sec"]),
    )
    pairs: list[tuple[dict, dict, float]] = []
    n = len(rows)
    for i in range(n):
        a = rows[i]
        a_t = float(a["time_sec"])
        a_key = (a.get("lane"), a.get("shape"))
        for j in range(i + 1, n):
            b = rows[j]
            b_t = float(b["time_sec"])
            dt = b_t - a_t
            if dt > threshold_sec:
                break
            if (b.get("lane"), b.get("shape")) != a_key and not is_protected_cross_layer_pair(a, b, dt):
                pairs.append((a, b, dt))
    return pairs


def validate_beatmap(path: Path) -> list[str]:
    beatmap = json.loads(path.read_text())
    name = path.stem.replace("_beatmap", "")
    findings: list[str] = []
    for difficulty, payload in beatmap.get("difficulties", {}).items():
        beats = payload.get("beats", []) or []
        pairs = find_simultaneous_shape_gate_pairs(beats)
        if not pairs:
            continue
        # Always show first 3 examples for actionable diagnostics.
        sample = pairs[:3]
        for a, b, dt in sample:
            findings.append(
                f"{name} [{difficulty}]: shape_gate pair Δt={dt*1000:.1f}ms "
                f"(lane={a.get('lane')},shape={a.get('shape')}) vs "
                f"(lane={b.get('lane')},shape={b.get('shape')}) at t≈{float(a['time_sec']):.3f}s (#528)"
            )
        if len(pairs) > len(sample):
            findings.append(
                f"{name} [{difficulty}]: total {len(pairs)} unplayable shape_gate "
                f"pair(s) within {MORPH_DURATION_SEC*1000:.0f}ms (#528)"
            )
    return findings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", help="Optional *_beatmap.json paths")
    args = parser.parse_args(argv)

    paths = [Path(raw) for raw in args.files] if args.files else sorted(DEFAULT_DIR.glob("*_beatmap.json"))
    if not paths:
        print(f"ERROR: no beatmaps found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    all_findings: list[str] = []
    for path in paths:
        all_findings.extend(validate_beatmap(path))

    if all_findings:
        print("SIMULTANEOUS SHAPE_GATE VIOLATIONS (#528):", file=sys.stderr)
        for finding in all_findings:
            print(f"  x {finding}", file=sys.stderr)
        return 1

    print(f"OK: no near-simultaneous shape_gate pairs in {len(paths)} beatmap(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
