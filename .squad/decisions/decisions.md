# Team Decisions

---

## Issue #137 — Offset Semantics & Off-Beat Collision Fix

### Decision: BeatMap.offset Anchored to First Authored Beat

**Owner:** Fenster (Scheduler Implementation)  
**Date:** 2026-04-27  
**Status:** ✅ APPROVED  
**Related Decisions:** Baer (offset validation), Rabin (content timing), Kujan (review gate)

### Problem

`level_designer.py::build_beatmap()` previously wrote `"offset": round(beats[0], 3)`, anchoring the entire beat grid to the first aubio-detected beat. If that detection was a false positive (noise/transient before the true musical downbeat), every obstacle collision shifted by the full error—up to one beat period (e.g., 377ms at 159 BPM). That is fully perceptible and breaks music sync.

### Decision

`BeatMap.offset` is now explicitly defined as **the audio time (seconds from audio start) of beat_index=0 in the uniform beat grid**. The pipeline computes it anchored to the first authored beat rather than blindly to `beats[0]` from the detector.

**Formula:**
```
offset = beats[anchor_idx] - anchor_idx * (60.0 / bpm)
```
where `anchor_idx = min(all authored beat_index values across all difficulties)`.

### What Changed

| File | Change |
|---|---|
| `tools/level_designer.py` | `build_beatmap()` now computes offset from `anchor_idx` instead of `beats[0]` |
| `app/components/beat_map.h` | Added doc comment to `offset` field explaining the uniform-grid semantics |
| `app/systems/beat_scheduler_system.cpp` | Added comment referencing offset semantics at the arrival_time computation site |
| `tools/validate_beatmap_offset.py` | **New** — authoring-time validation tool; exits 0 iff all shipped beatmaps have correctly anchored offsets (tolerance: 2ms) |
| `content/beatmaps/*.json` | All 3 shipped beatmaps regenerated; stomper offset changed by 1ms, drama/mental unchanged |

### Impact Assessment

- **Gameplay:** < 1ms change for all 3 current songs. No perceptible difference.
- **Future songs:** Robust — a false early beat detection no longer corrupts the entire grid.
- **Runtime code:** Unchanged. No C++ changes to the scheduler formula or data structures.
- **Content gates:** All pass — #125 LowBar/HighBar coverage, #134 shape_change_gap, #135 difficulty ramp.
- **Test count:** 2392 assertions, 757 test cases, all pass.

### Rejected Alternatives

1. **Keep `beats[0]`** — works for current songs but is fragile against false early detections.
2. **Per-beat timestamps at runtime** — would fix cumulative tempo drift too, but requires a large structural change (float array in BeatEntry, significant test churn). Deferred; the current fixed-BPM model has < 1ms error across 563 beats.
3. **Linear regression anchor** — most robust statistically, but changes both `offset` AND `bpm`, making it a larger change with more regression risk. Not needed given current song quality.

---

### Decision: Offset Semantics Validation (Test Engineering)

**Owner:** Baer (Test Engineer)  
**Status:** ✅ APPROVED  
**Related:** Rabin (content perspective), Fenster (scheduler semantics owner)

### Rationale

Add deterministic regression tests that pin the C++ scheduler's offset contract and will catch any future pipeline change that decouples `offset` from `beat_index=0` timing. Tests encode the runtime invariant:

> `arrival_time = offset + beat_index * beat_period` must hold for every scheduled obstacle, and changing `offset` by Δ must shift ALL collision times by exactly Δ.

The risk is a future pipeline or loader change that changes `offset` without updating beat_index values (or vice versa).

### Deliverables

- `tests/test_beat_scheduler_offset.cpp` — 9 C++ Catch2 tests (`[beat_scheduler][offset][issue137]`)
  - Offset invariant under difficulty selection
  - Offset shift propagates uniformly across all beats
  - Loader rejects out-of-range beat indices
  - Jitter detection (tempo variation test)
  
- `tools/validate_offset_semantics.py` — 7 deterministic Python suites
  - No audio required; validation via formula consistency
  - Jitter meta-test certifies detection logic

### CI Commands

```bash
# Authoritative C++ gate
./build/shapeshifter_tests "[offset]"

# Python convenience (dev-time + CI optional)
python3 tools/validate_offset_semantics.py

# Full content gate (all existing issues)
./build/shapeshifter_tests "[shipped_beatmaps][difficulty_ramp][low_high_bar][offset]"
```

---

### Decision: Shipped Beatmap Offsets are Timing-Correct; Content Regeneration Deferred

**Owner:** Rabin (Level Design)  
**Date:** 2026-04-27  
**Status:** ✅ APPROVED — coordinate with Fenster (semantics owner)  
**Scope:** Content/timing validation. Schema/scheduler semantics Fenster's call.

### Position

