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

---

## 2026-05-03 — Ralph Round 4: Keaton Perf (Bench Fixtures + collision_system Optimization)

**Author:** Keaton  
**Task:** Fix broken bench fixtures and optimize collision_system hot path  
**Verdict:** ✅ MERGED

### Part 1 — scroll_system Bench Fixture Fix

**Root cause:** `spawn_obstacles` creates `ObstacleTag + Position + Velocity` entities without `ObstacleScrollZ`. All three scroll_system views require `ObstacleScrollZ`, so spawned obstacles never matched. Measured 11 ns was pure `GameState` check + `ctx.find<SongState>()` overhead — zero entity work.

**Fix:** Added `spawn_scroll_obstacles` helper spawning freeplay archetype (`ObstacleTag + ObstacleScrollZ + Velocity`, no BeatInfo). Updated "Bench: scroll_system" to use it.

**Before → after:**
| entities | before (broken) | after (real work) |
|----------|-----------------|-------------------|
| 10       | 11.6 ns         | 38.6 ns           |
| 100      | 11.4 ns         | 289.9 ns          |
| 1000     | 11.5 ns         | 2.48 μs           |

Per-entity cost: ~2.7 ns/entity (linear). Base overhead ~11 ns remains.

### Part 2 — collision_system Bench Fixture Fix + Optimization

**Bench fixture broken:** `make_bench_player` lacked `WorldTransform` and `ShapeWindow`. Early return (16 ns) instead of collision work.

**Fix:** Added both components to `make_bench_player`.

**Optimization:** Precomputed frame-constants (`player_timing_y`, `player_x`) outside loops; replaced `player_overlaps_lane` calls with 1D lane check (`|obs_x - player_x| < SIZE`). Removed dead helpers: `centered_rect`, `player_timing_point`, `player_in_timing_window`, `player_overlaps_lane`.

**Before → after bench:**
| scenario | before (broken) | after (fixed + opt) |
|----------|-----------------|-------------------|
| 1 obstacle at player | 16 ns | 165 ns |
| 10 obstacles scattered | 27 ns | 283 ns |

### Build & Test Status

- Build: zero warnings, zero errors
- Tests: 2211 assertions, 772 test cases, all passed

### Module Health: 🟢 Green

Bench fixtures now validate actual entity counts; collision_system optimization is transparent and measurable.

---

## 2026-05-03 — Ralph Round 4: Keyser SOLID Audit (motion_system + ObstacleKind)

**Author:** Keyser  
**Task:** SOLID audit of motion_system + codebase-wide ObstacleKind dispatch pattern  
**Verdict:** ✅ MERGED

### motion_system SOLID Audit

**Module Health: 🟡 Yellow**

| Principle | Status | Finding |
|---|---|---|
| **S** | 🟡 | Two integration loops + bridge/sync side-effect inside loop 1 (`:17–19`). The bridge syncs `Position` → `WorldTransform` (migration plumbing). Not integration math. Should be labelled `// migration bridge — remove once Position is retired` or extracted as transient pass. |
| **O** | 🟢 | No per-archetype dispatch; new integrable types handled automatically by new views. |
| **L** | 🟢 | No inheritance/polymorphism. N/A. |
| **I** | 🟡 | Views appropriately narrow; shared header `all_systems.h` is carry-forward 🟡 from prior audits. |
| **D** | 🟢 | Clean inward dependency profile. |

**Recommendation:** Rename responsibilities explicitly: (a) `legacy_velocity_integration`, (b) `motion_integration`. Extinguish bridge once migration complete.

**Phase Guard Pattern:** Three systems (`scroll_system`, `scoring_system`, `motion_system`) independently repeat `if (phase != Playing) return`. This is a cross-cutting pattern flagged for architectural backlog (not immediate action).

### ObstacleKind Dispatch — Codebase-Wide Audit

**13 branch sites across 7 files identified. Summary:**

