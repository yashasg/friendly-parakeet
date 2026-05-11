#!/usr/bin/env python3
"""Validate that no protected onset cluster contains conflicting required gates.

Two required ``shape_gate`` obstacles inside the protected 50 ms public-layer
window with different ``(lane, shape)`` are physically unsatisfiable: the
player can occupy only one lane/shape at an instant. Distinct broad-layer
onsets must remain in the beatmap, but only one row in a protected group may
remain a required action; sibling public-layer rows should be non-blocking
``onset_marker`` entries that preserve their onset metadata.

The check is "representation-side de-conflict only" — no beat fallback, no
synthetic filler. Existing distinct cross-layer onsets remain in the beatmap;
the validator only fails when more than one independent required action is
authored in the same protected group.

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

PROTECTED_CROSS_LAYER_SEC = 0.050
REQUIRED_ACTION_KINDS = {"shape_gate", "combo_gate", "split_path"}


def required_action_key(beat: dict) -> tuple[object, object] | None:
    kind = beat.get("kind", "shape_gate")
    if kind not in REQUIRED_ACTION_KINDS:
        return None
    if kind == "combo_gate":
        return (tuple(beat.get("blocked", ())), beat.get("shape"))
    return (beat.get("lane"), beat.get("shape"))


def find_required_action_groups(
    beats: list[dict],
    threshold_sec: float = PROTECTED_CROSS_LAYER_SEC,
) -> list[list[dict]]:
    """Return protected-time groups with conflicting required action keys."""
    rows = sorted(
        (b for b in beats
         if isinstance(b, dict)
         and required_action_key(b) is not None
         and isinstance(b.get("time_sec"), (int, float))
         and not isinstance(b.get("time_sec"), bool)),
        key=lambda b: float(b["time_sec"]),
    )
    groups: list[list[dict]] = []
    current: list[dict] = []
    for row in rows:
        if not current or float(row["time_sec"]) - float(current[-1]["time_sec"]) <= threshold_sec:
            current.append(row)
        else:
            if len({required_action_key(item) for item in current}) > 1:
                groups.append(current)
            current = [row]
    if len({required_action_key(item) for item in current}) > 1:
        groups.append(current)
    return groups


def find_simultaneous_shape_gate_pairs(
    beats: list[dict],
    threshold_sec: float = PROTECTED_CROSS_LAYER_SEC,
) -> list[tuple[dict, dict, float]]:
    """Compatibility helper: expand conflicting protected groups into pairs."""
    pairs: list[tuple[dict, dict, float]] = []
    for group in find_required_action_groups(beats, threshold_sec):
        for i, a in enumerate(group):
            for b in group[i + 1:]:
                if required_action_key(a) != required_action_key(b):
                    pairs.append((a, b, float(b["time_sec"]) - float(a["time_sec"])))
    return pairs


def validate_beatmap(path: Path) -> list[str]:
    beatmap = json.loads(path.read_text())
    name = path.stem.replace("_beatmap", "")
    findings: list[str] = []
    for difficulty, payload in beatmap.get("difficulties", {}).items():
        beats = payload.get("beats", []) or []
        groups = find_required_action_groups(beats)
        if not groups:
            continue
        # Always show first 3 examples for actionable diagnostics.
        sample = groups[:3]
        for group in sample:
            start = float(group[0]["time_sec"])
            end = float(group[-1]["time_sec"])
            keys = ", ".join(
                f"(lane={beat.get('lane')},shape={beat.get('shape')},kind={beat.get('kind', 'shape_gate')},class={beat.get('onset_class')})"
                for beat in group
            )
            findings.append(
                f"{name} [{difficulty}]: protected group Δt={(end - start)*1000:.1f}ms "
                f"has conflicting required actions at t≈{start:.3f}s: {keys} (#642)"
            )
        if len(groups) > len(sample):
            findings.append(
                f"{name} [{difficulty}]: total {len(groups)} protected group(s) "
                f"with conflicting required actions within {PROTECTED_CROSS_LAYER_SEC*1000:.0f}ms (#642)"
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
        print("PROTECTED REQUIRED-ACTION VIOLATIONS (#642):", file=sys.stderr)
        for finding in all_findings:
            print(f"  x {finding}", file=sys.stderr)
        return 1

    print(f"OK: no protected required-action conflicts in {len(paths)} beatmap(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