Under the current scheduler formula
```
beat_time = song.offset + beat_index * (60 / bpm)
```
(`app/systems/beat_scheduler_system.cpp:27`), all 3 shipped beatmaps produce obstacle collision times that match the analyzed onsets in `content/beatmaps/*_analysis.json` to within **±1 ms** for every scheduled obstacle across easy/medium/hard. Aubio's beat grid is essentially uniform, so `offset = beats[0]` faithfully anchors the uniform-period model the runtime uses.

There is no off-beat collision present in shipped data today.

### What #137 Exposes

The bug is **semantic**, not numeric: `offset` is not currently *named or documented* as "first audible beat onset." Two valid interpretations exist:

1. `offset` = analysis `beats[0]` (current code, current data) — anchors the uniform period to the first detected onset.
2. `offset` = time of the first authored collision; beat indices become relative to that.

Either is workable; the runtime just needs the chosen one to match the producer (level_designer.py).

### Decision

- **No content regeneration is required to ship the test-flight build as-is.**
- If Fenster keeps semantics #1: shipped beatmaps need zero changes.
- If Fenster moves to semantics #2 (or any rebasing of beat indices): Rabin will regenerate all 3 beatmaps from existing `*_analysis.json` (no `rhythm_pipeline.py` rerun needed) and re-validate:
  - `tools/check_shape_change_gap.py` (#134)
  - `tools/check_bar_coverage.py` (#125)
  - `tools/validate_difficulty_ramp.py` (#135)
  - `./build/shapeshifter_tests` (authoritative C++ gate)

### Hand-Off

Fenster: pick semantics and document it in the beatmap schema/loader docstring. Ping Rabin when `level_designer.py` / `beat_map_loader.cpp` changes land; Rabin will regen + validate within the same PR window.

---

### Decision: Issue #137 Review — APPROVED

**Date:** 2026-04-27  
**Reviewer:** Kujan  
**Verdict:** ✅ APPROVED

### Summary

Issue #137 is resolved. Offset semantics are now explicitly defined, correctly computed, runtime-consistent, and regression-protected.

### Evidence

- **Semantics defined:** `beat_map.h` documents `offset` = audio time of `beat_index=0`; scheduler formula `arrival_time = offset + beat_index * beat_period` commented in `beat_scheduler_system.cpp`.
- **Anchor fix verified:** `build_beatmap()` computes `offset = beats[anchor_idx] - anchor_idx * beat_period` where `anchor_idx = min(authored beat indices across all difficulties)`. Blind `beats[0]` usage eliminated.
- **Content gates pass:** `validate_beatmap_offset.py` exits 0 (all 3 beatmaps within 2ms tolerance); `validate_offset_semantics.py` exits 0 (7 suites); `validate_beatmap_bars.py` (#125) exits 0; `validate_difficulty_ramp.py` (#135) exits 0.
- **C++ tests pass:** 9 `[beat_scheduler][offset][issue137]` tests — all 2392 assertions pass (757 test cases).
- **Content delta minimal:** Stomper offset 2.270→2.269s (1ms); drama/mental unchanged.

### Non-blocking Notes

None.

---

## 2026-04-28 — Engineering Model Override & Keaton UI Fix

### Decision: Switch Engineering to gpt-5.3-codex

**Initiator:** User (yashasg)  
**Date:** 2026-04-28T23:30:11.427-07:00  
**Status:** ✅ EXECUTED  

### Directive

User requested engineering members switch from default `claude-sonnet-4.6` to `gpt-5.3-codex` for specialized code generation capabilities.

### Resolution

**Applied to:** Keyser, McManus, Fenster, Hockney, Verbal, Keaton, Kobayashi, Baer  
**Preserved:** Edie, Redfoot, Saul, Rabin remain on `claude-opus-4.7`; Scribe and Ralph on `claude-haiku-4.5`  
**File:** `.squad/config.json` — `agentModelOverrides` updated and validated  
**Effective:** Immediate — all subsequent engineering work uses gpt-5.3-codex

---

## Keaton UI Regression Fix Completion

### Decision: Screen-Controller Runtime Overrides Restore Visual Consistency

**Owner:** Keaton (Screen Controller Implementation)  
**Approver:** Kujan (Integration Review)  
**Date:** 2026-04-29  
**Status:** ✅ APPROVED  

### Problem

UI elements (title text, level select cards, difficulty buttons, gameplay HUD) were visually inconsistent after rguilayout integration. Root cause: runtime rendering overrides needed to ensure screen-controller state machine properly manages visual state across screen transitions.

### Resolution

Implemented screen-controller runtime overrides that:
- Restore title text rendering with consistent positioning and styling
- Restore level select card layout and interactive difficulty buttons
- Restore gameplay HUD state machine with proper component visibility management

### Evidence

**Test Coverage:** 2595 passing assertions across UI regression suites  
**Validation:** All visual elements verified across title, level select, and gameplay screens  
**Approval:** Kujan signed off after staging unblock resolved tracking issue

### Impact

Visual consistency fully restored. UI no longer exhibits regression from rguilayout integration. Ready for merge.