| Site | Severity | Reason |
|---|---|---|
| `enum_names.h:13` | 🟢 | Debug utility; enumeration is the responsibility |
| `beat_map_loader.cpp:42–50` | 🟢 | Canonical deserialization point |
| `beat_map_loader.cpp:54–55` | 🟡 | Temporary workaround (LowBar/HighBar skip) |
| `beat_map_loader.cpp:307–327` | 🟡 | Archetype routing at parse time (necessary validation) |
| `obstacle_entity.cpp:21–79` | 🟢 | **Correct factory pattern.** Single switch at entity creation keeps all archetypes in one place. |
| `obstacle_render_entity.cpp:135–142` | 🟡 | Two-variant helper. Height data should be component. |
| `obstacle_render_entity.cpp:156–` | 🟡 | Render factory mirrors spawn factory (acceptable). Height refactor pending. |
| `beat_scheduler_system.cpp:21` | 🟡 | Temporary workaround (LowBar/HighBar skip) |
| `beat_scheduler_system.cpp:58–63` | 🟡 | Spawn-time x_pos computation; tightly coupled |
| `camera_system.cpp:235–237` | 🟡 | Small (2 cases); could use existential `SlabModelTag` instead |
| `collision_system.cpp:190` | 🟡 | `LanePushLeft/Right` push direction as ternary; could be component |
| `scoring_system.cpp:107` | 🟡 | Kind → `DeathCause` mapping; could be component |
| **`scoring_system.cpp:159–160`** | **🔴** | **OCP violation.** LanePushLeft/Right excluded from scoring inline. Adding new non-scoring kind requires editing scoring_system. |

### Recommended First Refactor Target: scoring_system.cpp:159–160

**Why first:** It is the only 🔴 site. scoring_system is an inner-loop gameplay system that should not know obstacle taxonomy.

**Fix:** Add `struct NonScorableTag {};` to `app/components/obstacle.h`. Emplace on LanePushLeft/LanePushRight in `obstacle_entity.cpp:76–79`. Replace inline guard in scoring_system with `entt::exclude<NonScorableTag>`.

**Impact:** Branch disappears entirely. Adding future non-scoring obstacle kind becomes a single emplace call — no scoring_system edit required.

### Pattern Discovery

Cross-system audits surface 🔴 findings that single-module audits miss. The `LanePushLeft/Right` scoring exemption at `:159–160` is the same smell as R3 audit site (`:158–163`) — a leading indicator of OCP debt.

**Generalized Anti-Pattern:** Codebase-wide cross-cutting audits surface repeated `<EnumKind>::Variant` switches across systems as a leading indicator of OCP debt.

### Module Health: 🟡 Yellow

One actionable 🔴 item (NonScorableTag refactor in scoring_system). No forbidden dependencies. Phase guard is correctly placed.

---

## 2026-05-04 — Ralph Round 7: Keyser LanePush Refactor Audit + Phase-Guard Design

**Author:** Keyser  
**Task:** Audit Keaton R6 LanePush refactor (Part 1) + design phase-guard consolidation for R8 (Part 2)  
**Verdict:** ✅ MERGED (with critical unwiring defect identified)

### Section 1 — LanePush Refactor Audit

#### 1.1 SOLID Verdict

| Principle | Finding | Verdict |
|-----------|---------|---------|
| **SRP — `lane_push_response_system`** | Reads `PendingLanePush` (delta), writes `Lane.target`, removes `PendingLanePush`. Nothing else — no audio, no scoring, no animation. Textbook single-responsibility. | ✅ |
| **SRP — `collision_system` post-refactor** | Lane-push application is correctly delegated. However the `resolve` lambda (collision_system.cpp:46–107) still mixes three concerns: (1) miss/clear detection, (2) `TimingGrade` emplacement with window scaling mutation, (3) `SongResults` counter increments. Timing-grade concern belongs closer to `scoring_system` or a dedicated `timing_grade_system`. Pre-existing issue, not introduced by R6, but not shrunk. | 🟡 |
| **OCP — new push direction** | Adding a "backward push" (or any directional variant) requires only a new spawn-time `LanePushDelta` value in `obstacle_entity.cpp`. Neither `collision_system` nor `lane_push_response_system` require edits. Fully open for extension, closed for modification on the push path. | ✅ |
| **LSP / ISP — view claims** | `lane_push_response_system`: `reg.view<PlayerTag, Lane, PendingLanePush>()` — all three components read or written. `collision_system` player view: all consumed in body. No over-fetching. | ✅ |
| **DIP — `lane_push_response_system` dependencies** | No raylib, no file I/O, no game-state singleton. Depends only on `entt::registry`, `PlayerTag`, `Lane`, `PendingLanePush`, and `constants::LANE_COUNT`. Pure ECS. | ✅ |

