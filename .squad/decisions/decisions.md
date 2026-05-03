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

---

## 2026-05-03 — Ralph Round 3: motion_system Extraction & scoring_system SOLID Audit

### Decision: motion_system Extraction (Keaton Implementation)

**Owner:** Keaton  
**Date:** 2026-05-03  
**Status:** ✅ APPROVED  
**Trigger:** Ralph Round 3 (perf + SOLID continuous loop)

### Problem

Per Keyser's Round 2 SOLID audit recommendation, `scroll_system` mixed two concerns:
- Rhythm obstacle scrolling (model_beat_view, beat_view)
- General entity motion (vel_view + motion_view without ObstacleTag constraint)

The `vel_view` (Position+Velocity) and `motion_view` (WorldTransform+MotionVelocity) loops were independent of rhythm logic and should be owned by a dedicated system.

### Decision

Extract `vel_view` and `motion_view` into a new `motion_system`, narrowing `scroll_system` to obstacle-only concerns.

### What Shipped

Extracted `vel_view` (Position+Velocity) and `motion_view` (WorldTransform+MotionVelocity) loops from `scroll_system` into a new `motion_system`, per Keyser's R2 SOLID audit recommendation.

#### Files Added
- `app/systems/motion_system.cpp` — new system with phase guard matching scroll_system's

#### Files Modified
- `app/systems/scroll_system.cpp` — stripped to obstacle-only loops (3 views: model_beat_view, beat_view, model_view, all ObstacleTag-bearing)
- `app/systems/all_systems.h` — added `void motion_system(entt::registry& reg, float dt)` declaration under Phase 5
- `app/game_loop.cpp` — added `motion_system(reg, dt)` immediately after `scroll_system(reg, dt)`
- `tests/test_world_systems.cpp` — renamed 3 "scroll:" test cases to "motion:" and updated calls to `motion_system`; added "motion: no movement when not in Playing phase" phase guard test
- `tests/test_scroll_rhythm.cpp` — renamed "scroll: non-rhythm entities use dt-based movement even in rhythm mode" to "motion: non-rhythm entities use dt-based movement"; updated call to `motion_system`
- `benchmarks/bench_systems.cpp` — added "Bench: motion_system" (10/100/1000 entity tiers); added `motion_system(reg, DT)` to full-frame typical + stress benches

### Build Result

Zero warnings. `-Wall -Wextra -Werror` clang. CMake reconfigured cleanly (new file picked up via GLOB CONFIGURE_DEPENDS).

### Test Result

**2211 assertions in 772 test cases — all passed.**  
(Up from 2209/771 in R2; the +2 assertions are the new motion phase-guard test.)

### Bench Delta

The `Bench: scroll_system` fixture uses `spawn_obstacles` which creates `ObstacleTag + Position + Velocity` (no `ObstacleScrollZ`, no `BeatInfo`). In the old code, these entities were processed by `vel_view` inside `scroll_system`. Post-extraction, `scroll_system` touches 0 of them (all three remaining views require `ObstacleScrollZ` or `BeatInfo`); `motion_system` now owns them.

| Metric | R2 baseline | R3 scroll_system | R3 motion_system | R3 combined |
|---|---|---|---|---|
| 10 entities (mean) | 48 ns | 11.3 ns (empty — 0 matching entities) | 50 ns | ~61 ns |
| 100 entities | N/A | 11.3 ns | 214 ns | ~225 ns |
| 1000 entities | N/A | 11.5 ns | 1923 ns | ~1935 ns |

**Combined 10-entity cost vs R2 baseline: 61 ns vs 48 ns (+27%).** This overhead is the cost of the architectural split: one extra function call, one extra phase guard check, one extra view construction. The per-entity work is identical.

The scroll_system 11 ns floor is the cost of three empty EnTT view iterations + one `ctx().get<GameState>()` + one `ctx().find<SongState>()` — it will only shrink further once real `ObstacleScrollZ` entities are present in a rhythm game bench fixture.

### Surprises / Coupling Discovered

**Phase guard coupling (handled):** The original `scroll_system` phase guard (`if phase != Playing return`) was silently gating `vel_view` and `motion_view` too. When first extracted without the guard, 3 existing tests failed (position-integration tests assumed no movement in non-Playing phase). Fixed by adding an identical phase guard to `motion_system`. No behavioral change.

**No other coupling found.** The two loops had zero shared state with the obstacle-scroll loops. Split was clean.

### Follow-ups for R4

1. **Add ObstacleScrollZ to bench fixture** — the `Bench: scroll_system` fixture should spawn entities with `ObstacleScrollZ` to actually stress the obstacle loops. Currently the fixture measures an empty scroll_system.
2. **ObstacleTag filter on vel_view/motion_view** — now that motion_system is isolated, adding an `ObstacleTag` exclude (or a dedicated non-obstacle tag) could tighten its loops further if obstacle entities shouldn't be processed there. Audit which entity types actually carry Position+Velocity in production.
3. **Legacy Position+Velocity migration** — with the migration path now cleanly owned by `motion_system`, continue the #349 migration: move obstacle entities to `WorldTransform + MotionVelocity`, then vel_view can be deleted.

### Module Health: 🟢 Green

SRP violation resolved. scroll_system now obstacle-only. motion_system owns motion. No regressions; tests pass; warnings zero.

---

### Decision: scoring_system SOLID Audit (Keyser Audit)

**Owner:** Keyser (SOLID Auditor)  
**Date:** 2026-05-03  
**Status:** ✅ AUDITED  
**Files audited:** `app/systems/scoring_system.cpp` (224 lines), `app/components/scoring.h`, `app/components/gameplay_intents.h`, `app/components/obstacle.h`, `app/components/rhythm.h`, `app/systems/popup_feedback_system.cpp`  
**Prior change reviewed:** `keaton-scoring-system-optimization.md`