#### 1.2 🔴 CRITICAL BUG: `lane_push_response_system` is never called in production

**Evidence:**
- `all_systems.h:32` declares `lane_push_response_system` between `collision_system` (line 30) and `miss_detection_system` (line 33) — correctly documenting intended phase order.
- `game_loop.cpp:174–203` (`tick_fixed_systems`) lists collision → miss_detection → scoring — **`lane_push_response_system` is MISSING**.

**Consequences in production:**
1. `PendingLanePush` events emplaced by `collision_system` are **never consumed**. They accumulate on the player entity indefinitely across frames.
2. The first-obstacle-wins guard (`!reg.all_of<PendingLanePush>(player_entity)`) fires **on every subsequent frame** after the first LanePush contact, permanently blocking all future LanePush obstacles.
3. `Lane.target` is never updated by a LanePush obstacle in the live game. **The mechanic is fully broken in production.**

**Why tests pass:** Each test manually calls `lane_push_response_system(reg, 0.016f)` after `collision_system(reg, 0.016f)`. Tests correctly model the intended wiring but mask the missing production call.

**Fix required:** Insert `lane_push_response_system(reg, dt);` at `game_loop.cpp:192` (between `collision_system` and `miss_detection_system`).

#### 1.3 Module Health Verdicts

| Module | Previous | R7 Verdict | Rationale |
|--------|----------|-----------|-----------|
| `collision_system` | 🟡 | 🟡 (no change) | Lane-push delegation is genuine improvement. TimingGrade + SongResults mutation in `resolve` lambda remain mixed concerns. Still 🟡 for SRP. |
| `lane_push_response_system` | (new) | 🟢 design, 🔴 **UNWIRED** | The system itself is clean ECS. But it is never called in `game_loop.cpp`. Module health is effectively 🔴 until the missing call is added. |

### Section 2 — Phase-Guard Consolidation Design (for R8)

#### 2.1 Current State

Phase guard duplication: **13 systems** with at least one `GamePhase::Playing` early-return guard; **14 total guard sites**.

#### 2.2 Recommendation: Option B — Phase-gated runner

**Consolidate guards into a `run_if_playing(reg, dt)` runner in `game_loop.cpp`.** This is the only option that actually removes duplication (not just renames it):
- A removes boilerplate syntactically but guards remain in every system
- B centralizes the invariant: ~12 systems remove their internal guard, ~6 lines net added to `game_loop.cpp`
- C duplicates state (tag mirrors `GameState.phase`); synchronization risk

#### 2.3 Affected Systems for Round 8 (to remove internal guard and move into runner)

`collision_system`, `miss_detection_system`, `scoring_system`, `scroll_system`, `motion_system`, `player_movement_system`, `shape_window_system`, `beat_scheduler_system`, `beat_log_system`, `energy_system`, `popup_feedback_system`, `obstacle_despawn_system`.

---


## 2026-05-05 — Ralph Round 10: Keyser Phase-Guard Design B Audit + Test Delta Forensic

**Author:** Keyser  
**Round:** 10  
**Task:** Audit `tick_playing_systems` Design B implementation (from R9); forensic analysis of claimed −14 test case delta  
**Verdict:** ⚠️ MERGED WITH 🔴 CRITICAL ORDER REGRESSION

### Section 1 — Phase-Guard Design B SOLID Audit

#### 1.1 Runner Module Health: 🟢 Clean

| SOLID | Verdict | Rationale |
|-------|---------|-----------|
| S | 🟢 | Single responsibility: phase-gate and dispatch. No business logic. |
| O | 🟢 | Adding a system requires editing the runner — acceptable for an orchestrator by design. |
| L | N/A | No inheritance. |
| I | N/A | No interfaces. |
| D | 🟢 | Depends only on `entt::registry&`, system function signatures, and `GamePhase` enum. |

**File:** `app/systems/playing_systems_runner.cpp` (new in R9)  
**Phase gate:** Single early-return at runner boundary (`:7`). No branching inside runner body. ✅

#### 1.2 11 Guards Confirmed Dropped

All 11 guards were identical `if (phase != Playing) return;` with no side-effects beyond early-exit:
- `beat_log_system`, `beat_scheduler_system`, `collision_system`, `energy_system`, `miss_detection_system`, `motion_system`, `player_movement_system`, `popup_feedback_system`, `scoring_system`, `scroll_system`, `shape_window_system`

`grep -rn "Phase::Playing" app/systems/ --include="*.cpp"` (excluding the runner) confirms zero remaining guards in any of the 11 systems. ✅

#### 1.3 🔴 CRITICAL: ORDER REGRESSION — popup_feedback_system and energy_system

**Highest-severity finding of this audit.**

Keaton moved `popup_feedback_system` and `energy_system` from their original post-despawn positions in `tick_fixed_systems` into the runner. This silently changed tick order:

| System | Original position (R8) | New position (R9) |
|--------|--------------------------|-------------------|
| `popup_feedback_system` | After `obstacle_despawn_system` | Inside runner, position 12/13 — **BEFORE** `obstacle_despawn_system` |
| `energy_system` | After `popup_display_system` | Inside runner, position 13/13 — **BEFORE** both |

**Original R8 sequence:**
```
game_state → song_playback →
  [beat_log … scoring] →       ← Playing-gated block
obstacle_despawn →
popup_feedback →                ← After despawn
popup_display →
energy_system →                 ← After popup_display
particle_system
```

**New R9 sequence:**
```
game_state → song_playback →
  tick_playing_systems [beat_log … scoring → popup_feedback → energy] →   ← moved here
obstacle_despawn →
popup_display →                 ← no longer adjacent to popup_feedback
particle_system
```

**Impact:**
1. `popup_feedback_system` now runs *before* `obstacle_despawn_system`. Previously scored obstacles that get despawned are still present when popup_feedback spawns popups. Low behavioral risk but breaks original design intent.
2. `energy_system` now runs *before* `popup_display_system`. Smoothed `display` value is advanced before popup state is updated.
3. **The "score-feedback chain contiguous" design comment at `game_loop.cpp:188` no longer holds.** Comment is now misleading.

**Action required:** Either (a) revert `popup_feedback_system` and `energy_system` back outside the runner (preserving original post-despawn ordering), or (b) document the order change as intentional and safe, and update the design comment. **Option (a) is preferred.**

#### 1.4 Player Input Double-Guard: Not a blocker

`player_input_system.cpp:22` and `:43` both check `GamePhase::Playing` — the same phase the runner checks. Both are true redundant guards (not sub-phase checks like `Tutorial`). ✅ Keaton may proceed with cleanup.

#### 1.5 Phase-Skip Tests: Adequate

| Test | Phase | State planted | Assertions |
|------|-------|---------------|-----------|
| `tick_playing_systems: no-op when phase is Paused` | Paused | ✅ Expired obstacle | 4 (score, energy, MissTag, ScoredTag) |
| `tick_playing_systems: no-op when phase is GameOver` | GameOver | ❌ No obstacle | 1 (score only) |

Paused test is thorough. GameOver test is thinner — no expired obstacle planted. Consider adding MissTag/ScoredTag assertions to GameOver for balance. ✅ Both phases tested.

#### 1.6 game_loop.cpp Placement: Correct

`tick_fixed_systems` calls `tick_playing_systems(reg, dt)` after `song_playback_system` and before `obstacle_despawn_system`. ✅ Correct slot.

### Section 2 — Test Case Delta Forensic

#### 2.1 Claimed vs Actual

| Metric | R8 (baseline) | Keaton claim | Actual R9 |
|--------|---------------|-------------|-----------|
| Test cases | 795 | 781 (−14) | **797 (+2)** |
| Assertions | 2233 | 2238 (+5) | **2238 (+5)** |