### SOLID Table

| | State | Evidence | Recommendation |
|---|---|---|---|
| **S** | 🟡 | `scoring_system` mixes four distinct concerns in one function: (1) score/chain computation (`:72–79`), (2) miss processing — energy drain + death cause assignment (`:96–119`), (3) hit processing — energy recovery + chain bonus + popup queue filling (`:127–214`), (4) `displayed_score` animation interpolation (`:218–223`). The popup queue write (`:207`) is a **presentation concern** living inside a scoring function. `displayed_score` smoothing (`:218–223`) is a **rendering concern** — its only consumer is `gameplay_hud_screen_controller.cpp:339`. | Extract `displayed_score` interpolation to a lightweight `score_display_system`. Consider moving popup queue writes to `popup_feedback_system` by having `scoring_system` only emit a `ScoredHitEvent` list — `popup_feedback_system` already owns spawn + SFX. |
| **O** | 🟡 | New obstacle kinds that should be scoring-exempt require editing the hardcoded `LanePushLeft`/`LanePushRight` guard at `:158–163`. New combo rules require editing the hardcoded chain-bonus ladder at `:192–196`. The energy tier→delta mapping is an inline switch at `:170–184`. Design docs mandate data-driven patterns ("Bullet patterns should be data-driven where possible"). | Move the "no-score obstacle kinds" to a flag in `ObstacleKind` metadata or a `ScoringExemptTag`. Chain bonus table is already a `constexpr` array (`constants.h:47`) — extend the pattern to the tier→energy mapping. |
| **L** | 🟢 | No inheritance or polymorphism in this module. N/A. | — |
| **I** | 🟢 | Both views narrow correctly: `miss_view` (`:99`) requests `ObstacleTag, ScoredTag, MissTag, Obstacle` — `Obstacle` is the only readable component (`.kind` at `:105`), the rest are existential structural filters, which is appropriate. `hit_view` (`:130`) requests `Obstacle, Position` and uses both (`:132`). `model_hit_view` (`:142`) requests `Obstacle, ObstacleScrollZ` and uses both (`:144`). No view pulls a component it doesn't actually read. | — |
| **D** | 🟡 | `scoring_system` directly calls `popup_queue_for(reg)` (`:55–60`, `:155`) — a concrete find-or-emplace of `ScorePopupRequestQueue`. This couples scoring computation directly to a presentation message queue. `enqueue_energy_effect` (`:50–53`) similarly injects into `PendingEnergyEffects` — a lighter coupling since energy IS gameplay, but still a direct concrete call. Neither abstraction exists behind a boundary. | The popup coupling is the sharper violation. Scoring should express *what happened* (point values, positions, timing tiers); `popup_feedback_system` should decide *what to display*. One path: `scoring_system` writes to a plain `std::vector<ScoredHitRecord>` ctx singleton (no popup semantics), and `popup_feedback_system` reads it. |

### Cross-Check: Keaton's Behavior-Preservation Claim

**Claim:** Moving `popup_queue_for(reg)` inside `if (!hit_buf.empty())` (`:154`) is a pure perf optimization with no behavior change.

**Verdict: ✅ Preserved.**

Audit trace:
- The miss pass (`:96–119`) contains **zero calls** to `popup_queue_for` or any push to `ScorePopupRequestQueue`. Miss events only call `enqueue_energy_effect` and mutate `score.chain_count`/`gos->cause`.
- `popup_queue_for` was never reachable from the miss path, before or after the change.
- The `!hit_buf.empty()` guard (`:154`) ensures the lookup is skipped on frames with no scored hits. On frames with hits, behavior is identical: all hit records are iterated and requests pushed (`:207`) exactly as before.
- `popup_feedback_system.cpp:11–12` uses `find<ScorePopupRequestQueue>()` with an early-return null check — so if the queue was never emplaced (e.g. in a zero-hit test registry), it safely returns. No functional regression.

There is **no near-miss visual feedback** through the popup path for misses. The concern raised in the audit brief does not apply to this codebase.

### Single Most-Impactful Improvement

**Extract the `displayed_score` interpolation** (`:218–223`) into a dedicated `score_display_system` — it is a pure rendering/UI concern with one consumer (`gameplay_hud_screen_controller.cpp:339`), and its presence in `scoring_system` is the clearest SRP violation with zero downside to splitting.

### Patterns Shared with scroll_system (Round 1)

- **Hardcoded kind-checks vs data-driven design:** `scroll_system` swept all moving entities (structural over-breadth); `scoring_system` hard-codes obstacle kind exclusions inline (`:158–163`). Both reflect a preference for branching over tag/flag-driven dispatch.
- **No lazy-init regressions here:** Keaton's note that `ScoringSystemScratch`, `PendingEnergyEffects`, and `ScorePopupRequestQueue` remain lazy (find-or-emplace) is consistent with the established pattern for inter-system scratch/queue types. No eager-init violation.
- **No dead branches:** No unreachable `_with_X`/`_without_X` view splits found. The `hit_view`/`model_hit_view` split (`:130`, `:142`) is live — `model_hit_view` handles entities lacking `Position` (Model-authority bars with `ObstacleScrollZ`), a real production path.

### Module Health: 🟡 Yellow

Two actionable 🟡 items: SRP (popup queue + display interpolation in scoring body) and OCP (hardcoded obstacle-kind exclusion and chain ladder). No 🔴. Tests pass, warnings zero, Keaton's behavior claim holds.