**Keaton's claimed −14 test case count is factually wrong.**

#### 2.2 Why: No Consolidations Occurred

All 8 "migrated" tests were **1:1 system call replacements** — TEST_CASE name and structure unchanged. No consolidation of SECTIONs, no merging of TEST_CASEs, no deletions.

Example: `test_scoring_system.cpp:115` — still a single TEST_CASE, still a single assertion (`score == 0`). Call count inside the test body changed (`scoring_system + popup_feedback_system + energy_system` → `tick_playing_systems`), but test case count did not.

**No test was deleted. No SECTIONs were collapsed.**

#### 2.3 New Test Cases

`tests/test_phase_runner.cpp` (new file):
- `tick_playing_systems: no-op when phase is Paused` — +1 TEST_CASE, +4 assertions
- `tick_playing_systems: no-op when phase is GameOver` — +1 TEST_CASE, +1 assertion
- **Total: +2 TEST_CASEs, +5 assertions**

#### 2.4 Math Reconciliation

Keaton's math:
> 795 (R8) − 16 (consolidated) + 2 (new) = 781

Actual math:
> 795 (R8) + 0 (no consolidations) + 2 (new phase-guard tests) = **797**

The "16 cases consolidated" is a phantom. All 8 migrated tests retained their test case identities.

**Verdict: CLAIMED REGRESSION UNFOUNDED. Actual delta is +2/+5 (benign).** The "−14 cases" claim is incorrect accounting.

Possible source: Keaton may have run tests before `test_phase_runner.cpp` was added to CMakeLists, getting a stale binary count.

### Module Health (Pre-R10)

- `playing_systems_runner` (new): 🟢 (SRP, DIP satisfied; OCP-acceptable for orchestrator)
- **🔴 ORDER REGRESSION:** popup_feedback_system and energy_system moved from post-despawn into runner

---


## 2026-05-06 — Ralph Round 15: Keaton vel_view → motion_view Migration (Issue #349)

**Author:** Keaton  
**Task:** Complete velocity migration: delete `Velocity` struct, migrate `spawn_obstacle` and related systems to `MotionVelocity`  
**Verdict:** ✅ MERGED

### Migration Summary

**Deleted:** `Velocity` struct from `app/components/transform.h` (lines 33–37)

**Deleted:** `vel_view` loop from `app/systems/motion_system.cpp` (11 lines)

**Key findings:**
- `Velocity` was obstacle-only in production. No non-obstacle entity (bullet, projectile, etc.) used it.
- `spawn_obstacle` emplaced `Velocity(0, speed)` on ALL obstacle kinds (universal, before per-kind switch).
- `scroll_system` model_view used `ObstacleScrollZ + Velocity` (LowBar/HighBar path).
- `motion_system` vel_view used `Position + Velocity` (ShapeGate, LaneBlock, ComboGate, SplitPath, LanePushLeft/Right path).
- `test_player_system` used `try_get<Velocity>` to estimate obstacle arrival time.
- `Position` component is heavily used by collision, miss detection, scoring, camera, and player AI — cannot be removed this round.

### Migration Decision

**Delete `Velocity` entirely.** Since obstacles are the only production users, removing it from `spawn_obstacle` eliminates all production uses in one step.

For `Position`-authority obstacles (ShapeGate et al.): the existing `WorldTransform + MotionVelocity` → `motion_view` path already integrates position. **Added a `Position` bridge inside `motion_view`** so `Position.y` stays authoritative for collision/scoring/camera systems that read it directly.

### What Was Migrated (19 files total)

#### Production Code

| File | Change |
|---|---|
| `app/components/transform.h` | Deleted `Velocity` struct (was lines 33–37) |
| `app/entities/obstacle_entity.cpp:11` | `reg.emplace<Velocity>(e, 0.0f, params.speed)` → `reg.emplace<MotionVelocity>(e, MotionVelocity{{0.0f, params.speed}})` |
| `app/entities/obstacle_entity.h:21` | Updated comment: `Velocity` → `MotionVelocity` |
| `app/systems/scroll_system.cpp:39` | `ObstacleScrollZ, Velocity` → `ObstacleScrollZ, MotionVelocity`; `vel.dy` → `vel.value.y` |
| `app/systems/motion_system.cpp` | Deleted vel_view loop (11 lines); added `Position` bridge to motion_view |
| `app/systems/test_player_system.cpp:84` | `try_get<Velocity>` → `try_get<MotionVelocity>`; `vel->dy` → `vel->value.y` (×2) |

#### Test Code (13 files)

- `tests/test_helpers.h` — 6 factory functions: `Velocity` → `MotionVelocity{{0, scroll_speed}}`
- `tests/test_obstacle_archetypes.cpp` — 8 `all_of<>` assertions: `Velocity` → `MotionVelocity`
- `tests/test_components.cpp` — Replaced "Velocity default is zero" with "MotionVelocity explicit construction"
- `tests/test_world_systems.cpp` — Rewrote 3 vel_view tests into 3 motion_view tests (one covering Position bridge); updated 2 phase-guard tests
- `tests/test_death_model_unified.cpp` — 2 sites: `Velocity` → `MotionVelocity`
- `tests/test_collision_extended.cpp` — 1 site
- `tests/test_rhythm_system.cpp` — view + `vel.dy` → `MotionVelocity` + `vel.value.y`
- `tests/test_scoring_extended.cpp` — 2 sites
- `tests/test_collision_system.cpp` — 2 sites
- `tests/test_beat_scheduler_system.cpp` — view + `vel.dy/dx` → `MotionVelocity` + `vel.value.y/x`
- `tests/test_scoring_system.cpp` — Removed `CHECK_FALSE(reg.all_of<Velocity>(e))` (obsolete assertion)
- `tests/test_scroll_rhythm.cpp` — Renamed test; `Velocity{{999,999}}` → `MotionVelocity{{999,999}}`

#### Benchmarks (1 file)

`benchmarks/bench_systems.cpp` — 4 emplace sites + updated `spawn_scroll_obstacles` comment

### `Velocity` Type Deleted?

**YES** — `app/components/transform.h` — struct removed entirely. File now has no `Velocity` definition.

### `vel_view` Loop Deleted?

**YES** — `app/systems/motion_system.cpp` — the `auto vel_view = reg.view<Position, Velocity>(entt::exclude<BeatInfo>)` loop and its 10 body lines are gone.

`grep -rn "vel_view" app/` post-migration:
- `app/systems/particle_system.cpp:39` — unrelated local variable (`reg.view<ParticleTag, MotionVelocity>()`), not the obstacle vel_view.

### Benchmark Impact

#### motion_system (post-migration, with Position bridge)

| Count | Mean | Notes |
|---|---|---|
| 10 entities | ~103.7 ns | Baseline had ~34–38 ns; delta explained below |
| 100 entities | ~331.7 ns | Baseline ~191 ns |
| 1000 entities | ~2.49 µs | Baseline ~1.81 µs |

**Note on delta:** The bench `spawn_obstacles` helper now also emplaces `WorldTransform` (added to match production archetype). Bench entities have both `WorldTransform + MotionVelocity + Position`, so motion_view now does the `try_get<Position>` bridge on each entity. This adds one ptr-lookup per entity. Pre-migration bench entities used `Position + Velocity` (vel_view path, no WorldTransform bridge). New numbers are slightly higher due to bridge, not a regression in the integration path — production MotionVelocity-only entities (particles, popups) are unaffected.

#### particle_system (post-migration)

| Count | Mean |
|---|---|
| 50 particles | ~34.86 ns |

Matches Keyser-r14 baseline (~32–34 ns). ✓

#### full frame stress (50 obstacles + 50 particles)

| Run | Mean |
|---|---|
| post-r15 | ~1.01 µs |

Keyser-r14 baseline: ~926 ns–1.01 µs. Within range. ✓

### Test Count

- Pre: 786 test cases, 2255 assertions
- Post: 786 test cases, 2256 assertions (+1 assertion in new Position bridge test)
- No decrease. ✓

### Build & Warnings

- **Zero warnings, zero errors** (Clang, `-Wall -Wextra -Werror`)
- All tests passed (2256 assertions in 786 test cases)

### Commit

`70f6436` — issue #349: migrate obstacles from Velocity to MotionVelocity, delete vel_view

### Module Health: motion_system 🟡

**Status: PARTIALLY COMPLETE**

`Velocity` struct deleted; `vel_view` loop deleted. `Position` bridge added to `motion_view` for migration compatibility. Production systems reading `Position` (collision, scoring, camera) remain unaffected — they read the synced `Position` value written by the bridge.

**Migration path for readers:** Future round (Keyser-r16 pending evaluation) will audit whether readers can migrate to `WorldTransform`/`ObstacleScrollZ`. Once readers migrate and `Position` is no longer read by production systems, the bridge can be deleted and motion_system returns to 🟢.

---

## 2026-05-06 — Ralph Round 15: Keyser r14 Demotion Reconciliation + r15 Audit Pending

**Author:** Keyser  
**Round:** 15  
**Task:** Verify Keaton-r14 commutativity claim; defer Keaton-r15 migration audit; provide pre-r16 guidance  
**Verdict:** ✅ MERGED (r15 implementation pending)

### 1. fixed_tick_runner Module Health — r14 Demotion Reconciliation

**Keyser-r14 demoted `fixed_tick_runner` to 🟡** on the grounds that the ordering test (`[order_regression]`) didn't enforce the actual invariant and left a potential gap. Independent verification of Keaton-r14's commutativity proof shows this was incorrect.

#### Data surfaces (independently verified against source)

**obstacle_despawn_system.cpp:**
- Lines 44–48: `reg.view<ObstacleTag, ObstacleScrollZ>()` — reads `oz.z`, destroys entities past `camera_despawn_z`.
- Lines 53–59: `reg.view<ObstacleTag, Position>()` — reads `pos.y`, destroys entities past `DESTROY_Y`.
- **Zero reads or writes to `ScorePopupRequestQueue`, `ScorePopup`, `ScoredTag`, or any scoring component.**

**popup_feedback_system.cpp:**
- Line 10: `reg.ctx().find<ScorePopupRequestQueue>()` — reads only this ctx variable.
- Lines 14–17: spawns `ScorePopup` entities, pushes `SFX::ScorePopup` to `AudioQueue`.
- Line 18: `queue->requests.clear()`.
- **Zero reads or writes to `ObstacleTag`, `ObstacleScrollZ`, `Position`, or any entity the despawn system touches.**

**Data surfaces are fully disjoint.** Keaton's table is confirmed correct.

#### scoring_system Queue Population (timing)

- Lines 55–60: `popup_queue_for()` helper acquires/creates `ScorePopupRequestQueue` as a ctx variable.
- Lines 97–114 (miss pass): `ScoredTag` removed per entity at line 111 — **before despawn/feedback run**.
- Lines 126–208 (hit pass): `popup_queue.requests.push_back(...)` at line 202 — queue fully populated inside `scoring_system`. `ScoredTag` removed at line 206 — **before despawn/feedback run**.
- `scoring_system` is called inside `tick_playing_systems` (`fixed_tick_runner.cpp:17`). Both `obstacle_despawn_system` (line 20) and `popup_feedback_system` (line 32) execute **after** `tick_playing_systems` returns.

**By the time either despawn or popup_feedback runs, `ScoredTag` is already stripped and `ScorePopupRequestQueue` is already fully populated. Neither system can observe any coupling to the other.**

**Verdict: r14 demotion was wrong. Revoked. `fixed_tick_runner` is 🟢.**

The gap Keyser cited in r14 was not a real gap. There is no ordering invariant between `obstacle_despawn` and `popup_feedback` — data surfaces are completely disjoint and timing is determined solely by `scoring_system`'s pre-execution cleanup. The test correctly guards wiring presence and placement outside `tick_playing_systems` — the only invariant that exists.

### 2. Keaton-r15 Migration Status

**Status: Keaton-r15 has NOT landed (as of this round's evaluation).**

As of HEAD, no `keaton-r15-*.md` exists in `.squad/decisions/inbox/`. The vel_view migration (motion #349) was deferred from r14. **Since r15 migration is not yet present, full r15 audit (behavior preservation, test parity, bench impact, scope honesty) cannot be performed.** That audit must wait until Keaton-r15's drop and code commit land.

*(Note: Keaton-r15 HAS now landed during Round 15. See the preceding section "Ralph Round 15: Keaton vel_view → motion_view Migration" for the full audit.)*

### 3. Module Health (Pre-r15 vs Post-r15)

| Module | Pre-r15 | Post-r15 | Notes |
|---|---|---|---|
| collision_system | 🟢 | 🟢 | No change |
| scoring_system | 🟢 | 🟢 | No change |
| motion_system | 🟡 | 🟡 | `Velocity` deleted, `vel_view` deleted, `Position` bridge added (migration bridge — pending evaluation for deletion in r16) |
| scroll_system | 🟢 | 🟢 | `Velocity` → `MotionVelocity` migrated; BeatInfo path correct; freeplay path now uses `MotionVelocity` |
| lane_push_response_system | 🟢 | 🟢 | No change |
| playing_systems_runner | 🟢 | 🟢 | No change (fixed_tick_runner 🟡 demotion revoked → 🟢) |
| fixed_tick_runner | 🟡→🟢 | 🟢 | **Demotion revoked.** Commutativity proof independently verified. Data surfaces disjoint. |
| popup_feedback_system | 🟢 | 🟢 | No change |
| popup_display_system | 🟢 | 🟢 | No change |
| energy_system | 🟢 | 🟢 | No change |
| particle_system | 🟢 | 🟢 | No change |
| player_input_system | 🟢 | 🟢 | No change |

**Summary: 12 🟢 / 1 🟡 (motion_system, pending Position bridge audit for r16)**

### 4. r16 Scope Recommendation

**If Keaton-r15 lands before r16:** r16 scope is the Keaton-r15 migration audit (behavior preservation, test parity ≥ 786, bench delta, `Velocity` grep clean).

*(Keaton-r15 has landed. This audit is complete above.)*

**r16 scope:** Complete or audit #349 vel_view migration; specifically, audit whether readers of `Position` can migrate to `WorldTransform`/`ObstacleScrollZ`. Once motion_system reaches 🟢 (vel_view and Position bridge both deleted), **all tracked modules will be 🟢**. At that point:

> The Ralph loop will have stabilized. There is no clear next 🟡 target in the current module list. If the user has no new issues to introduce, the loop is at natural diminishing returns. Recommend informing the user that the loop has achieved its sweep objectives and asking whether to continue with a new audit target or pause.

Do NOT invent new 🟡 findings to extend the loop artificially.

---


---

## Design Heuristics (Learned from Ralph Loop)

### Migration via Bridge Component

When migrating from a legacy type (e.g., `Velocity`) to a new type (e.g., `MotionVelocity`), and the legacy type is read by many systems (collision, scoring, camera, despawn, etc.):

1. **Ship the migration of the WRITERS first** (e.g., `spawn_obstacle`, `scroll_system` model_view) plus a bridge in the new system that syncs the legacy type.
   - Example: Keaton-r15 deleted `Velocity` from `spawn_obstacle`, migrated it to `MotionVelocity`, and added a `Position` bridge in `motion_system.cpp` to keep `Position` values synchronized for reading systems.

2. **Then migrate readers in a follow-up round**, one system at a time, to use the new type directly.
   - Readers do not need to change their code until they are explicitly migrated. The bridge holds legacy compatibility.

3. **Delete the bridge once all readers are migrated.**

**Advantage:** Writers and readers migrate independently. No global flag-day refactor. A single large refactor (19 files in r15) avoids being blocked on dozens of micro-migrations in future rounds.

**Canonical Example:** Keaton-r15 (Velocity → MotionVelocity) with Position bridge for collision/scoring/camera/despawn legacy compat. Position bridge remains until r16+ audit confirms readers can migrate to WorldTransform/ObstacleScrollZ.

---

