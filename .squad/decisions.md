# Squad Decisions

## Active Decisions

### EnTT Input Model Guardrails (2026-04-27)

**Owners:** Keyser (diagnostics), Keaton (implementation), Baer (validation), McManus (integration)  
**Status:** PRE-IMPLEMENTATION GUIDANCE

Replace hand-rolled `EventQueue` fixed arrays with `entt::dispatcher` stored in `reg.ctx()`. Use `enqueue`+`update` (deferred delivery) — **not** `trigger`. System execution order remains unchanged; dispatcher becomes the transport layer.

**Target architecture:**
- `reg.ctx().emplace<entt::dispatcher>()` at init (consistent with existing singleton pattern)
- Preserve `(entt::registry&, float dt)` system signature convention
- Listeners registered in `game_loop_init` in canonical order (block-comment documented)

**Event delivery:** Two-tier `enqueue`+`update`
- Tier 1: `input_system` enqueues `InputEvent`, then `disp.update<InputEvent>()` fires gesture_routing and hit_test listeners (before fixed-step)
- Tier 2: Inside `player_input_system`, `disp.update<GoEvent/ButtonPressEvent>()` drains those queues (fixed-step-only delivery)
- Eliminates manual `eq.go_count = 0` anti-pattern; no-replay invariant (#213) preserved

**Seven guardrails:**
1. **R1 — Multi-consumer ordering:** Registration order in `game_loop_init` is canonical
2. **R2 — No overflow cap:** Verify test_player_system ≤ prior MAX=8/frame
3. **R3 — clear vs update:** `clear` skips listeners (defensive cleanup); `update` fires listeners
4. **R4 — Listener registry access:** Use payload/lambda; no naked global ref
5. **R5 — No connect-in-handler:** EnTT UB; all connects in init/shutdown only
6. **R6 — trigger prohibited:** For game input; only out-of-band signals (app suspend)
7. **R7 — Stale event discard:** Phase transitions leave events queued; start-of-frame `clear` discards (add Baer test)

**Migration order:**
1. Add dispatcher to ctx (inert)
2. Migrate InputEvent tier → gesture_routing + hit_test listeners
3. Migrate GoEvent/ButtonPressEvent tier → player_input_system handlers
4. Remove EventQueue struct
5. Baer gate: R7 test + no-replay validation

**Preserved invariants:**
- No multi-tick input replay (#213) — `update()` on empty queue = no-op
- Deterministic fixed-step — raw input and gesture/hit routing stay pre-loop
- No frame-late input — defensive `clear()` at input_system top
- Taps → ButtonPressEvents — hit_test listener logic unchanged
- Swipes → GoEvents — gesture_routing listener logic unchanged
- MorphOut interrupt (#209) — inside player_press_handler, unchanged
- BankedBurnout first-commit-lock (#167) — inside player_input_system, unchanged

---

### #135 — Difficulty Ramp: Easy Variety + Medium LanePush Teaching (2026-04-27)

**Owners:** Saul (design), Rabin (initial impl, locked out), McManus (revision), Baer (initial testing, locked out), Verbal (revision), Kujan (review)  
**Status:** APPROVED

Easy difficulty gains controlled *variety without complexity*: rhythm-driven (section density, gap/lane distribution) and shape-driven (3-shape rotation), but zero new mechanics. Medium LanePush becomes a *taught mechanic*: shape-only intro (no pushes before max(30s, first chorus)), capped share (5–25%), readable spacing (min 3 beats between consecutive, 4-beat telegraph window around first 3), and safe directions (no wall-pushes).

**Design target (Saul):**
- Easy: `shape_gate` only, ≥20% non-center lane distribution, no >6 consecutive same-lane, no >12 consecutive same-shape, no single gap >50%, section density variation
- Medium: First `lane_push` ≥ later of (30s, first chorus); LanePush ≤20% share; min 3-beat gap between consecutive; first 3 have ≥4-beat windows; no wall-pushes
- Hard: Unchanged from #125 (bars only)

**Initial implementation (Rabin):** `apply_lanepush_ramp` + `balance_easy_shapes` + beatmap regeneration. ❌ REJECTED by Kujan: Easy contained 1.6–4.1% lane_push (contract violation). Baer's tests lacked kind-exclusion guard (violation passed silently).

**Revision (McManus + Verbal):** Set `LANEPUSH_RAMP["easy"] = None` (disable injection for easy). Added `[shape_gate_only]` C++ test + `check_easy_shape_gate_only()` Python validator. ✅ APPROVED by Kujan.

**Validation:**
- Easy: 100% shape_gate (zero lane_push/bars), 3 shapes, dominant ≤60%
- Medium: lane_push 9.3–19.5% (stomper/drama/mental), max consecutive ≤3, start_progress 0.30 respected
- Hard: bars intact (stomper 1/3, drama 2/2, mental 7/7) — #125 contract preserved
- #134 shape gap: all 9 combos pass
- 2366 C++ assertions (730 test cases), all pass
- Python validators: bar coverage, shape gap, difficulty ramp all pass

**Kind-exclusion convention:** Every difficulty contract test must include (1) kind-exclusion assertions and (2) distribution/variety assertions. This catches both *presence* violations and *distribution* violations.

**Non-blocking note:** Medium test lacks start_progress assertion in C++ (generator-enforced, content-valid; future regen with changed start_progress would pass C++ silently). Noted for hardening ticket.

---

### #134 — Enforce min_shape_change_gap in Shipped Beatmaps (2026-04-26)

**Owners:** Rabin (content), Baer (validation), Kujan (review)  
**Status:** APPROVED

Rule 6 (`min_shape_change_gap = 3` beats) now enforced in shipped beatmap generation. Shape-bearing obstacles (`shape_gate`, `split_path`, `combo_gate`) must be ≥3 beats apart; if violation detected after cleaner passes, the later gate is mutated to match the prior shape.

**Implementation:**
- `tools/level_designer.py`: new `clean_shape_change_gap` pass (runs after all earlier cleaners, before `clean_two_lane_jumps`)
- `tools/check_shape_change_gap.py`: Python validation tool for authoring-time checks
- `tests/test_shipped_beatmap_shape_gap.cpp`: C++ regression test (Catch2), loaded into CI gate via `file(GLOB TEST_SOURCES)`

**Why mutate, not drop:**
Dropping reduces density and risks emptying short sections. Mutating never violates the gap rule (same-shape consecutive gates allowed) and preserves rhythm content (choruses, outros, #125 bar coverage).

**Validation:**
- All 9 shipped beatmap×difficulty combos pass validation
- All 2360 C++ assertions pass (751 test cases)
- No regression to #125 LowBar/HighBar coverage (stomper 1/3, drama 2/2, mental 7/7)
- All difficulties populated (100–206 beats)

**Non-blocking notes:**
- `check_shape_change_gap.py` not currently in CI YAML; C++ test is authoritative. Python tool is dev-time utility; could be added as belt-and-suspenders in future.
- Pipeline order: `clean_shape_change_gap` → `clean_two_lane_jumps` (final). Low risk; flag if `clean_two_lane_jumps` extended to mutate shapes.

---

### #167 — Bank-on-Action Burnout Multiplier (2026)

**Owners:** Saul (design), McManus (implementation)  
**Status:** IMPLEMENTED

Burnout multiplier is snapshotted onto obstacle entities (`BankedBurnout` component) at the moment the player commits a qualifying input (shape press or lane change), not at geometric collision time. `scoring_system` reads `BankedBurnout::multiplier` instead of live `BurnoutState::zone`.

**Design:** `BankedBurnout { float multiplier; BurnoutZone zone; }` emplaced on obstacle entity during `player_input_system`. First-commit-locks prevent overwrite. Scoring falls back to `MULT_SAFE (1.0)` if no bank is present. LanePush obstacles are excluded from the scoring ladder (chain, popup, best_burnout).

**Validation:**
- 11 new `[burnout_bank]` Catch2 tests, all pass
- All existing `[scoring]`, `[player]`, `[player_rhythm]` tests pass
- Zero compilation warnings on macOS (arm64) clang

---

### Agent Role Fit Assessment (2026-04-26)

**Owners:** Edie, Keyser  
**Status:** DOCUMENTED

Reviewed all 13 specialist agents against README and architecture docs. Assessment:

**High usefulness:**
- McManus (Gameplay), Hockney (Platform), Keaton (C++ Perf), Kobayashi (CI/CD), Fenster (Tools), Baer (Testing), Kujan (Review), Rabin (Level Design), Saul (Game Design), Edie (Product)

**Medium usefulness:**
- Verbal (QA edge cases), Redfoot (UI/UX)

**Infrastructure roles:**
- Scribe (Session Logger), Ralph (Work Monitor)

**Handoff clarity:**
1. QA vs Test: Verbal owns edge-case unit tests; Baer owns regression and platform validation
2. Rhythm tuning: McManus implements systems; Saul owns balance; Rabin owns per-song layouts. Heuristic changes route to Rabin first, Saul reviews.

**Gap identified:** No dedicated audio/music engineer. Fenster covers the Python pipeline side; deeper audio work (sync drift, onset detection refinements) has no specialist owner. Not blocking for current scope.

---

### #46 — Release Workflows (2026-04-26)

**Owners:** Ralph (implementation), Coordinator (validation), Kujan (review)  
**Status:** APPROVED

Push-to-main and insider-branch release workflows implemented and approved.

**Workflow structure (both `squad-release.yml` and `squad-insider-release.yml`):**
1. Checkout + cache build directory
2. Install Linux dependencies (libx11-dev, libxrandr-dev, libxinerama-dev, libxcursor-dev, libxi-dev, libxext-dev)
3. Set VCPKG_ROOT environment variable
4. Build: `./build.sh Release`
5. Test: `./build/shapeshifter_tests "~[bench]"` (exclude benchmarks)
6. GitHub release: `build/shapeshifter` only; skip existing tags; propagate failures

**Key design decisions:**
- No `|| echo` fallback on release creation — explicit error propagation alerts on GitHub
- Artifact is game binary only (no test executables leaked)
- Insider workflow uses prerelease flag and `-insider` version suffix

**Validation:**
- YAML syntax: valid
- Workflow structure: both workflows follow identical pattern
- All gates properly sequenced
- Sanity checks passed: build command correct, test command excludes benchmarks, release artifact path correct, no stub/TODO markers

**Non-blocking notes:** None.

---

### User Model Directives (2026-04-26)

**Owners:** yashasg (via Copilot)  
**Status:** CAPTURED

1. **Default model:** claude-sonnet-4.6
2. **Exceptions:** Redfoot, Saul, Rabin, Edie → claude-opus-4.7
3. **Scribe, Ralph:** claude-haiku-4.5
4. **Issue creation:** During diagnostics, create an issue for whatever the team sees; do not skip based on perceived priority

---

### #125 — LowBar and HighBar Obstacles (Hard Difficulty Only)

**Owners:** Rabin (implementation), Baer (validation), Kujan (review)  
**Status:** APPROVED

LowBar and HighBar obstacles ship on hard difficulty only. Both are documented in `design-docs/game.md` (Swipe UP = jump = low_bar, Swipe DOWN = slide = high_bar) and fully runtime-supported (loader, scheduler, collision, burnout systems). Bars are confined to verse, pre-chorus, chorus, and drop sections.

**Difficulty ramp (post-merge):**
| Difficulty | Allowed obstacle kinds |
|------------|----------------------|
| easy | shape_gate |
| medium | shape_gate, lane_push |
| hard | shape_gate, lane_push, low_bar, high_bar |

**Implementation notes:**
- `tools/level_designer.py` now adds bars to `DIFFICULTY_KINDS["hard"]` and rotates variety through `{lane_push, low_bar, high_bar}` instead of always picking lane_push
- New post-clean pass `ensure_bar_coverage` guarantees ≥1 low_bar and ≥1 high_bar per hard beatmap
- All 3 hard beatmaps regenerated with coverage: stomper 1/3, drama 2/2, mental_corruption 7/7 (low_bar/high_bar)
- Easy/medium unchanged; zero bar contamination

**Validation:**
- Python: `python3 tools/check_bar_coverage.py` (exits 0 on success)
- C++: 10 new `[low_high_bar]` tests in `tests/test_beat_map_low_high_bars.cpp`
- All 2357 assertions pass; 751 total test cases

**Known limitation:** Bar placement is currently coverage-driven (guaranteed presence, not music-driven). Onkyo (audio tooling) identified as future improvement vector for musicality.

---

### #46 — Release Workflows (2026-04-26)

**Owners:** Ralph (implementation), Coordinator (validation), Kujan (review)  
**Status:** APPROVED

Push-to-main and insider-branch release workflows implemented and approved.

**Workflow structure (both `squad-release.yml` and `squad-insider-release.yml`):**
1. Checkout + cache build directory
2. Install Linux dependencies (libx11-dev, libxrandr-dev, libxinerama-dev, libxcursor-dev, libxi-dev, libxext-dev)
3. Set VCPKG_ROOT environment variable
4. Build: `./build.sh Release`
5. Test: `./build/shapeshifter_tests "~[bench]"` (exclude benchmarks)
6. GitHub release: `build/shapeshifter` only; skip existing tags; propagate failures

**Key design decisions:**
- No `|| echo` fallback on release creation — explicit error propagation alerts on GitHub
- Artifact is game binary only (no test executables leaked)
- Insider workflow uses prerelease flag and `-insider` version suffix

**Validation:**
- YAML syntax: valid
- Workflow structure: both workflows follow identical pattern
- All gates properly sequenced
- Sanity checks passed: build command correct, test command excludes benchmarks, release artifact path correct, no stub/TODO markers

**Non-blocking notes:** None.

---

### Data Ownership and Input Processing Rules (2026-05-15)

**Owners:** Keyser (diagnostics, recommendations), Development team (implementation)  
**Status:** DOCUMENTED

Diagnostics identified critical data ownership and input processing patterns requiring formalization.

**Data Ownership Rule for `PlayerShape::morph_t`:**
- **Rhythm mode:** `shape_window_system` owns `morph_t` exclusively (derives from `song_time`). `player_movement_system` must skip the morph update when `SongState::playing` is true.
- **Freeplay mode:** `player_movement_system` owns `morph_t` exclusively.

*Status: IMPLEMENTED in commit 7b420ed.*

**Input Processing Order Rule:**
`EventQueue` action events (GoEvents, ButtonPressEvents) must be consumed exactly once per logical frame. `player_input_system` now zeroes `eq.go_count` and `eq.press_count` after processing. This prevents the fixed-timestep accumulator loop from replaying the same events on sub-ticks.

*Status: IMPLEMENTED in commit 7b420ed.*

**MorphOut Shape Input Policy:**
Shape button presses during `WindowPhase::MorphOut` are accepted and interrupt the return-to-Hexagon morph, starting a fresh `MorphIn` for the pressed shape. Rationale: MorphOut has no active scoring window; interrupting it preserves player intent and matches the existing Active-interrupt pattern.

*Status: IMPLEMENTED in commit 7b420ed.*

**Findings Already Fixed (issues remain open — should be closed):**
The following were filed in diagnostics but are fixed in the current codebase:
- #108 (std::rand unseeded)
- #111 (session_log miss detection)
- #113 (audio SFX silence)
- #114 (total_notes = 0)
- #116 (reg.clear() signal UB)
- #117 (burnout lane-blind)
- #119 (ResumeMusicStream per-frame)

**Recommendation:** Ralph or Baer should verify these are truly fixed and close them (or note the exact commit).

---

### Agent Role Fit Assessment (2026-04-26)

**Owners:** Edie, Keyser  
**Status:** DOCUMENTED

Reviewed all 13 specialist agents against README and architecture docs. Assessment:

**High usefulness:**
- McManus (Gameplay), Hockney (Platform), Keaton (C++ Perf), Kobayashi (CI/CD), Fenster (Tools), Baer (Testing), Kujan (Review), Rabin (Level Design), Saul (Game Design), Edie (Product)

**Medium usefulness:**
- Verbal (QA edge cases), Redfoot (UI/UX)

**Infrastructure roles:**
- Scribe (Session Logger), Ralph (Work Monitor)

**Handoff clarity:**
1. QA vs Test: Verbal owns edge-case unit tests; Baer owns regression and platform validation
2. Rhythm tuning: McManus implements systems; Saul owns balance; Rabin owns per-song layouts. Heuristic changes route to Rabin first, Saul reviews.

**Gap identified:** No dedicated audio/music engineer. Fenster covers the Python pipeline side; deeper audio work (sync drift, onset detection refinements) has no specialist owner. Not blocking for current scope.

---

### #180/#182/#183/#184/#186 — iOS TestFlight Readiness (2026-04-26)

**Owners:** Hockney (platform implementation), Edie (product docs), Coordinator (validation)  
**Status:** PROPOSED — awaiting user-provided values

iOS build pipeline and TestFlight submission readiness defined.

**Audio Session (#180):**
- Category: `AVAudioSessionCategoryPlayback` (primary intentional audio; supports interruption callbacks)
- OS-driven interruptions trigger same pause state machine as #74 (manual pause)
- No auto-resume; `song_time` resumes from frozen value on player Resume

**App Lifecycle (#182):**
- `applicationWillResignActive` → Paused + `PauseMusicStream()`
- `applicationDidEnterBackground` → `StopMusicStream()` (releases audio device)
- `applicationDidBecomeActive` → Resume prompt + `PlayMusicStream()` + seek to frozen `song_time`
- Process kill by OS: run lost, high scores preserved (per #71)

**Version Scheme (#183):**
- `CFBundleShortVersionString`: SemVer from `CMakeLists.txt`
- `CFBundleVersion`: Monotonic integer from `app/ios/build_number.txt`
- Bump policy: build number on every TF upload, short version on feature milestones
- `CHANGELOG.md` required (Keep-a-Changelog format)
- Preflight script `tools/ios_preflight.sh` enforces build number bump vs last tag

**Bundle ID & Signing (#184):**
- Proposed bundle ID: `com.yashasg.shapeshifter` (user confirmation required)
- v1: Local Xcode Automatic Signing
- No capabilities for v1 (GameCenter, iCloud, Push, IAP deferred)

**Device Matrix (#186):**
- Minimum iOS: 16.0
- iPhone-only, portrait-only; iPad deferred
- 720×1280 logical viewport, centered uniform scaling, black letterbox
- 60fps cap (until #204 ProMotion resolution)
- UAT minimum: 3 devices (SE, notch, Dynamic Island)

**User-Provided Blockers (Blocking TestFlight Upload):**
1. Apple Developer Team ID (iOS Xcode build)
2. Bundle ID confirmation `com.yashasg.shapeshifter` (App ID registration)
3. Program type: individual or organization (documentation)
4. App icons (TestFlight upload)
5. Bump `app/ios/build_number.txt` 0 → 1 (first TF upload)

**Non-blocking notes:**
- iOS platform decisions documented in `docs/ios-testflight-readiness.md` (CMake generation, signing, device setup)
- `app/ios/build_number.txt` initialized to `0`

---

### EnTT ECS Audit Findings (2026-05-17)

**Scope:** Three-agent read-only audit of ECS compliance and C++/DoD performance patterns.

**Owners:** Keyser (compliance), Keaton (performance), Kujan (materiality review)  
**Status:** DOCUMENTED — remediation backlog created, all owners assigned

**Consolidated Material Findings:**

| Item | Category | File(s) | Owner | Est. | Priority |
|------|----------|---------|-------|------|----------|
| F1 | scoring_system removes iterated view components inside loop | scoring_system.cpp:33–152 | McManus | Low | MEDIUM |
| F2 | COLLISION_MARGIN tripled across 3 files | collision_system.cpp:16, beat_scheduler_system.cpp:25, test_player_system.cpp:319 | McManus/Fenster | Low | MEDIUM |
| F3 | APPROACH_DIST duplicated between headers | constants.h:70, song_state.h:72 | McManus/Fenster | Trivial | MEDIUM |
| F4 | System logic in component headers | input_events.h, obstacle_counter.h | McManus | Low-Med | LOW-MEDIUM |
| F5 | Render systems non-const registry parameter | game_render_system.cpp:153, ui_render_system.cpp:379 | McManus/Keaton | Trivial | LOW |
| F6 | Component structs with mutation methods | high_score.h, settings.h | McManus/Saul | Low | LOW |
| Rule 1 | emplace_or_replace → get_or_emplace per-frame | camera_system.cpp:373 | Keaton | Trivial | LOW |
| Rule 2 | Branching by component inside view loop | scoring_system.cpp:36 | McManus | Low | LOW |
| Rule 3 | Hoist ctx() lookups above loops | scoring_system.cpp, collision_system.cpp, miss_detection_system.cpp | McManus/Keaton | Low | LOW |

**In-flight validation:**
- Input dispatcher pipeline (PR #272, in-flight) is architecturally sound — no rework needed on structure
- Only placement of `ensure_active_tags_synced()` needs move (already in F4 above)

**Noise items (approved as-is, no action needed):**
- `cleanup_system.cpp:11` static buffer — intentional optimization, documented in #242
- No groups in codebase — correct choice given query cardinality
- `BurnoutState` stale fields — no UB risk, deferred to broader burnout review

**Compliant patterns noted (no action needed):**
- All systems are pure free functions
- Game state lives in registry context singletons
- Signal wiring/disconnection lifecycle-safe
- Tag/empty-type patterns used correctly
- No groups (correct for this workload)

**Remediation backlog:** 9 items total (3 trivial, 5 low, 1 low-medium effort). All owners assigned. Ready for team sprint prioritization.

**Key learnings for team:**
- ECS violations are allowed by EnTT but often represent latent traps (e.g., iterator invalidation)
- Constants triplication is a consistency risk that compounds over time
- Per-frame `emplace_or_replace` is an anti-pattern for stable entities (unnecessary signal dispatch)
- Component headers should contain only data definitions; all logic belongs in system `.cpp` files

**Decisions that follow from this audit:** Should be routed through Coordinator to McManus (primary) and Keaton (performance items) for sprint assignment.

---

## Governance

- All meaningful changes require team consensus
- Document architectural decisions here
- Keep history focused on work, decisions focused on direction

---

### Post-TestFlight Cleanup Findings (2026-05-16)

**Owners:** Keyser (ECS audit)  
**Status:** DOCUMENTED

**Fixed in this pass:**

| Bug | File | Fix |
|-----|------|-----|
| `BankedBurnout` stale on miss path | `scoring_system.cpp` | Add `remove<BankedBurnout>` to miss branch |
| `SongResults::total_notes` always 0 (#114) | `play_session.cpp` | Set `total_notes = beatmap.beats.size()` after reset |
| `SongState` ctx lookup inside entity loop | `player_movement_system.cpp` | Moved outside loop |
| Duplicated Left/Right lane change blocks | `player_input_system.cpp` | Unified with `int8_t delta` pattern |

**Deferred Material Bugs (not safe for cleanup scope):**

1. **`cleanup_system` miss gap** — Unscored obstacles that scroll past `DESTROY_Y` are silently destroyed without energy penalty. `test_death_model_unified` ("cleanup miss drains energy") proves this is incorrect. Fix: emplace `MissTag`+`ScoredTag` in `cleanup_system` for unscored entities at `DESTROY_Y`, let `scoring_system` handle the penalty. Requires Saul sign-off on energy balance impact before merge.

2. **`test_high_score_integration` SIGABRT** — EnTT `dense_map` assertion crash at `scores["key"] = value` in test fixture. Root cause: `ankerl::unordered_dense::map::operator[]` throws on missing key in non-const context. Test needs `scores.emplace(key, value)` or map pre-initialization. Assign to Baer.

3. **Collision timing window tests** — `test_collision_system.cpp:308,340,341` fail on timing window contraction behavior. "Perfect timing" test expects `window_scale > 1.0` (extension) but collision_system sets 0.5 (contraction). Semantics mismatch between test and implementation. Assign to McManus to adjudicate which behavior is spec-correct.

---

## Review: EnTT Dispatcher Input Model (2026-04-27)

**Reviewer:** Kujan  
**Verdict:** APPROVED  
**Date:** 2026-04-27  
**Commits:** 2547830 (Keaton implementation), bd6ff37 (history)

### Test Results

- **2419 assertions pass**
- **768 test cases pass**
- **Zero build warnings**

### Architecture Review

**Acceptance Criteria Met:**

GoEvent and ButtonPressEvent are now fully delivered via `entt::dispatcher` stored in `reg.ctx()`. The hand-rolled `go_count`/`press_count`/`go_events[]`/`press_events[]` arrays are removed from EventQueue. The manual `eq.go_count = 0` anti-pattern that caused the #213 replay bug is eliminated — drain semantics of `disp.update<T>()` are the replacement.

**Architecture Actually Shipped (differs from decisions.md Tier-1 spec):**

`decisions.md` specified: input_system enqueues InputEvent → `disp.update<InputEvent>()` fires gesture_routing and hit_test as listeners.

What shipped: gesture_routing_system and hit_test_system remain direct system calls (not listeners). They read the raw EventQueue (touch/mouse gesture buffer) and call `disp.enqueue<GoEvent/ButtonPressEvent>`. The dispatcher drains in the first fixed sub-tick via `game_state_system → disp.update<GoEvent/ButtonPressEvent>()`, firing all three listener chains (game_state, level_select, player_input handlers) atomically.

**Rationale:** This is architecturally equivalent for the accepted acceptance criteria. No pool-order latency hazard applies because gesture_routing and hit_test are not inside a `disp.update()` chain. Same-frame behavior is preserved.

**EventQueue not fully removed:** EventQueue struct is retained as a raw gesture shuttle (InputEvent[] only). Decisions.md migration step 4 ("Remove EventQueue struct") is NOT done. The raw-input layer (EventQueue) and semantic-event layer (dispatcher) remain separate. This is acceptable — the acceptance criteria scoped the migration to "Go/ButtonPress event delivery where intended."

### Guardrails for Future Dispatcher Work

1. **No start-of-frame `disp.clear<GoEvent/ButtonPressEvent>()`** — R7 from decisions.md is not explicitly addressed for the dispatcher queues. In practice, game_state_system drains within the same frame (it's the first fixed-tick system). If a frame skips the fixed tick, events accumulate until the next tick — currently benign but worth hardening.

2. **Redundant `disp.update()` calls are intentional** — game_state_system, level_select_system, and player_input_system all call `disp.update<GoEvent/ButtonPressEvent>()`. Only the first call per tick delivers events; subsequent calls are no-ops. The player_input_system redundancy is required for isolated unit tests that call it directly (test_player_input_rhythm_extended).

3. **Contract test comments are now stale** — `test_entt_dispatcher_contract.cpp` says "gesture_routing must use trigger(), not enqueue()". This was written before Keaton's approach was chosen. The actual implementation avoids the latency problem differently (direct system call, not listener). The comment should be updated to not mislead future readers.

4. **R4 — registry reference lifetime** — `disp.sink<GoEvent>().connect<&handler>(reg)` stores a pointer to `reg`. Safe because `unwire_input_dispatcher` disconnects before `reg.clear()`. Future changes that omit the unwire step would introduce a dangling pointer.

## Decision: EnTT-safe iteration pattern for scoring_system (#315)

**Author:** McManus  
**Date:** 2026-05-17  
**Issue:** #315

### Decision

Scoring_system now uses **collect-then-remove** across two structural views.
All `reg.remove<>` calls on view components (Obstacle, ScoredTag, MissTag)
are deferred until after the view iterator is exhausted.

### Pattern (reusable)

```cpp
// Read pass — collect into static buffer
static std::vector<Record> buf;
buf.clear();
auto view = reg.view<A, B, C>();
for (auto [e, a, b, c] : view.each()) {
    buf.push_back({e, /* copies of needed data */});
}
// Mutation pass — safe: view is exhausted
for (auto& r : buf) {
    reg.remove<A>(r.e);   // A is a view component — would be UB mid-loop
    reg.remove<B>(r.e);
}
```

### Structural view split

The old `any_of<MissTag>` per-entity branch is replaced by two structural views:
- Miss: `reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>()`
- Hit: `reg.view<ObstacleTag, ScoredTag, Obstacle, Position>(entt::exclude<MissTag>)`

This matches the `collision_system` pattern and gives EnTT better cardinality
hints for pool iteration.

### Impact on other systems

Any system that removes components present in its own active view should apply
this pattern. Known candidates from the ECS audit (F1 was scoring_system —
now fixed). Collision_system already tags-only (emplace) and does not remove
during iteration — compliant.

---

## Parallel ECS/EnTT Audit Findings (2026-04-27)

**Agents:** Keyser, Keaton, McManus, Redfoot, Baer  
**Branch:** user/yashasg/ecs_refactor  
**Status:** AUDIT COMPLETE — Findings documented for prioritization

### P1 Findings

**Keyser — sfx_bank.cpp: Raw registry pointer in ctx singleton**

- **File:** `app/systems/sfx_bank.cpp:149`
- **Issue:** `reg.ctx().emplace<SFXPlaybackBackend>(SFXPlaybackBackend{play_sfx_from_bank, &reg})`
- **Risk:** Storing `&reg` inside ctx singleton couples lifetime to registry address stability. Safe today (stack-local, synchronous), breaks if invocation becomes async or registry moves.
- **Fix:** Remove `user_data = &reg`; pass `reg` directly at invocation time in `audio_system()`.
- **Owner:** Keaton

**McManus — obstacle_counter.h: Signal wiring in component header**

- **File:** `app/components/obstacle_counter.h:29-35`
- **Issue:** `inline void wire_obstacle_counter(entt::registry& reg)` performs signal registration (system concern) in a component header.
- **EnTT principle:** Signal wiring is system logic, not component data.
- **Fix:** Move `wire_obstacle_counter()` and its two listener functions to new `app/systems/obstacle_counter_system.cpp`. Keep `ObstacleCounter` data struct in header.
- **Owner:** McManus

**Redfoot — overlay_render: Dead code + hot-path JSON violation**

- **File:** `app/systems/ui_render_system.cpp:350–358`
- **Issue:** Checks `ovr.contains("overlay_color")` (flat key) but paused.json stores `overlay.color` (nested). Condition always false — pause dim overlay never rendered. Compounds hot-path JSON access (#322 policy violation).
- **Fix:** Extract overlay color at screen-load time into ctx POD (`OverlayLayout` struct). Call `extract_overlay_color(ui.overlay_screen)` in `ui_navigation_system` at `ui_load_overlay`. Replace JSON traversal in render with ctx read.
- **Bonus:** Fixes silent visual regression (pause dim not rendering).
- **Owner:** McManus

### P2 Findings

**Keyser — high_score.h: Mutation methods in component**

- **File:** `app/components/high_score.h:71–110`
- **Methods:** `set_score()`, `set_score_by_hash()`, `ensure_entry()`
- **EnTT principle:** Components are data; mutations belong in systems or free functions.
- **Context:** Commit `fdcd709` (#318) moved logic out of systems; next step is to move methods out of struct into `high_score_persistence.cpp` (or new `high_score_ops.h`) as free functions taking `HighScoreState&`.
- **Not blocking:** `HighScoreState` is ctx singleton with correct logic; this is purity cleanup.
- **Owner:** Keaton

**Keaton — TestPlayerAction.done_flags: Hand-rolled bitmask**

- **File:** `app/components/test_player.h:39-47`
- **Issue:** `uint8_t done_flags` with six manual getters/setters using raw hex masks.
- **Fix:** Use `enum class ActionDoneBit : uint8_t { ShapeDone=1, LaneDone=2, VerticalDone=4, _entt_enum_as_bitmask }` — consistent with `GamePhaseBit` pattern.
- **Owner:** Keaton or any C++ contributor

**McManus — EventQueue Tier-1: Incomplete dispatcher migration**

- **Files:** `app/components/input_events.h`, `app/systems/input_system.cpp`, `app/systems/gesture_routing_system.cpp`, `app/systems/hit_test_system.cpp`
- **Issue:** decisions.md specifies full migration to `entt::dispatcher` with step 4 = "Remove EventQueue struct". Tier 2 (GoEvent, ButtonPressEvent) is fully migrated and wired in `input_dispatcher.cpp`. Tier 1 remains incomplete: `input_system` still pushes raw inputs to `EventQueue`, and `gesture_routing_system`/`hit_test_system` read from `EventQueue` before forwarding.
- **Fix:** Complete Tier 1: Replace `EventQueue::inputs` with `disp.enqueue<InputEvent>()` in `input_system`. Update `gesture_routing_system`/`hit_test_system` to receive `InputEvent` via dispatcher. Remove `EventQueue` struct.
- **Owner:** Keaton (per decisions.md ownership)

**Baer — entt::dispatcher not asserted in singleton coverage test**

- **File:** `tests/test_components.cpp`
- **Issue:** `make_registry()` test asserts only 6 of ~17 singletons. `entt::dispatcher` is NOT listed. Any test that constructs bare `entt::registry` and calls `gesture_routing_system`, `hit_test_system`, `player_input_system`, etc., will hard-crash without diagnostic.
- **Fix:** Extend singleton test to call `reg.ctx().get<T>()` for every singleton added by `make_registry()`. Add a null-registry crash-guard test for at least one ctx-dependent system.
- **Owner:** Baer

### P3 Findings

**Keyser — input_events.h: EventQueue member methods (deferred to Tier-1 migration)**

- **File:** `app/components/input_events.h:38–46`
- **Methods:** `push_input()`, `clear()`
- **Issue:** Deviates from team convention — `AudioQueue` uses free-function pattern (`audio_push`, `audio_clear`).
- **Resolution:** Resolved by Tier-1 EventQueue migration (McManus P2 finding).
- **Owner:** Keaton (as part of dispatcher migration)

**Keaton — TestPlayerState.planned[]: Raw entity array**

- **File:** `app/components/test_player.h:85`
- **Issue:** `entt::entity planned[MAX_PLANNED]` stores raw obstacle entity IDs with no validation at storage time. Use sites apply `.valid()` guards.
- **Fix:** Consider wrapping in version-tagged weak ref or documenting "caller must validate" contract explicitly.
- **Owner:** Keaton

**Keaton — UIState.current: String vs hashed dispatch**

- **File:** `app/components/ui_state.h:22`
- **Issue:** `std::string current` used for screen-change comparisons while `element_map` uses `entt::id_type` (hashed).
- **Note:** Minor inconsistency. Could use `entt::hashed_string` for transitions.
- **Priority:** Low.
- **Owner:** Low priority; any contributor.

**Redfoot — spawn_ui_elements: JSON operator[] without exception guard**

- **File:** `app/systems/ui_navigation_system.cpp` (spawn_ui_elements, lines ~107–141)
- **Issue:** `el["animation"]["speed"].get<float>()` uses `operator[]` on const references. Per SKILL guideline, `operator[]` on const nlohmann::json triggers `assert()` on missing key; use `.at()` inside `try/catch` instead.
- **Note:** Not hot path (screen-load time), but malformed animation block would crash.
- **Fix:** Replace `el["animation"]["speed"]` with `el.at("animation").at("speed")` inside try/catch.
- **Owner:** McManus or Keaton.

**Redfoot — UIActiveCache: Lazy emplacement during game loop**

- **File:** `app/systems/active_tag_system.cpp:20-21`
- **Issue:** Fallback `emplace` fires on first call to `ensure_active_tags_synced()` during game loop. Not a crash risk (idempotent after), but registry mutation during gameplay is code smell.
- **Fix:** Initialize `UIActiveCache` alongside other ctx singletons in `game_loop_init`.
- **Owner:** McManus (low effort, follows existing pattern).

### Coverage Gaps (Baer)

**P1 — R7 Stale event discard not tested**

- **Guarantee:** Events queued before phase transition must be discarded, not replayed after transition.
- **Gap:** No test exists. `test_input_pipeline_behavior.cpp` covers #213 (same-frame drain) but not cross-phase accumulation.
- **Risk:** Events enqueued in Playing → GameOver frame survive in dispatcher; if next frame re-enters Playing, events fire into fresh player state.
- **Test:** Enqueue GoEvent → trigger phase transition → run `game_state_system` → verify no GoEvent delivered to post-transition player state.
- **Owner:** Baer (explicitly assigned in R7)

**P1 — Dispatcher singleton coverage**

- **Guarantee:** `entt::dispatcher` in ctx required by 8 production systems; absence is hard UB/crash.
- **Gap:** `test_components.cpp` asserts only 6 of ~17 singletons; dispatcher not checked. See P2 finding above.
- **Owner:** Baer

**P2 — miss_detection_system emplace idempotency**

- **Guarantee:** collect-then-remove is safe pattern for view-time destruction.
- **Gap:** `miss_detection_system` emplaces `ScoredTag` and `MissTag` during `exclude<ScoredTag>` view iteration. No test verifies (a) each entity gets exactly one tag, (b) no entity skipped, (c) no entity double-tagged.
- **Test:** Run `miss_detection_system` on N obstacles with all past DESTROY_Y; verify MissTag count == N and ScoredTag count == N; run again, verify no second tag.
- **Owner:** Baer

**P2 — make_registry() singleton completeness**

- **Guarantee:** `make_registry()` is canonical init contract; all singletons must be present.
- **Gap:** ~11 singletons (SongState, EnergyState, BeatMap, etc.) emplaced but not checked. Adding singleton and forgetting it would silently crash.
- **Test:** Extend singleton test to call `reg.ctx().get<T>()` for every singleton, or use `find<T>() != nullptr` to report missing by name.
- **Owner:** Baer

**P3 — on_construct<> signal tests platform-gated**

- **Guarantee:** on_construct<ObstacleTag> / on_construct<ScoredTag> signal lifecycle: connect once, no double-connect.
- **Gap:** Tests entirely within `#ifdef PLATFORM_DESKTOP` — invisible on Linux CI. No test for double-connect risk.
- **Test:** Port at least one `on_construct<ObstacleTag>` signal test to non-platform-gated file.
- **Owner:** Baer

**P3 — Component purity contract test**

- **Guarantee:** Components are plain data, no business logic.
- **Gap:** No contract test asserting purity boundary. Methods tested functionally, but no test would catch future addition of external-call side effects.
- **Note:** Low urgency for existing code. Flag for future refactor.
- **Owner:** No immediate test needed — design note for Keyser/McManus.

### SKILL File Correction (Keaton)

**File:** `.squad/skills/ui-json-to-pod-layout/SKILL.md`

**Error:** States "EnTT v3 context API: insert_or_assign (NOT emplace). emplace is deprecated in v3."

**Correction:** In EnTT v3.16.0, `ctx().emplace<T>()` is NOT deprecated. It uses `try_emplace` internally (insert-if-absent, idempotent).

**Correct guidance:**
- Use `emplace<T>()` for first-time-only insertion (startup context init)
- Use `insert_or_assign(value)` when you need to replace existing value (session restart)

**Code status:** Mixed usage in `game_loop.cpp` (emplace) and `play_session.cpp` (insert_or_assign) is CORRECT. No code change needed; update SKILL doc only.

**Owner:** Keaton or doc maintainer.

---

*Audit completed 2026-04-27T22:30:13Z. All findings documented for prioritization.*

---

## Inbox Merges (2026-04-27)


### baer-324-burnout-regression

# Baer Decision: #324 Burnout Regression Pass

**Date:** 2026-04-27
**Branch:** squad/324-remove-dead-burnout-surface
**Commit:** 6ee912a

## Finding

The `d9be464` migration of the LanePush exclusion test (from deleted `test_burnout_bank_on_action.cpp`) only covered `LanePushLeft`. `LanePushRight` was silently unguarded for the scoring-exclusion path.

## Action Taken

Added `TEST_CASE("scoring: LanePushRight excluded from chain and popup", "[scoring][lane_push]")` to `tests/test_scoring_extended.cpp`. This closes the gap introduced by the burnout deletion without modifying production code.

## Convention Established

When a scoring or chain-exclusion branch covers two symmetric variants (Left/Right, A/B), both variants must have independent test cases. A single test covering one side is insufficient for regression guarding.

## Status

2565 assertions, 792 test cases — all pass. Zero warnings. Branch is ready for Kujan's final gate.

### baer-archetypes-folder-validation

# Archetypes Folder Move — Validation Complete

**Author:** Baer (Test Engineer)
**Date:** 2026-04-27
**Status:** VALIDATED — No source changes made; validation only.

## Decision

The in-progress move of `app/systems/obstacle_archetypes.{h,cpp}` → `app/archetypes/obstacle_archetypes.{h,cpp}` is structurally sound and ready to commit.

## Evidence

- **CMake configure:** clean (`cmake -B build -S . -Wno-dev`)
- **Build:** `cmake --build build --target shapeshifter_tests` — zero project warnings; one pre-existing `ld: warning: ignoring duplicate libraries: 'vcpkg_installed/arm64-osx/lib/libraylib.a'` linker note unrelated to this move
- **Archetype tests:** `./build/shapeshifter_tests "[archetype]"` — **11 tests, 64 assertions, all pass**
- **Full suite:** `./build/shapeshifter_tests "~[bench]"` — **810 test cases, 2748 assertions, all pass**

## Key Structural Facts

- `shapeshifter_lib` include root is `${CMAKE_CURRENT_SOURCE_DIR}/app`; both `beat_scheduler_system.cpp` and `obstacle_spawn_system.cpp` use `"archetypes/obstacle_archetypes.h"` which resolves correctly to `app/archetypes/`
- `shapeshifter_tests` include root is also `${CMAKE_CURRENT_SOURCE_DIR}/app`; `test_obstacle_archetypes.cpp` resolves identically
- `ARCHETYPE_SOURCES` glob at CMakeLists.txt:90 picks up `app/archetypes/*.cpp` and is compiled into `shapeshifter_lib` — single compiled implementation shared by exe, tests, and any future consumers

## Not Started

P2 (miss_detection idempotency test) and P3 (on_construct platform-gate test) gaps from the ECS audit are **not addressed here**. These require source file modifications and are outside the scope of this validation task.

### baer-beat-log-system

# Decision: beat_log_system placement and state ownership (#283)

**Date:** 2026-04-27  
**Author:** Baer  
**Status:** IMPLEMENTED (commit c2720ea)

## Decision

Beat logging is extracted from `song_playback_system` into a new `beat_log_system` that lives in `session_logger.cpp`. The system follows the `ring_zone_log_system` precedent exactly.

## State ownership

`SessionLog::last_logged_beat` (int, default -1) tracks the highest beat index already emitted to the log file. This field belongs to logging, not playback. `song_playback_system` never touches it.

## Execution order

`beat_log_system` runs immediately after `song_playback_system` in the fixed-tick loop, before `beat_scheduler_system`. This ensures beat log entries appear before any scheduler action triggered by that beat.

## Test seam

`SessionLog` stored in `reg.ctx()`. Tests open a `/dev/null`-backed file so no disk I/O occurs. Nine `[beat_log]` Catch2 tests cover: no-op paths (absent log, null file, wrong phase, song not playing), forward advance, multi-beat catch-up, no-re-log, and the critical separation proof (playback does not advance last_logged_beat).

### baer-magic-enum-tests

# Decision: SFX Enum Test Contract After magic_enum Refactor

**Author:** Baer  
**Date:** 2026-04-26  
**Status:** RECOMMENDED — no approval needed (test-only change)

---

## Context

Keaton's magic_enum refactor removed the `SFX::COUNT` sentinel from the `SFX` enum.  
The `test_audio_system.cpp` file was previously excluded from the build via a CMakeLists regex.  
The queue-capacity test used `static_cast<SFX>(i % static_cast<int>(SFX::COUNT))` — now invalid.

---

## Decision

**`test_audio_system.cpp` is re-enabled in the build.** The `kAllSfx[]` explicit array pattern replaces the COUNT-based cycle.

### Pattern (in `test_audio_system.cpp`)

```cpp
constexpr SFX kAllSfx[] = {
    SFX::ShapeShift, SFX::BurnoutBank, SFX::Crash,    SFX::UITap,
    SFX::ChainBonus, SFX::ZoneRisky,   SFX::ZoneDanger, SFX::ZoneDead,
    SFX::ScorePopup, SFX::GameStart,
};
constexpr int kSfxCount = static_cast<int>(sizeof(kAllSfx) / sizeof(kAllSfx[0]));

static_assert(static_cast<int>(SFX::ShapeShift) == 0,
              "SFX must be zero-based (ShapeShift must equal 0)");
static_assert(static_cast<int>(magic_enum::enum_count<SFX>()) == kSfxCount,
              "SFX enum count mismatch — update kAllSfx when adding/removing SFX values");
```

### Rationale

| Old approach | New approach |
|---|---|
| `static_cast<SFX>(i % SFX::COUNT)` — COUNT is a sentinel, can produce COUNT as a value if loop overruns | `kAllSfx[i % kSfxCount]` — only real enum values used |
| No sync guard between test and enum | `static_assert(enum_count == kSfxCount)` — build fails if array goes stale |
| Relied on COUNT being last enumerator | Independent of sentinel existence |

---

## Scope of Change

- `CMakeLists.txt` — removed `test_audio_system` from EXCLUDE regex  
- `tests/test_audio_system.cpp` — contiguity guards + COUNT-free cycle  
- `tests/test_helpers_and_functions.cpp` — added `LanePushLeft`/`LanePushRight` to ObstacleKind ToString test

---

## Convention for Future SFX Additions

When a new `SFX` enumerator is added:

1. Add it to `kAllSfx[]` in `test_audio_system.cpp`
2. Add its spec to `SFX_SPECS` in `sfx_bank.cpp`
3. Update the `enum_count<SFX>() == N` guard in `audio.h`

The build will fail at static_assert if any of these are missed.

### baer-pr43-review-regressions

# Decision: PR #43 — Regression tests verified, 10 threads resolved

**Author:** Baer  
**Date:** 2026-04-27  
**Status:** VERIFIED

## Summary

All five C++ review themes from PR #43 have deterministic regression tests that pass.
10 GitHub review threads were marked resolved.

## Tests covering each theme

| Theme | File | Tests | Tag |
|-------|------|-------|-----|
| ScreenTransform stale before input | test_pr43_regression.cpp | 2 | [screen_transform][pr43] |
| Slab Y/Z dimension contract | test_pr43_regression.cpp | 3 | [camera][slab][pr43] |
| Level select same-tick hitbox reposition | test_level_select_system.cpp | 3 | [level_select][pr43] |
| on_obstacle_destroy parent-specific | test_pr43_regression.cpp | 2 | [obstacle][cleanup][pr43] |
| Paused overlay parsed once | test_pr43_regression.cpp | 1 | [ui][overlay][pr43] |

**Validation command:**
```
./build/shapeshifter_tests "[pr43]" "[level_select]"
```

## Threads NOT resolved (out of scope)

- Workflow template threads → Hockney
- squad/team.md thread → coordinator
- Outdated threads (auto-dismissed when diff changes)

### baer-pr43-review-tests

# Decision: PR #43 regression test suite + pool-priming fix

**Author:** Baer  
**Date:** 2026-04-26  
**Commit:** 31bc2d8

## Summary

Added regression tests for all 6 PR #43 review themes and fixed a real production bug discovered during investigation.

## New test files

- `tests/test_pr43_regression.cpp` — 14 test cases covering all 6 themes
- 3 new test cases appended to `tests/test_level_select_system.cpp`

## Bug fixed: ObstacleChildren pool ordering (game_loop.cpp)

### Problem

`entt::registry::destroy(entity)` iterates component pools in **reverse insertion order**. In production:
1. `game_loop_init` connects `on_destroy<ObstacleTag>` → ObstacleTag pool inserted at index 0
2. First obstacle spawn calls `add_slab_child` / `add_shape_child` → ObstacleChildren pool inserted at a higher index

When `cleanup_system` calls `reg.destroy(obstacle)`, reverse iteration removes ObstacleChildren first. By the time `on_obstacle_destroy` fires (for ObstacleTag), `try_get<ObstacleChildren>(parent)` returns null → children are silently NOT destroyed.

Consequence: orphaned MeshChild entities accumulate in the registry. `camera_system` then calls `reg.get<Position>(mc.parent)` on a destroyed (invalid) entity — **undefined behavior**.

### Fix

In `app/game_loop.cpp`, added `reg.storage<ObstacleChildren>();` before `reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();`. This primes the ObstacleChildren pool at a lower index so it is processed **last** in reverse iteration — still alive when the signal fires.

### For team

This is an EnTT footgun: any time you use `on_destroy<TagA>` to read component B from the same entity, component B's pool must be primed (via `reg.storage<B>()`) **before** the `on_destroy<TagA>` connection, or B will already be gone by the time the signal fires.

## Test setup pattern

`make_obs_registry()` (in test_pr43_regression.cpp) documents the correct setup:
```cpp
static entt::registry make_obs_registry() {
    auto reg = make_registry();
    reg.storage<ObstacleChildren>();   // prime BEFORE connecting signal
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();
    return reg;
}
```

### baer-song-complete-score-tests

# Decision: Song-Complete Score/High-Score Render Regression Tests

**Date:** 2026-05-27  
**Author:** Baer (Test Engineer)  
**Triggered by:** Bug report — "when the song completes score and highscore dont render"

## Coverage gaps identified

Existing tests verified `score.high_score` is updated on transition and that all JSON sources *resolve* (with zero values). They did NOT pin:

1. That `score.score` is **retained** (not zeroed) when `enter_song_complete` fires.
2. That the sources resolve to **non-empty, correct strings** using non-zero gameplay values.
3. That `song_complete.json` binds to the **exact** source strings `"ScoreState.score"` and `"ScoreState.high_score"` (not `displayed_score` or a wrong key).

## Tests added

### `tests/test_game_state_extended.cpp` (3 new cases, tag `[song_complete]`)

| Test | Guards against |
|------|---------------|
| `score.score is retained (not zeroed) after enter_song_complete` | `enter_song_complete` accidentally resetting `ScoreState` |
| `both score and high_score resolve to non-empty strings after transition` | Resolver returning nullopt or wrong value when score > high_score |
| `score is visible even when it does not set a new high score` | Resolver silenced when score < existing high_score |

### `tests/test_ui_source_resolver.cpp` (3 new cases, tag `[song_complete]`)

| Test | Guards against |
|------|---------------|
| `song_complete.json: score element declares source ScoreState.score` | JSON source field rename or typo |
| `song_complete.json: high_score element declares source ScoreState.high_score` | JSON source field rename or typo |
| `ScoreState.score source formats to decimal string` | Resolver returning empty/nullopt for non-zero values |

## Render-path gap (open, for McManus/Redfoot)

The `draw_hud` function renders score/high_score via `find_el` + `text_draw_number` only for `Gameplay` and `Paused` phases. For `SongComplete` the ECS `UIText`/`UIDynamicText` path is used instead (correct). However, the dynamic text elements in `song_complete.json` do **not** set `"align": "center"` — the text spawns left-aligned at `x_n: 0.5`, which may cause visual offset. This is a layout concern, not a data concern; it is outside scope for these regression tests.

### copilot-directive-2026-04-26T22-06-enum-macro

### 2026-04-26T22:06:19-07:00: User directive
**By:** yashasg (via Copilot)
**What:** The enum refactor should not use the current X-macro list pattern because it is too cumbersome; use a macro where the enum name is an input.
**Why:** User request — captured for team memory and implementation constraints.

### copilot-directive-2026-04-26T22-20-burnout-removal

### 2026-04-26T22:20:24-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Remove the burnout system altogether because it gets in the way of staying-on-beat gameplay; changing shape on beat should not be penalized even when it is not close to obstacle arrival.
**Why:** User request — current burnout behavior is breaking functionality and docs must be updated.

### copilot-directive-2026-04-27T00-04-33-dod-audit

### 2026-04-27T00:04:33-07:00: User directive
**By:** yashasg (via Copilot)
**What:** For the DoD pass, use multiple DoD subagents, create GitHub issues for every surfaced issue, and treat no issue as too small to be fixed.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T00-04-34-dod-loop-tests

### 2026-04-27T00:04:34-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Keep looping with DoD passes using subagents until no new issues come up; once clear, add new tests and improve coverage/edge-case testing.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T00-04-35-no-skip-dod

### 2026-04-27T00:04:35-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Do not skip any system or component in the DoD audit.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T00-52-58-system-boundaries

### 2026-04-27T00:52:58-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Systems should not handle responsibilities outside their purpose; creating new focused systems is acceptable when it improves clean, readable code.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T02-36-17-0700

### 2026-04-27T02:36:17-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Prioritize DoD-related TestFlight issues first, then run a cleanup/readability pass.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T02-40-40-0700

### 2026-04-27T02:40:40-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Run multiple DoD fixes in parallel; most systems should function independently, so parallel ownership should be used where file/system boundaries do not conflict.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T02-40-41-0700

### 2026-04-27T02:40:41-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Once DoD-related issues are resolved, go through the remaining ECS refactor issues and chip away at those next.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T02-46-00-0700

### 2026-04-27T02:46:00-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Run up to 32 agents in parallel for independent DoD/ECS fixes; do not hold back on parallel subagent fixes when systems are file/system independent.
**Why:** User request — captured for team memory

### copilot-directive-2026-04-27T12-04-35-0700-entt-input-model

### 2026-04-27T12:04:35-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Use EnTT's model for the input implementation; move away from the custom hand-rolled input event queue where EnTT dispatcher/signal patterns apply.
**Why:** User request after comparing current input pipeline against `docs/entt/EnTT_In_Action.md`.

### copilot-directive-20260427T162547-ecs-refactor-testflight

### 2026-04-27T16:25:47-07:00: User directive
**By:** yashasg (via Copilot)
**What:** First create the P0 `ecs_refactor` TestFlight issue for auditing/consolidating dead or duplicate ECS systems/components; validate and review the existing archetype move to `app/archetypes/`; then stop and report status before starting any P2/P3 fixes.
**Why:** User request — captured for team memory

### copilot-directive-20260427T211713Z-lane-push-removal

### 2026-04-27T20:52:52Z: User directive
**By:** yashasg (via Copilot)
**What:** Remove the LanePush feature for now; it can be added back later if needed. This should be tracked as a GitHub issue but does not need to be addressed in the ECS refactor. LanePush touches the editor, level builder, and game, so it is a large cross-system change and must be addressed that way.
**Why:** User request — captured for team memory

### copilot-directive-20260427T212049Z-ecs-refactor-source

### 2026-04-27T21:17:52Z: User directive
**By:** yashasg (via Copilot)
**What:** Once fixes are rebased/integrated, clean up old branches and maintain `user/yashasg/ecs_refactor` as the source of truth.
**Why:** User request — captured for team memory

### copilot-directive-20260427T231658Z-archetypes-folder

### 2026-04-27T23:16:58Z: User directive
**By:** yashasg (via Copilot)
**What:** Archetypes should live in their own folder at `app/archetypes`.
**Why:** User request — captured for team memory

### hockney-magic-enum-commit-gap

# Decision: magic_enum Build Wiring Must Co-Land With Include Usage

**Author:** Hockney  
**Date:** 2026-05  
**Status:** RESOLVED — commit `72c83b5`

## Context

Kujan rejected the combined batch due to commit `8fbce9c` introducing `#include <magic_enum/magic_enum.hpp>` across the codebase while the corresponding `vcpkg.json` and `CMakeLists.txt` wiring was left as working-tree-only changes. Fresh checkout of HEAD could not build.

## Decision

Created follow-up commit `72c83b5` (branch `user/yashasg/ecs_refactor`) containing only:

| File | Change |
|------|--------|
| `vcpkg.json` | `"magic-enum"` added to dependencies |
| `CMakeLists.txt` | `find_package(magic_enum CONFIG REQUIRED)`, SYSTEM deps loop entry, PUBLIC link on `shapeshifter_lib` |

Did **not** amend existing commits (reviewer protocol).

## Team Rule Established

Any commit that introduces `#include` headers from a new third-party library must simultaneously commit:
1. `vcpkg.json` — package name in dependencies array
2. `CMakeLists.txt` — `find_package`, SYSTEM deps loop, `target_link_libraries`

Leaving dependency wiring as working-tree-only while committing include usage creates a broken HEAD that fails fresh checkout. This is a build-integrity regression regardless of whether local artifacts mask it.

## Validated

`cmake -Wno-dev -B build -S .` on HEAD `72c83b5` → `Configuring done (0.7s)`, exit 0.

### hockney-pr43-template-comments

# Decision: PR #43 Template Workflow Fixes

**Author:** Hockney  
**Date:** 2026-05  
**Status:** IMPLEMENTED — pending coordinator merge

## Summary

Five unresolved review threads on PR #43 addressed in `.squad/templates/workflows/`. All threads resolved on the PR.

## Decisions Made

### 1. `.squad/` ship-gate narrowed to stateful subdirs

**File:** `.squad/templates/workflows/squad-preview.yml`  
**Decision:** The `Check no AI-team state files are tracked` step now checks specific stateful subdirectories (`.squad/agents/`, `.squad/decisions/`, `.squad/identity/`, `.squad/log/`, `.squad/orchestration-log/`, `.squad/sessions/`, `.squad/casting/`) instead of the entire `.squad/` tree. `.squad/templates/**` is explicitly permitted to be tracked.  
**Rationale:** Broad `.squad/` check broke any repo that legitimately tracks squad templates (like this one). Stateful state must not ship; template content can.

### 2. TEMPLATE header added to all three workflow templates

**Files:** `squad-ci.yml`, `squad-preview.yml` (implicitly), `squad-docs.yml`  
**Decision:** Added `# TEMPLATE — ...` comment block at the top of `squad-ci.yml` (the most likely to be confused with a live workflow) explaining that GitHub Actions does not execute files under `.squad/templates/workflows/` and they must be copied to `.github/workflows/`.  
**Rationale:** GitHub Actions' discovery is strictly `.github/workflows/` scoped. Without the comment, a new contributor could expect these templates to run automatically.

### 3. squad-docs.yml path filter — clarify deployment target

**File:** `.squad/templates/workflows/squad-docs.yml`  
**Decision:** Path filter `'.github/workflows/squad-docs.yml'` is CORRECT for the deployed workflow; it refers to the target path after the template is copied. Added inline TEMPLATE NOTE clarifying this so authors do not "fix" a working path.  
**Rationale:** The path is an intentional forward-reference to the deployed filename. The confusion arises only when reading the template in its source location.

### 4. squad-preview.yml package.json/CHANGELOG.md expectations explicit

**File:** `.squad/templates/workflows/squad-preview.yml`  
**Decision:** Added explicit file-existence checks for `package.json` and `CHANGELOG.md` with targeted `::error::` messages, plus a comment documenting the expected CHANGELOG format (`## [x.y.z]` — Keep a Changelog / standard-version).  
**Rationale:** Silent failure mode (`grep -q ... 2>/dev/null`) was unhelpful for repos missing these files. Fail fast with actionable messages.

### 5. team.md PR mismatch — no-op

**File:** `.squad/team.md`  
**Decision:** No change required. The reviewer artifact is a transient GitHub diff comment from the PR including `.squad/` files alongside a large C++ refactor. `team.md` content is correct.  
**Rationale:** PR review diff confusion, not a content error.

### hockney-validation-constants-path

# Decision: beat_map_loader validation constants path resolution

**Author:** Hockney  
**Date:** 2026-05  
**Issue:** #289

## Decision

`load_validation_constants(app_dir)` in `beat_map_loader.h/.cpp` now uses the project's standard dual-path pattern:
1. `app_dir + "content/constants.json"` (app-directory-first, for bundled platforms)
2. `"content/constants.json"` (CWD-relative fallback)

## Rationale

Meyer's singleton was removed because it locked constants at first-use and only tried CWD, breaking on iOS/WASM-style bundle layouts. The dual-path pattern is already established in `play_session.cpp` lines 43-54.

## Impact for other agents

- Callers with `GetApplicationDirectory()` (e.g. any future game system calling `validate_beat_map`) should call `load_validation_constants(GetApplicationDirectory())` and pass the result to `validate_beat_map(map, errors, vc)`.
- The 2-arg `validate_beat_map(map, errors)` stays for test / non-raylib contexts; it uses CWD only.
- `ValidationConstants` struct is now public in `beat_map_loader.h` — can be stored in the ECS registry context if needed by other systems.

### keaton-311-314-phase-bitmasks

# Decision: GamePhaseBit as dedicated mask enum (not changing GamePhase)

**Date:** 2026-04-27  
**Author:** Keaton  
**Issues:** #311, #314  
**Branch:** squad/311-314-phase-bitmasks

## Decision

Adopted `GamePhaseBit` (a new power-of-two enum with `_entt_enum_as_bitmask`) rather than converting `GamePhase` itself to power-of-two values.

## Rationale

`GamePhase` is a state-machine discriminant stored in `GameState::phase` and compared in switches throughout the codebase. Changing it to power-of-two would have been invasive and semantically confusing ("what phase am I in" vs "which phases is this entity active in" are different questions).

`GamePhaseBit` cleanly owns the masking concern. A `to_phase_bit(GamePhase)` bridge function handles the conversion inside `phase_active()`.

## Interface

```cpp
// game_state.h
enum class GamePhaseBit : uint8_t {
    Title = 1<<0, LevelSelect = 1<<1, Playing = 1<<2,
    Paused = 1<<3, GameOver = 1<<4, SongComplete = 1<<5,
    _entt_enum_as_bitmask
};
[[nodiscard]] constexpr GamePhaseBit to_phase_bit(GamePhase p) noexcept;

// input_events.h
struct ActiveInPhase { GamePhaseBit phase_mask = GamePhaseBit{}; };
inline bool phase_active(const ActiveInPhase& aip, GamePhase phase);

// Spawn sites use: GamePhaseBit::X | GamePhaseBit::Y
```

## Impact

All `phase_bit()` call sites migrated. `phase_active()` unchanged at call sites. 2616 tests pass.

### keaton-313-high-score-key

# Decision: entt::hashed_string runtime API — use const_wrapper overload

**Author:** Keaton  
**Issue:** #313  
**Date:** 2026-04-28

## Decision

When hashing runtime (non-literal) strings with `entt::hashed_string`, always cast to `const char*` before calling `value()`:

```cpp
entt::hashed_string::value(static_cast<const char*>(buf))
```

**Do NOT** use `entt::hashed_string{buf}.value()` or `entt::hashed_string::value(buf)` when `buf` is a `char[N]` array — both resolve to the `ENTT_CONSTEVAL` array-literal overload, which will fail to compile when `buf` is not a constant expression.

## Rationale

EnTT's `basic_hashed_string` has three `value()` overloads:
1. `static ENTT_CONSTEVAL hash_type value(const value_type (&str)[N])` — compile-time only (array literal)
2. `static constexpr hash_type value(const value_type *str, size_type len)` — runtime (pointer + length)
3. `static constexpr hash_type value(const_wrapper wrapper)` — runtime (implicit from `const char*`)

Overload (3) is selected when you pass `static_cast<const char*>(buf)`. This is consistent with how `ui_source_resolver.cpp` uses `entt::hashed_string::value(source.data())`.

## Scope

All runtime hashing sites: `HighScoreState::make_key_hash()`, `get_score_by_hash()`, `set_score_by_hash()`, `play_session.cpp`.

### keaton-318-rebase

# Decision: #318 Rebase onto ecs_refactor (Keaton)

**Date:** 2026-04-27
**Author:** Keaton
**Issue:** #318 — Move high-score and settings mutation logic out of components

## What I did

Kujan rejected the McManus PR because `squad/318-high-score-settings-logic` was stale behind
`user/yashasg/ecs_refactor` (missing commits `2e2b0c8` and `b5e81c1`). As revision owner I:

1. Merged `origin/user/yashasg/ecs_refactor` into `squad/318-high-score-settings-logic` (`git merge --no-edit`).
2. No conflicts: `2e2b0c8` touched only `.squad/` files; `b5e81c1` added dispatcher-comment blocks to
   `game_state_system.cpp` in regions McManus had not touched.
3. Build and tests passed: 2588 assertions / 808 test cases, zero warnings.
4. Merge commit: `d4d3f01`. Force-pushed with `--force-with-lease`.

## Relevant pattern for future branches

When a PR is rejected because it is stale behind a shared refactor branch (`ecs_refactor` in this case),
a plain `git merge origin/<refactor-branch>` is the correct fix — not a rebase — because the branch
already has a published history that other reviewers have read. Rebasing would rewrite shared commits
and require a harder force-push that can confuse reviewers.

### keaton-archetypes-folder

# Archetype sources live in `app/archetypes`

**Author:** Keaton (C++ Performance Engineer)  
**Date:** 2026-04-27  
**Status:** IMPLEMENTED

## Decision

Obstacle archetype factory code is no longer owned by `app/systems/`. It lives in `app/archetypes/`, and `shapeshifter_lib` includes an `ARCHETYPE_SOURCES` CMake glob for that folder.

## Rationale

Archetypes are shared construction/factory code used by scheduler, random spawning, and tests. Keeping them out of systems clarifies ownership while preserving a single compiled implementation for the library, executable, and tests.

### keaton-burnout-sfx-cleanup

# Decision: Remove SFX::BurnoutBank — Use SFX::ScorePopup for Obstacle-Clear Audio

**Owner:** Keaton  
**Date:** 2026-05-17  
**Commit:** 9ab0a1c  
**Blocker:** Kujan rejection — BurnoutBank fires on every obstacle clear in a non-burnout scoring path

## Decision

Remove `SFX::BurnoutBank` from the `SFX` enum entirely. The slot was originally added for burnout-multiplier scoring feedback. After `#239` removed burnout altogether, the name became a semantic contradiction when fired in the standard scoring path.

`SFX::ScorePopup` (already present, 988 Hz Sine, 0.060 s) is the correct slot for "obstacle successfully cleared" audio feedback.

## Changes

| File | Change |
|------|--------|
| `app/components/audio.h` | Removed `BurnoutBank` from `SFX` enum; updated `enum_count` guard 10 → 9 |
| `app/systems/sfx_bank.cpp` | Removed BurnoutBank `SfxSpec` entry; array stays contiguous |
| `app/systems/scoring_system.cpp` | `audio_push(..., SFX::BurnoutBank)` → `audio_push(..., SFX::ScorePopup)` |
| `tests/test_audio_system.cpp` | Removed `SFX::BurnoutBank` from `kAllSfx[]`; count stays consistent |

## Rationale

- Removing > renaming: a renamed slot would still require updating all call sites and documentation. Since no production path needs a dedicated "burnout bank confirmation" sound post-#239, removal is cleaner.
- `SFX::ScorePopup` already existed and is the appropriate semantic slot.
- Enum contiguity and `static_assert` guards maintained.
- Build: zero compiler warnings. Tests: all `[audio]` and `[scoring]` cases pass.

### keaton-entt-core-implementation

# EnTT Core Functionalities — Implementation Decisions

**Author:** Keaton (C++ Performance Engineer)
**Date:** 2026
**Status:** GUIDANCE

## Summary

Audited `docs/entt/Core_Functionalities.md` against the full `app/` codebase. Filed GitHub issues #308–#313 tagged `ecs_refactor` under TestFlight milestone. P1 fixes landed in commit 7fc569a.

## Decisions

### 1. Static vector pattern is the standard for deferred-destroy loops (CONFIRMED)
`static std::vector<entt::entity>; .clear()` is the canonical pattern for collect-then-destroy loops in hot systems. `cleanup_system.cpp` is the reference implementation. `lifetime_system.cpp` now matches.

### 2. `ctx().find<>()` must be hoisted above all entity loops
Any `reg.ctx().find<T>()` or `reg.ctx().get<T>()` call inside a `view.each()` loop is a pattern violation. The lookup is O(1) but the null-check guard inside the loop is noise. Hoist once, use the pointer throughout. Applies to all future system authors.

### 3. `entt::enum_as_bitmask` is the right replacement for `ActiveInPhase.phase_mask` (PENDING)
The `GamePhase` enum + `phase_bit()` + `phase_active()` manual bitmask pattern should be replaced with `_entt_enum_as_bitmask`. **Blocker:** `GamePhase` values 0–5 must become powers-of-two 0x01–0x20. Any serialized or raw-cast usage must be audited first. Do not land until all GamePhase integer usages are confirmed safe.

### 4. `entt::hashed_string` is the right key type for the UI element lookup map (PENDING)
`find_el()` in `ui_render_system.cpp` should be replaced with a pre-built `std::unordered_map<entt::hashed_string::hash_type, const json*>` at `UIState::load_screen()`. Lookup keys are compile-time string literals that become `"id"_hs` constants. FNV-1a collision risk is negligible for ~10 element IDs per screen.

### 5. `entt::monostate` is NOT adopted — `reg.ctx()` remains the singleton standard
The project uses `reg.ctx().emplace<T>()` consistently for all game singletons. `entt::monostate` is global and bypasses registry lifetime management. Decision: never adopt for game-owned state.

### 6. No new EnTT Core utilities are needed for the hot path
`entt::any`, `allocate_unique`, `y_combinator`, `iota_iterator`, `type_index`, `family`, `ident`, `compressed_pair`, `input_iterator_pointer` — none apply to the current codebase patterns.

## What Needs Sign-Off Before #311 Lands

The `entt::enum_as_bitmask` change for `GamePhase` requires confirming:
- No serialized `GamePhase` integer values in beatmap JSON or settings files
- No network/IPC protocol using raw GamePhase integers
- All `static_cast<uint8_t>(phase)` sites are safe after value reassignment

Recommend Kujan or McManus does a call-site audit before implementation starts.

### keaton-enum-macro-design

# Decision: Safe Enum Macro — DECLARE_ENUM

**Author:** Keaton  
**Date:** 2026-05-17  
**Status:** PROPOSED — awaiting user approval before implementation  
**Blocks:** PR #43 must merge first; do not touch component headers until then

---

## Problem

- The existing X-macro pattern (`#define FOO_LIST(X) X(A) X(B) ...`) requires a separate named macro at the top of every header that needs it. Users must reference both the list macro and the expansion logic every time.
- The originally proposed 7-argument unscoped macro was rejected (Keyser + Kujan) for breaking `enum class`, underlying types, explicit values, fixed arity, and ODR.
- The user clarified: "the xmacro pattern is too cumbersome… I want a macro that takes the enum name as input."

---

## Hard Preprocessor Limit (be honest)

**The C++ preprocessor cannot derive enumerator names from an already-defined `enum class` by name alone.** There is no runtime reflection on enum members in C++20 without external tooling (libclang, magic_enum with compiler-specific builtins, etc.). The enumerator list MUST be provided at definition time — there is no way around this.

What we CAN do: accept the list inline in a single macro call that both defines the enum and generates its string table, eliminating the separate `FOO_LIST` macro entirely.

---

## Recommended API: `DECLARE_ENUM`

### Signature

```cpp
DECLARE_ENUM(EnumName, UnderlyingType, enumerator1, enumerator2, ...)
```

### Example Usage

```cpp
// Shape — replaces existing X-macro
DECLARE_ENUM(Shape, uint8_t, Circle, Square, Triangle, Hexagon)

// SFX — adds ToString(); COUNT sentinel included last
DECLARE_ENUM(SFX, uint8_t,
    ShapeShift, BurnoutBank, Crash, UITap, ChainBonus,
    ZoneRisky, ZoneDanger, ZoneDead, ScorePopup, GameStart, COUNT)

// HapticEvent — adds ToString() (13 enumerators, no gaps)
DECLARE_ENUM(HapticEvent, uint8_t,
    ShapeShift, LaneSwitch, JumpLand,
    Burnout1_5x, Burnout2_0x, Burnout3_0x, Burnout4_0x, Burnout5_0x,
    NearMiss, DeathCrash, NewHighScore, RetryTap, UIButtonTap)
```

`ToString()` output: `Shape::Circle → "Circle"`, `SFX::COUNT → "COUNT"`.

### Enums with explicit values — keep manual

```cpp
// GamePhase — explicit values document semantics; keep as-is
enum class GamePhase : uint8_t {
    Title        = 0,
    LevelSelect  = 1,
    Playing      = 2,
    Paused       = 3,
    GameOver     = 4,
    SongComplete = 5
};
// ToString added manually via switch (or the existing ui_source_resolver pattern)

// EndScreenChoice — same reasoning
enum class EndScreenChoice : uint8_t { None = 0, Restart = 1, LevelSelect = 2, MainMenu = 3 };
```

Explicit-value annotations serve as documentation of protocol/ABI stability. Removing them to use the macro is a net regression in clarity for these enums.

---

## Implementation

### New file: `app/util/enum_utils.h`

```cpp
#pragma once
// DECLARE_ENUM(Name, UnderlyingType, enumerators...)
//
// Generates:
//   enum class Name : UnderlyingType { enumerators... };
//   inline const char* ToString(Name) noexcept;
//
// Requirements:
//   - C++20 (__VA_OPT__ used for zero-overhead empty-arg safety)
//   - Enumerator values must be 0-based consecutive integers
//     (explicit = N assignments are not supported inline)
//   - For enums needing forward declarations, place the forward decl
//     manually: enum class Name : UnderlyingType;

// ── Internal FOR_EACH infrastructure ────────────────────────────────────────
// Recursive deferred expansion: handles up to 256 enumerators (4^4 expansions).
#define _SE_PARENS ()
#define _SE_EXPAND(...)  _SE_EX1(_SE_EX1(_SE_EX1(_SE_EX1(__VA_ARGS__))))
#define _SE_EX1(...)     _SE_EX2(_SE_EX2(_SE_EX2(_SE_EX2(__VA_ARGS__))))
#define _SE_EX2(...)     _SE_EX3(_SE_EX3(_SE_EX3(_SE_EX3(__VA_ARGS__))))
#define _SE_EX3(...)     __VA_ARGS__

#define _SE_MAP(f, ...)  __VA_OPT__(_SE_EXPAND(_SE_MAP_H(f, __VA_ARGS__)))
#define _SE_MAP_H(f, a, ...) f(a) __VA_OPT__(_SE_MAP_AGAIN _SE_PARENS (f, __VA_ARGS__))
#define _SE_MAP_AGAIN()  _SE_MAP_H

#define _SE_VAL(e)  e,
#define _SE_STR(e)  #e,

// ── Public macro ─────────────────────────────────────────────────────────────
#define DECLARE_ENUM(Name, Type, ...)                                       \
    enum class Name : Type {                                                \
        _SE_MAP(_SE_VAL, __VA_ARGS__)                                       \
    };                                                                      \
    inline const char* ToString(Name _e) noexcept {                        \
        static constexpr const char* const _s[] = {                        \
            _SE_MAP(_SE_STR, __VA_ARGS__)                                   \
        };                                                                  \
        auto _i = static_cast<std::size_t>(static_cast<Type>(_e));         \
        return _i < (sizeof(_s) / sizeof(_s[0])) ? _s[_i] : "???";        \
    }
```

### Why this is safe

| Concern | Status |
|---------|--------|
| `enum class` preserved | ✓ — generated verbatim |
| Underlying type (`:uint8_t`) preserved | ✓ — `Type` arg is injected |
| ODR / duplicate symbol | ✓ — `inline` function in header; one definition per TU guaranteed by C++17+ ODR for `inline` |
| `constexpr` string array | ✓ — zero heap allocation; resides in `.rodata` |
| Trailing comma in enum body | ✓ — valid C++11+ |
| Trailing comma in array initializer | ✓ — valid C++11+ |
| Mutable global (the original macro's `const char*[]`) | ✗ eliminated — `constexpr const char* const` is fully immutable |
| Variable arity | ✓ — `__VA_OPT__` + recursive expansion |
| Empty enum guard | ✓ — `__VA_OPT__` suppresses expansion on zero args |
| Zero warnings | ✓ — no implicit conversions; `static_cast` is explicit |
| Forward declarations | ⚠ — `DECLARE_ENUM` always produces a full definition. Write `enum class WindowPhase : uint8_t;` manually in `player.h`; `rhythm.h` uses `DECLARE_ENUM`. One-line cost, not a regression. |

### Why explicit values are excluded

Supporting `(Name, Value)` tuple unpacking in variadic macros requires either:
- `BOOST_PP_SEQ_FOR_EACH` (not in this project)
- An additional layer of parenthesized-pair parsing macros (3–4 extra helpers per tuple element type)

For only 5 enums in the codebase that have meaningful explicit values, that complexity is not justified. Those enums (GamePhase, EndScreenChoice, BurnoutZone, WindowPhase, Layer, VMode, FontSize) keep manual definitions. The `= N` annotations carry semantic weight; preserving them is a feature, not a bug.

---

## Which Enums Convert, Which Stay Manual

### Convert to `DECLARE_ENUM` (replace X-macro or add ToString)

| Enum | File | Values | Action |
|------|------|--------|--------|
| `Shape` | `player.h` | 4 | Replace `SHAPE_LIST` X-macro |
| `ObstacleKind` | `obstacle.h` | 8 | Replace `OBSTACLE_KIND_LIST` X-macro |
| `TimingTier` | `rhythm.h` | 4 | Replace `TIMING_TIER_LIST` X-macro |
| `SFX` | `audio.h` | 10 + COUNT | Add `ToString()` |
| `HapticEvent` | `haptics.h` | 13 | Add `ToString()` |
| `ActiveScreen` | `ui_state.h` | 6 | Add `ToString()` (debug/logging) |
| `DeathCause` | `song_state.h` | 4 | Add `ToString()` (replace `death_cause_to_string`) |

### Stay manual (explicit values, forward-decl, or no ToString needed)

| Enum | Reason |
|------|--------|
| `GamePhase` | Explicit values document ABI/protocol; manual `ToString()` switch in `ui_source_resolver.cpp` is fine |
| `EndScreenChoice` | Explicit values, no logging need |
| `WindowPhase` | Forward-declared in `player.h`; definition stays in `rhythm.h` |
| `BurnoutZone` | Explicit values (semantically important: 0=None…4=Dead); add manual `ToString()` if needed |
| `Layer` | Explicit values |
| `VMode` | Explicit values |
| `FontSize` | `int` underlying type (not `uint8_t`) |
| `InputSource`, `Direction`, `TextAlign` | Pure data, no ToString needed |
| `InputType`, `MeshType`, `MenuActionKind` | Pure data, no ToString needed |
| `ProceduralWave` | In `.cpp` (sfx_bank.cpp), not a header enum; leave in place |

---

## Does This Replace All Enums?

**No.** `DECLARE_ENUM` targets only enums that need `ToString()` and have 0-based consecutive values. Enums that are pure data or have meaningful explicit value assignments stay as plain `enum class`. Approximately 7 of 23 enums convert.

---

## Scope Fit With PR #43

PR #43 (ecs_refactor) touches `player.h`, `rendering.h`, `rhythm.h`, and several component headers — exactly the files this refactor would touch. **Do not start implementation until PR #43 merges.** All 7 target headers are in the conflict zone.

---

## Implementation Plan (post-PR #43 merge)

| Step | Owner | Action |
|------|-------|--------|
| 1 | Keaton | Create `app/util/enum_utils.h` with macro + internal helpers |
| 2 | Keaton | Convert `Shape`, `ObstacleKind`, `TimingTier` (X-macro replacements; safest first) |
| 3 | Keaton | Convert `SFX`, `HapticEvent` (add ToString) |
| 4 | Keaton | Convert `ActiveScreen`, `DeathCause` (replace `death_cause_to_string` in `ui_source_resolver.cpp`) |
| 5 | Baer | Zero-warnings build on macOS (arm64) + WASM; run full test suite |
| 6 | Kujan | Review pass |

**Files touched:** `app/util/enum_utils.h` (new), `app/components/player.h`, `app/components/obstacle.h`, `app/components/rhythm.h`, `app/components/audio.h`, `app/components/haptics.h`, `app/components/ui_state.h`, `app/components/song_state.h`, `app/systems/ui_source_resolver.cpp`.  
No system `.cpp` files change behavior; `death_cause_to_string` in `ui_source_resolver.cpp` is the only logic migration.

---

## Acceptance Tests / Build Plan

```bash
# After implementation:
cmake -B build -S . -Wno-dev
cmake --build build --parallel   # must produce zero warnings

./build/shapeshifter_tests        # all existing tests must pass
# Spot-check ToString outputs:
# ToString(Shape::Circle)      == "Circle"
# ToString(SFX::COUNT)         == "COUNT"
# ToString(HapticEvent::DeathCrash) == "DeathCrash"
# ToString(ObstacleKind::LanePushRight) == "LanePushRight"
```

Additional manual check: confirm `SFXBank::SFX_COUNT` still equals `static_cast<int>(SFX::COUNT)` after conversion (it does — implicit value assignment is preserved).

---

## Comparison With Prior Recommendations

| Approach | By | Verdict |
|----------|----|---------|
| Fixed-7-arg unscoped macro | User (initial) | Rejected — breaks enum class, types, arity, ODR |
| X-macro `FOO_LIST(X)` (existing pattern) | Keyser | Works but user finds too cumbersome |
| `DECLARE_ENUM` variadic C++20 | Keaton | **Recommended** — single definition site, enum class preserved, zero ODR issues, variable arity |

### keaton-magic-enum-refactor

# Decision: magic_enum ToString Refactor

**Author:** Keaton  
**Date:** 2026-05-17  
**Commit:** 8fbce9c  
**Status:** IMPLEMENTED

## What changed

Replaced X-macro `ToString` scaffolding in `player.h`, `obstacle.h`, and `rhythm.h` with plain `enum class` declarations and `ToString()` wrappers backed by `magic_enum::enum_name()`. Removed `SFX::COUNT` sentinel and derived the count via `magic_enum::enum_count<SFX>()`.

## Key decisions

1. **`ToString()` still returns `const char*`** — Call sites use `%s` in printf-family calls. Returning `std::string_view` would require call-site changes across session_logger, ring_zone_log_system, and test_player_system. Wrapping `enum_name().data()` is safe: magic_enum 0.9.7 stores names in `static constexpr static_str<N>` with an explicit null terminator in `chars_`.

2. **`SFX::COUNT` removed, not kept as alias** — A `COUNT` entry in a `uint8_t` enum creates a footgun: any switch on SFX must handle COUNT even though it is never a valid sound. `magic_enum::enum_count<SFX>()` is the canonical, always-accurate replacement. A `static_assert` in `sfx_bank.cpp` ties `SFX_SPECS.size()` to `enum_count<SFX>()` to catch mismatches at compile time.

3. **`test_audio_system.cpp` edited minimally** — Task scope allows test edits to unblock compile. Only the `SFX::COUNT` static_assert and the modulo expression were updated; no test logic or assertions changed.

## Affected files

- `app/components/player.h`
- `app/components/obstacle.h`
- `app/components/rhythm.h`
- `app/components/audio.h`
- `app/systems/sfx_bank.cpp`
- `tests/test_audio_system.cpp`

### keaton-score-popup-tier

# Decision: ScorePopup::timing_tier is now std::optional<TimingTier>

**Date:** 2025  
**Author:** Keaton  
**Issue:** #288

## Decision

`ScorePopup::timing_tier` was changed from `uint8_t` (with magic sentinel `255`) to `std::optional<TimingTier>`.

## Rationale

The sentinel value `255` duplicated enum knowledge outside the type system. Any code reading `timing_tier` had to know that 255 means "no grade" — and `popup_display_system` then had to map raw integers 3/2/1/0 back to enum meanings. Using `std::optional<TimingTier>` makes "no grade" explicit at the type level and lets the switch use named enum cases.

## Impact on other agents

- Any code constructing a `ScorePopup` must use `std::nullopt` (no grade) or `std::make_optional(tier)` — not a raw uint8_t cast.
- Any code reading `timing_tier` must call `.has_value()` / `*timing_tier` — not compare against 255.
- `scoring.h` now includes `rhythm.h`; downstream includes get `TimingTier` transitively.

### keyser-322-ui-pod-layout

# Decision: #322 — UI Layout POD Cache in reg.ctx()

**Author:** Keyser  
**Date:** 2026-04-27  
**Issue:** #322 — Precompute UI layout data instead of traversing JSON during render

## Decision

Stable HUD and level-select layout constants are extracted from JSON at screen-load time into typed POD structs (`HudLayout`, `LevelSelectLayout`) stored in `reg.ctx()`. The render path reads from these structs; zero JSON traversal occurs in the hot render loop.

## Rationale

- `ui_render_system.cpp` previously called `nlohmann::json::operator[]` + `.get<float>()` on every frame for shape button layout, approach ring, lane divider, and all level-select card/button geometry.
- `#312` gave us O(1) element lookup (`element_map`), but still returned `const json*`; callers still parsed field values per frame.
- Extracting to POD at screen-load boundary (O(N) once, amortised) eliminates the per-frame parse cost and keeps the render system decoupled from JSON schema details.

## Pattern Established

```
Screen load (ui_navigation_system):
  build_ui_element_map(ui)               // O(N) — #312
  reg.ctx().insert_or_assign(build_hud_layout(ui))      // O(K fields)
  reg.ctx().insert_or_assign(build_level_select_layout(ui))

Render (ui_render_system):
  const auto* hud = reg.ctx().find<HudLayout>();
  if (hud) draw_hud(reg, *hud);          // zero JSON access
```

## Key Implementation Notes

- Use `.at()` not `operator[]` in builder functions — the latter uses `assert()` on const objects, breaking try-catch error handling.
- `reg.ctx()` uses `insert_or_assign()` (EnTT v3 context API), not `emplace_or_replace`.
- `valid` bool on each struct guards the render function; missing required elements set it to `false` with a stderr warning.
- Coordinate fields are pre-multiplied by `SCREEN_W_F`/`SCREEN_H_F` at build time.

## Files

- `app/components/ui_layout_cache.h` (new)
- `app/systems/ui_loader.h`, `ui_loader.cpp` — builder declarations + implementations
- `app/systems/ui_navigation_system.cpp` — calls builders on screen change
- `app/systems/ui_render_system.cpp` — draw functions take POD structs
- `tests/test_ui_layout_cache.cpp` (new) — 11 tests

### keyser-archetypes-folder-validation

# Architectural Validation: `app/archetypes/` as canonical archetype home

**Author:** Keyser (Lead Architect)  
**Date:** 2026-05-18  
**Status:** APPROVED — no blocking issues, one P1 VCS hygiene note

---

## Verdict

The move of `obstacle_archetypes.h/.cpp` from `app/systems/` → `app/archetypes/` is **architecturally correct and complete**. The working tree is clean of stale references. The build configuration is correct. All include paths are updated. The ECS ownership boundary is improved, not violated.

---

## What Was Audited

- `app/archetypes/obstacle_archetypes.h` and `.cpp`  
- `app/systems/beat_scheduler_system.cpp` and `obstacle_spawn_system.cpp` (callers)  
- `tests/test_obstacle_archetypes.cpp`  
- `CMakeLists.txt` (build configuration)  
- `app/systems/` (stale reference scan)  
- `app/components/` (component ownership boundary check)

---

## Findings

### ✅ ECS Ownership — CORRECT

`apply_obstacle_archetype(entt::registry&, entt::entity, const ObstacleArchetypeInput&)` is a **pure entity assembly function**, not a system. It has no `float dt`, no `GameState` query, no iteration. It emplaces components on a pre-created entity. Housing it under `app/systems/` was an ECS ownership violation. `app/archetypes/` is the correct canonical layer.

The header only includes component headers (`obstacle.h`, `player.h`) and `entt/entt.hpp` — no system headers, no upward dependency. Correct layering boundary.

### ✅ No stale duplicate in `app/systems/`

`git status` confirms `D app/systems/obstacle_archetypes.h` and `D app/systems/obstacle_archetypes.cpp`. No other file in `app/systems/` retains a reference to the old path. `all_systems.h` does not mention archetypes. Clean.

### ✅ Include paths correct in all callers

- `beat_scheduler_system.cpp:2` → `#include "archetypes/obstacle_archetypes.h"` ✅  
- `obstacle_spawn_system.cpp:2` → `#include "archetypes/obstacle_archetypes.h"` ✅  
- `test_obstacle_archetypes.cpp:3` → `#include "archetypes/obstacle_archetypes.h"` ✅  

Both `shapeshifter_lib` and `shapeshifter_tests` have `${CMAKE_CURRENT_SOURCE_DIR}/app` on their include path, so all three resolve to `app/archetypes/obstacle_archetypes.h`. No broken include paths remain.

### ✅ CMakeLists.txt build configuration correct

```cmake
file(GLOB ARCHETYPE_SOURCES app/archetypes/*.cpp)
add_library(shapeshifter_lib STATIC ${SYSTEM_SOURCES} ${ARCHETYPE_SOURCES} ...)
```

`obstacle_archetypes.cpp` is compiled exactly once into `shapeshifter_lib`, which is linked by both the game executable and the test binary. The old `SYSTEM_SOURCES` glob (`app/systems/*.cpp`) no longer picks up the deleted files. No duplicate compilation, no missing translation unit.

### ✅ Test coverage complete (all 7 kinds)

`test_obstacle_archetypes.cpp` covers all `ObstacleKind` variants with 10 test cases:
- ShapeGate × 3 (Circle/Square/Triangle color variants)  
- LaneBlock (component exclusion and mask check)  
- LowBar (RequiredVAction Jumping)  
- HighBar (RequiredVAction Sliding)  
- ComboGate (RequiredShape + BlockedLanes)  
- SplitPath (RequiredShape + RequiredLane)  
- LanePushLeft — dedicated test ✅  
- LanePushRight — dedicated test ✅ (symmetric coverage per dead-surface skill)  
- Position x/y propagation

Component exclusion assertions are present on all kinds that should not carry optional components.

---

## P1 — VCS Hygiene (commit atomicity, not a code issue)

The new files in `app/archetypes/` are **untracked** (`?? app/archetypes/`). The deleted `app/systems/obstacle_archetypes.*` are `D` (staged for deletion). This must be committed as an **atomic add+delete** in a single commit — ideally using `git mv` to preserve rename tracking in `git log --follow`. If committed as separate add/delete, history trace through `git log --follow` will break at the rename point. This is not a blocking code issue but must be resolved before merge.

---

## P2/P3 — Deferred (explicitly out of scope for this TestFlight issue)

- **P3:** `LaneBlock` case in `apply_obstacle_archetype` still emits `BlockedLanes`. In the random spawner, `LaneBlock` is converted to `LanePushLeft/Right` before calling the factory, so this case may only be reached from beatmap-driven content or legacy paths. Audit whether `LaneBlock` is live or dead surface. Out of scope for #344.  
- **P3:** Hardcoded colors per kind in the factory. No data-driven color spec. Cosmetic/data-driven cleanup. Out of scope.

---

## Decision

`app/archetypes/` is the confirmed canonical home for obstacle archetype factories. Any future archetype (player archetypes, particle archetypes, UI entity archetypes) should follow the same pattern: `app/archetypes/{domain}_archetypes.h/.cpp`, picked up by `ARCHETYPE_SOURCES`, included via `"archetypes/{domain}_archetypes.h"`.

### keyser-entt-core-leverage

# EnTT Core_Functionalities Leverage — Audit Conclusions

**Author:** Keyser  
**Date:** 2026-05-18  
**Status:** DOCUMENTED — two issues filed, P2 implemented

## What Was Audited

`docs/entt/Core_Functionalities.md` vs. active codebase — looking for actionable new EnTT features beyond the prior ECS structural audit (decisions.md: "EnTT ECS Audit Findings 2026-05-17").

## New Issues Filed

| Issue | Priority | Feature | File | Status |
|-------|----------|---------|------|--------|
| [#310](https://github.com/yashasg/friendly-parakeet/issues/310) | P2 | `entt::hashed_string` for UI source resolver string dispatch | `app/systems/ui_source_resolver.cpp` | **IMPLEMENTED** in commit `4f4574f` |
| [#314](https://github.com/yashasg/friendly-parakeet/issues/314) | P3 | `entt::enum_as_bitmask` for `ActiveInPhase` phase-mask type | `app/components/input_events.h`, `game_state.h` | Open — owner: McManus |

## P2 — #310 Implemented

`resolve_ui_int_source` (15 branches) and `resolve_ui_dynamic_text` (5 branches) in `ui_source_resolver.cpp` converted from sequential `if (source == "...")` string comparisons to `switch(entt::hashed_string::value(source.data()))` with `_hs` UDL case labels. Single FNV-1a hash per dispatch replaces worst-case N×string_length comparisons. Sources are JSON-parsed `std::string` fields — `data()` is null-terminated, safe for `hashed_string::value()`.

## P3 — #314 Deferred

`ActiveInPhase::phase_mask (uint8_t)` uses manual helpers `phase_bit()` / `phase_active()` for bit manipulation on `GamePhase` indices. A dedicated `GamePhaseBit` enum (power-of-2 values, `_entt_enum_as_bitmask` sentinel) would make the mask type-safe and idiomatic. Current helpers are correct; this is ergonomic only. Deferred to McManus.

## Features Reviewed — No Issue

| Feature | Rationale for No Issue |
|---------|----------------------|
| `entt::monostate` | `reg.ctx()` is superior (registry-scoped, testable). Not worth migrating. |
| `entt::any` | Typed `ctx()` pattern already covers all use-cases cleanly. |
| `entt::tag<"name"_hs>` | Existing empty tag structs are more readable. No value in retrofitting. |
| `entt::compressed_pair` | Internal library utility; no game-side benefit. |
| `entt::allocate_unique` | No custom allocators in use. |
| `entt::overloaded` | No `std::variant` pattern identified in codebase. |
| `entt::type_name<T>()` | `magic_enum` already covers enum value names; no component-type debug logging gap. |
| `entt::fast_mod` | Hot-path moduli found (lane `% 3`) are not powers of 2 — `fast_mod` requires power-of-2. |
| `entt::ident` / `entt::family` | No compile-time or runtime type sequencing need identified. |
| `entt::integral_constant` tags | Empty structs are fine; `entt::tag<"x"_hs>` only saves a struct definition. |

## Guidance for Team

- **New UI data-binding sources**: Add cases to the `switch` in `ui_source_resolver.cpp`. Follow `"Context.field"_hs` naming convention. The compiler will flag duplicate case values (FNV-1a collision guard).
- **`entt::hashed_string` null-termination assumption**: `source.data()` passed to `hashed_string::value()` is safe only when `source` comes from a `std::string` or literal. Do NOT use on mid-buffer `string_view` slices without a local copy.
- **entt::dispatcher**: Already the active migration target for `EventQueue` (decisions.md). That migration supersedes Core_Functionalities findings for input.

### keyser-enum-macro-refactor

# Decision Recommendation: Enum Macro Refactor

**Author:** Keyser  
**Date:** 2026-05-17  
**Status:** NO-GO on exact macro — propose safer alternative  
**Blocks:** PR #43 must merge first

---

## Request

Replace all codebase enums with a fixed 7-argument unscoped macro:

```c
#define ENUM_MACRO(name, v1, v2, v3, v4, v5, v6, v7) \
    enum name { v1, v2, v3, v4, v5, v6, v7 };         \
    const char *name##Strings[] = { #v1, #v2, #v3, #v4, #v5, #v6, #v7 };
```

---

## Finding: Zero enums can be converted directly

Every enum in this codebase fails at least one compatibility test with the proposed macro.

### Incompatibility Matrix

| Enum | File | Count | Explicit values | enum class | uint8_t | Forward decl | X-macro already | Verdict |
|------|------|-------|----------------|-----------|---------|--------------|----------------|---------|
| TestPlayerSkill | test_player.h | 3 | ✓ (0,1,2) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| UIElementType | ui_element.h | 4 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| ProceduralWave | sfx_bank.cpp | 4 | — | ✓ | ✓ | — | — | ❌ 2; in .cpp not header |
| SFX | audio.h | **11** | ✓ (ShapeShift=0) | ✓ | ✓ | — | — | ❌ 3; COUNT sentinel |
| ObstacleKind | obstacle.h | **8** | — | ✓ | ✓ | — | **✓** | ❌ 3; already X-macro |
| InputType | input_events.h | 2 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| MenuActionKind | input_events.h | 7 | ✓ (0–6) | ✓ | ✓ | — | — | ❌ 2 (class + explicit values) |
| DeathCause | song_state.h | 4 | ✓ (0–3) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| ActiveScreen | ui_state.h | 6 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| TextAlign | text.h | 3 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| FontSize | text.h | 3 | ✓ (0,1,2) | ✓ | **int** | — | — | ❌ 3; int not uint8_t |
| InputSource | input.h | 3 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| Direction | input.h | 4 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| Shape | player.h | 4 | — | ✓ | ✓ | — | **✓** | ❌ 3; already X-macro |
| VMode | player.h | 3 | ✓ (0,1,2) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| Layer | rendering.h | 4 | ✓ (0–3) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| MeshType | rendering.h | 3 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| HapticEvent | haptics.h | **13** | ✓ (first=0) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| GamePhase | game_state.h | 6 | ✓ (all) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| EndScreenChoice | game_state.h | 4 | ✓ (all) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| BurnoutZone | burnout.h | 5 | ✓ (all) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| WindowPhase | rhythm.h | 4 | ✓ (all) | ✓ | ✓ | **✓ player.h** | — | ❌ 4 incompatibilities |
| TimingTier | rhythm.h | 4 | — | ✓ | ✓ | — | **✓** | ❌ 3; already X-macro |

### Root Incompatibilities (structural — affect ALL enums)

1. **`enum class` → plain `enum`** — Every usage site uses scoped names (`Shape::Circle`, `GamePhase::Playing`, etc.). Plain `enum` removes the scope qualifier, requiring a global rename of every call site or introducing silent name collisions. With `-Wall -Wextra -Werror`, shadowing warnings would become build failures.

2. **No underlying type** — All enums specify `: uint8_t` (one uses `: int`). The macro produces a compiler-determined underlying type. This breaks ABI stability, storage-size assumptions in structs, and will trigger `-Wconversion` warnings = build failure on our zero-warnings policy.

3. **Fixed 7 arguments** — Only `MenuActionKind` has exactly 7 enumerators, and even it has explicit assignments (`= 0` through `= 6`) the macro doesn't support. All other enums have 2–13 members.

4. **`const char *name##Strings[]` is a mutable non-const global array** — Three enums (`ObstacleKind`, `Shape`, `TimingTier`) already have `ToString()` functions via X-macros. The proposed string array would be a regression: mutable global state, no compile-time lookup, duplicate symbol risk across TUs.

5. **Forward declaration** — `WindowPhase` is forward-declared in `player.h`. A macro cannot produce a forward declaration; any TU that includes `player.h` before `rhythm.h` currently compiles. The macro would break this.

---

## Recommendation: NO-GO on exact macro

**Verdict: The proposed macro cannot be applied to any enum in this codebase without breaking compilation, ABI, or the zero-warnings policy.**

---

## Minimal Safe Alternative

The spirit of the request — enums with built-in string tables — is already addressed for three enums via the project's existing X-macro pattern. Extend that pattern to remaining enums that actually need string serialization.

```cpp
// Pattern already in use (obstacle.h, player.h, rhythm.h):
#define THING_LIST(X) X(A) X(B) X(C)
enum class Thing : uint8_t {
    #define THING_ENUM(name) name,
    THING_LIST(THING_ENUM)
    #undef THING_ENUM
};
inline const char* ToString(Thing t) {
    switch (t) {
        #define THING_STR(name) case Thing::name: return #name;
        THING_LIST(THING_STR)
        #undef THING_STR
    }
    return "???";
}
```

This pattern:
- Preserves `enum class` and underlying types ✓
- Preserves explicit values where needed (add after the `name` in the X) ✓
- Compiles with zero warnings ✓
- Handles any enumerator count ✓
- Supports forward declarations ✓
- String table is static const (no mutable global) ✓

If a broader utility header is desired, a variadic-safe project macro wrapping this pattern is appropriate, but **not** the 7-argument unscoped form requested.

---

## PR #43 Constraint

**Wait for PR #43 to merge first.** Active review-fix agents are still running on that PR. The files most likely in flight (`player.h`, `rhythm.h`, `rendering.h`, component headers) are the same headers this refactor touches. Starting now risks merge conflicts on every modified file.

---

## Implementation Sequence (if approved, after PR #43 merges)

1. **Keyser** — Finalize X-macro helper convention (add to `CONTRIBUTING.md` or `design-docs/architecture.md`). Identify which enums actually need string tables (not all do).
2. **McManus** — Apply X-macro pattern to enums that lack `ToString()` and are used in debug/logging paths: `GamePhase`, `ActiveScreen`, `BurnoutZone`, `DeathCause`, `HapticEvent`, `VMode`, `Layer`, `WindowPhase`, `TimingTier` (already done), `Shape` (already done).
3. **Baer** — Verify zero-warnings clean build on all 4 platforms after each file is touched. Run full test suite (386 tests).
4. **Kujan** — Review pass before merge.

**Enums that do NOT need string tables** (pure data, never logged/displayed): `InputType`, `InputSource`, `Direction`, `MeshType`, `EndScreenChoice`, `MenuActionKind`. Leave as-is.

---

## Files That Would Be Touched

~14 headers + 1 .cpp file (sfx_bank.cpp for ProceduralWave, if included). All in `app/components/` plus `app/systems/sfx_bank.cpp`.

No system `.cpp` files need changes — all enum definitions are in headers.

### keyser-testflight-dod-queue

# TestFlight DoD Execution Queue — After #288

**Date:** 2026-05-17  
**Author:** Keyser  
**Status:** PROPOSED — for coordinator routing

## Decision

The TestFlight queue of 20 open issues has been prioritized. The ordering rule is:

1. **DoD purity first:** system-responsibility violations before component-purity violations before platform/lifecycle issues.
2. **Dependency safety:** issues that touch the same file as active work (#288 is in `rendering.h`) are deferred to the next batch.
3. **Small before large:** single-file fixes before multi-system refactors; prove the ownership model before splitting god systems.
4. **Energy ownership must settle before dependent splits:** #278 (scoring→EnergyState) and #276 (energy→GameOver) must merge before #282 (collision→EnergyState) and #280 (cleanup→miss grading) are safe to execute.

## Batch Ordering

### Q1 — Start immediately (safe, no conflict with active #288)
| # | Owner | Risk |
|---|-------|------|
| #283 song_playback_system inline logging | baer | Low |
| #285 TestPlayerState fat singleton | keaton | Low |
| #289 beat_map_loader hidden singleton | hockney | Low |

### Q2 — After #288 merges
| # | Owner | Risk | Note |
|---|-------|------|------|
| #278 + #279 scoring_system energy+popup | keaton | Medium | Do together (same file) |
| #276 energy_system GameOver transition | mcmanus | Medium | Upstream of #278 semantically |
| #223 window_scale_for_tier inverted (bug) | mcmanus | Medium | Separate file (rhythm.h) |

### Q3 — After Q2 energy ownership settled
| # | Owner | Risk | Note |
|---|-------|------|------|
| #282 collision_system energy/GameOver | keaton | Medium | Depends on #278 first |
| #280 cleanup_system miss grading | keaton | Medium | Needs Saul sign-off on energy curve |
| #294 ObstacleChildren overflow | keaton | Low-Med | rendering.h, after #288 merged |

### Q4 — Large splits (plan carefully, pair with reviewer)
| # | Owner | Risk | Note |
|---|-------|------|------|
| #296 mesh child signal wiring | keaton | Medium | After #294 |
| #295 unchecked mesh indices | mcmanus | Medium | After #296 |
| #277 game_state_system god class | mcmanus | High | Pair with Kujan review |
| #281 setup_play_session omnibus | mcmanus | High | Pair with Kujan review |

### Q5 — Platform / persistence
| # | Owner | Risk |
|---|-------|------|
| #287 UIState::load_screen I/O | redfoot | Low |
| #297 persistence iOS path | hockney | Low |
| #298 persistence failures ignored | hockney | Low |
| #304 WASM lifecycle bypasses shutdown | hockney | Medium |

### Deferred / Low priority
| # | Owner | Note |
|---|-------|------|
| #236 haptics_enabled dead | kujan | Feature gap, not blocking |

## Dependency Graph
- #278 must precede #282 (EnergyState single-writer invariant)
- #276 must precede or pair with #278 (GameOver transition ownership)
- #288 must merge before #294 (same file: rendering.h)
- #294 should precede #296 (ObstacleChildren ownership → signal wiring)
- #278+#279 should precede #281 (session setup omnibus needs clean scoring interface)
- #280 requires Saul sign-off before execution (gameplay balance change)
- #277 requires Kujan review (god-system split, high surface area)

### kobayashi-magic-enum-vcpkg

# Decision: magic_enum via vcpkg — Single Source of Truth

**Author:** Kobayashi  
**Date:** 2026-05  
**Status:** IMPLEMENTED

## Decision

`magic_enum` (header-only enum reflection library) is wired through vcpkg as the canonical dependency, not vendored.

## Changes

| File | Change |
|------|--------|
| `vcpkg.json` | Added `"magic-enum"` to `dependencies` array (alphabetical, between `entt` and `raylib`) |
| `CMakeLists.txt` | Added `find_package(magic_enum CONFIG REQUIRED)` after `find_package(EnTT ...)` |
| `CMakeLists.txt` | Added `magic_enum::magic_enum` to `_system_deps` SYSTEM-include loop |
| `CMakeLists.txt` | Linked `magic_enum::magic_enum` PUBLIC from `shapeshifter_lib` (propagates to all consumers: exe and tests) |

## Rationale

- Public link on `shapeshifter_lib` means both `shapeshifter` and `shapeshifter_tests` inherit the target without repeating it.
- SYSTEM-include suppression prevents magic_enum headers from generating `-Werror` failures (header-only, but still guarded for consistency with all other third-party deps).
- vcpkg-managed: version tracked in lockfile, consistent across all 4 CI platforms (Linux, macOS, Windows, WASM).

## Validated

- `cmake -Wno-dev -B build -S .` → vcpkg installed `magic-enum:arm64-osx@0.9.7#1`, configure succeeded.
- `cmake --build build --target shapeshifter_lib` → `[100%] Built target shapeshifter_lib`, zero warnings.

### kujan-312-ui-element-map-review

# Decision: #312 UI Element Map — APPROVED

**Date:** 2026-05-17
**Reviewer:** Kujan
**Commit:** 62ace0d59a65463b09243d9bbbf40fa9fdeb46e1
**Branch:** squad/312-ui-element-map-hashed-string

## Verdict: APPROVED

## Evidence

- Full suite: 2607 assertions / 813 test cases — all pass (zero warnings)
- Focused `[ui][312]`: 19 assertions / 5 test cases — all pass
- `find_el` is now O(1) via pre-computed `UIState::element_map`; map rebuilt only on `screen_changed` boundary
- Call sites use `_hs` compile-time literals; defensive guards in `find_el` handle stale-map edge cases
- Branch is 2 commits behind `user/yashasg/ecs_refactor` but missing commits touch **disjoint files** (dispatcher docs / squad docs) — merge-tree confirms zero conflicts; no rebase required

## Reusable Convention

Branch lag relative to merge target requires rebase only when missing commits overlap with the branch's modified files. Disjoint file sets are safe to merge without rebase (contrast with #318 where overlap existed).

### kujan-313-high-score-key-review

# Decision: #313 High-Score Key Allocation — APPROVED

**Reviewer:** Kujan  
**Date:** 2026-05-17  
**Branch:** `squad/313-high-score-key-allocation` (commit `880b389`)  
**Against target:** `origin/user/yashasg/ecs_refactor` (`6887eae`)

## Verdict: APPROVED

All 6 acceptance criteria met. Full suite passes (2645/815). Clean merge into `ecs_refactor` confirmed.

## Key Facts

- `HighScoreState::current_key` (std::string) fully removed; replaced by `current_key_hash` (uint32_t)
- `ensure_entry()` called at session start to pre-register the string entry; subsequent session updates use hash only (zero heap allocation)
- `static_cast<const char*>` pattern used explicitly to select EnTT runtime hashing overload — this is the correct pattern for runtime strings
- Persistence round-trip unchanged: entries still stored as `char key[32]`, serialized as JSON strings
- `current_key_hash` preserved across persistence loads (explicit line in `high_score_state_from_json`)

## Integration Note

Branch needs rebase onto `origin/user/yashasg/ecs_refactor` before merge (user directive: ecs_refactor is SOT). Merge-tree confirms clean auto-merge: `play_session.cpp` "changed in both" — burnout removal + GamePhaseBit (ecs_refactor) and hash wiring (#313) are disjoint edits.

## Pattern for Future Hash-Key Migrations

1. `ensure_entry(key_str)` at session start — registers entry with string key
2. `current_key_hash = make_key_hash(...)` — set once, zero-allocation
3. All in-session updates via `set_score_by_hash` / `get_score_by_hash`
4. Persistence continues using string keys (stable format)
5. Preserve `current_key_hash` across load in the from_json function

### kujan-318-state-logic-rereview

# Decision: #318 high-score and settings logic — APPROVED on re-review

**Date:** 2026-05-17
**Author:** Kujan
**Issue:** #318

## Decision

Branch `squad/318-high-score-settings-logic` is **APPROVED** to merge into `user/yashasg/ecs_refactor`.

## Evidence

- Stale-branch blocker from prior rejection resolved: Keaton's `d4d3f01` merge commit brings `2e2b0c8` and `b5e81c1` into branch history.
- Diff vs target: only the 11 expected #318 files (no dispatcher docs dropped, no unrelated changes).
- Build: zero warnings.
- Full suite: 2588 assertions / 808 test cases — all pass.
- Focused `[high_score],[settings],[ftue]`: 155 assertions / 36 test cases — all pass.

## Action

Safe to merge. No further revision needed.

### kujan-318-state-logic-review

# Decision: #318 Review Outcome

**Author:** Kujan  
**Date:** 2026-05-17  
**Issue:** #318 — Move high-score and settings mutation logic out of components  
**Commit reviewed:** f34de773d419639d4c6e5cb5e6b9365b3b79276a  

## Verdict: REJECTED

## Blocking Issue

Branch `squad/318-high-score-settings-logic` is **2 commits behind** `user/yashasg/ecs_refactor`:

| Missing commit | Description |
|---|---|
| `2e2b0c8` | Scribe: document ECS closure issues #316, #317, #323, #325 |
| `b5e81c1` | docs(#327): add dispatcher drain-ownership comments and guards |

The `git diff user/yashasg/ecs_refactor..HEAD` shows these as "removed" — they were never on this branch. The post-merge combined state is untested. A rebase onto `user/yashasg/ecs_refactor` is required before this PR can be accepted.

## Implementation Quality (Correct — Not the Rejection Reason)

- `high_score::update_if_higher()` correctly extracts the mutation policy from `HighScoreState`.
- `settings::clamp_audio_offset()`, `clamp_ftue_run_count()`, `mark_ftue_complete()` correctly extracted from `SettingsState`.
- All call sites updated. Struct is now plain data.
- **2588 assertions / 808 test cases — all pass**. Zero build warnings.
- Acceptance criteria are met at the implementation level.

## Non-Blocking Note

`settings::mark_ftue_complete()` is exported but has no active call sites and no unit test. Advisory; not blocking.

## Assignment

Per lockout protocol, McManus (original author) is locked out of the rebase revision.  
**Keaton** should own the rebase onto `user/yashasg/ecs_refactor` and re-submit.

### kujan-322-ui-layout-cache-review

# Decision: #322 UI Layout Cache — APPROVED

**Date:** 2026-05-17  
**Author:** Kujan  
**Branch:** `squad/322-ui-render-pod-layout`  
**Commit:** f8e049a

## Decision

✅ APPROVED for integration into `user/yashasg/ecs_refactor`.

## Evidence

**All 5 acceptance criteria met:**

1. **AC1 — No JSON in render hot path:** `find_el`, `json_color`, `element_map` grep returns zero hits in `ui_render_system.cpp`. Only remaining JSON access is transient overlay color (out of scope for this issue).
2. **AC2 — POD layout at screen-load boundary:** `HudLayout` and `LevelSelectLayout` are plain structs, built in `ui_navigation_system` on `screen_changed`, stored via `reg.ctx().insert_or_assign()` — consistent with existing ctx singleton pattern.
3. **AC3 — Behavioral equivalence:** Same multiplications, same fallback values (`corner_radius=0.1f/0.2f`), pixel-identical render paths.
4. **AC4 — Tests:** 11 test cases / 64 assertions covering invalid-input (empty, missing required element, missing nested field), valid construction with field-value verification, and integration against shipped JSON files.
5. **AC5 — Merge compatibility:** `git merge-tree origin/user/yashasg/ecs_refactor HEAD` → single clean tree hash, zero conflicts.

**Full suite:** 2671 assertions / 824 test cases — all pass.  
**Build:** Zero compiler warnings.

## Pattern established

`build_*_layout()` builder functions: use `.at()` inside `try/catch(nlohmann::json::exception)`, return `{.valid=false}` on error, log WARN to stderr. Optional sub-elements are separately gated with `find_layout_el` + own try/catch. Render path does `if (!layout.valid) return;` guard. This is the canonical layout-cache pattern for this repo.

### kujan-324-burnout-removal-review

# Decision: #324 burnout ECS surface removal — APPROVED

**Date:** 2026-05-17  
**Reviewer:** Kujan  
**Authors:** McManus (implementation), Baer (test follow-up)  
**Branch:** `squad/324-remove-dead-burnout-surface`  
**Commits:** d9be464, 6ee912a

## Verdict: ✅ APPROVED

## Evidence

All dead burnout ECS surface (component, helpers, system, constants, ctx registrations, test files) cleanly removed. No burnout wiring remains in production ECS path.

Preserved per Saul sign-off: energy bar (`EnergyState`/`energy_system`), timing-graded scoring, chain bonus, distance bonus, LanePush exclusion.

LanePushRight mirror test added — previous coverage gap closed.

Full suite: **2565 assertions / 792 test cases — all pass, zero warnings.**

## Non-blocking notes (separate tickets)

- `"burnout_meter"` string in `ui_loader.cpp` / `test_ui_element_schema.cpp` is a pre-existing UI schema ID not connected to any ECS component or screen JSON — cleanup candidate.
- `scoring.h` comment `// burnout tier (legacy)` predates this branch — comment cleanup candidate.

## No lockouts. Branch is merge-ready.

### kujan-archetypes-folder-review

# Kujan Review Decision: Archetype Move Approved (issue #344)

**Author:** Kujan (Reviewer)  
**Date:** 2026-04-27  
**Status:** APPROVED WITH NON-BLOCKING NOTES

## Decision

The archetype relocation from `app/systems/obstacle_archetypes.*` to `app/archetypes/obstacle_archetypes.*` passes the reviewer gate.

## Evidence

- Full build: zero warnings, zero errors (`-Wall -Wextra -Werror`).
- `[archetype]` test suite: 11 tests, 64 assertions — all pass.
- All 8 `ObstacleKind` values covered exhaustively in `apply_obstacle_archetype`.
- Both system callers and test file updated to new include path; no stale paths remain.
- CMake `ARCHETYPE_SOURCES` glob correctly picks up `app/archetypes/*.cpp`; linked into `shapeshifter_lib` with correct include root.

## Non-Blocking Notes (not blocking merge)

1. `ObstacleArchetypeInput.mask` field comment only mentions LaneBlock and ComboGate; LanePush kinds also pass through (mask ignored). Update comment to reflect actual usage.
2. Random spawner selects `LaneBlock` kind index then remaps to LanePush — vestigial. Not introduced here; defer to ECS audit.

## Next Steps

- This change is ready to commit/merge.
- P2/P3 cleanup (LaneBlock vestigial mapping, comment doc gaps) deferred to ECS audit under issue #344.
- Reviewer is NOT beginning P2/P3 fixes per stated scope.

### kujan-input-dispatcher-review

# Decision: EnTT Dispatcher Input Model — Review Outcome

**Author:** Kujan  
**Date:** 2026-04-27  
**Scope:** EnTT dispatcher/sink input-model rewrite (commit 2547830, Keaton)

## Verdict: APPROVED

All 2419 assertions / 768 test cases pass. Zero build warnings.

## What Shipped

The migration is **functionally complete** for its stated scope: GoEvent and ButtonPressEvent are now fully delivered via `entt::dispatcher` stored in `reg.ctx()`. The hand-rolled `go_count`/`press_count`/`go_events[]`/`press_events[]` arrays are gone from EventQueue. The manual `eq.go_count = 0` anti-pattern that caused the #213 replay bug is eliminated — drain semantics of `disp.update<T>()` are the replacement.

### Architecture actually used (differs from decisions.md Tier-1 spec)

`decisions.md` specified: input_system enqueues InputEvent → `disp.update<InputEvent>()` fires gesture_routing and hit_test as listeners.

What shipped instead: gesture_routing_system and hit_test_system remain direct system calls (not listeners). They read the raw EventQueue (touch/mouse gesture buffer) and call `disp.enqueue<GoEvent/ButtonPressEvent>`. The dispatcher drains in the first fixed sub-tick via `game_state_system → disp.update<GoEvent/ButtonPressEvent>()`, firing all three listener chains (game_state, level_select, player_input handlers) atomically.

**This is architecturally equivalent for the accepted acceptance criteria.** No pool-order latency hazard applies because gesture_routing and hit_test are not inside a `disp.update()` chain. Same-frame behavior is preserved.

### EventQueue not fully removed

`EventQueue` struct is retained as a raw gesture shuttle (InputEvent[] only). Decisions.md migration step 4 ("Remove EventQueue struct") is NOT done. The raw-input layer (EventQueue) and semantic-event layer (dispatcher) remain separate. This is acceptable — the acceptance criteria scoped the migration to "Go/ButtonPress event delivery where intended."

## Guardrails for Future Dispatcher Work

1. **No start-of-frame `disp.clear<GoEvent/ButtonPressEvent>()`**: R7 from decisions.md is not explicitly addressed for the dispatcher queues. In practice, game_state_system drains within the same frame (it's the first fixed-tick system). If a frame skips the fixed tick, events accumulate until the next tick — currently benign but worth hardening.

2. **Redundant `disp.update()` calls are intentional**: game_state_system, level_select_system, and player_input_system all call `disp.update<GoEvent/ButtonPressEvent>()`. Only the first call per tick delivers events; subsequent calls are no-ops. The player_input_system redundancy is required for isolated unit tests that call it directly (test_player_input_rhythm_extended).

3. **Contract test comments are now stale**: `test_entt_dispatcher_contract.cpp` says "gesture_routing must use trigger(), not enqueue()". This was written before Keaton's approach was chosen. The actual implementation avoids the latency problem differently (direct system call, not listener). The comment should be updated to not mislead future readers.

4. **R4 — registry reference lifetime**: `disp.sink<GoEvent>().connect<&handler>(reg)` stores a pointer to `reg`. Safe because `unwire_input_dispatcher` disconnects before `reg.clear()`. Future changes that omit the unwire step would introduce a dangling pointer.

### kujan-pr43-template-review

# Decision: PR #43 Workflow Template Review — APPROVED

**Author:** Kujan  
**Date:** 2026-05  
**Status:** APPROVED — coordinator may commit and push

## Summary

Reviewed Hockney's uncommitted changes to `.squad/templates/workflows/` (squad-preview.yml, squad-ci.yml, squad-docs.yml) against the 5 stated acceptance criteria. All criteria pass. All 30 PR #43 review threads are resolved.

## Criteria Results

| # | Criterion | Result |
|---|-----------|--------|
| 1 | `squad-preview.yml` does NOT block `.squad/templates/**`; DOES block stateful runtime paths + `.ai-team/` | ✅ PASS |
| 2 | `squad-preview.yml` gives targeted `::error::` for missing package.json, missing CHANGELOG.md, empty version, missing changelog heading | ✅ PASS |
| 3 | `squad-ci.yml` has `# TEMPLATE` header with copy-to-.github/workflows instruction and "never run automatically" statement | ✅ PASS |
| 4 | `squad-docs.yml` `TEMPLATE NOTE` clarifies path filter targets deployed location, not template source; valid YAML | ✅ PASS |
| 5 | No C++ regressions — changed artifacts are YAML templates only | ✅ PASS |

## Non-Blocking Observation

`squad-preview.yml` has a redundant final step ("Validate package.json version") that repeats the empty-version check already performed in the earlier "Validate version consistency" step. The step is unreachable if the earlier step fails, and produces no false positives. Not a blocker — housekeeping only.

## Reviewer Lockout Note

Hockney authored this revision. No lockout applies here — this is an approval, not a rejection. If a follow-up revision is needed, Kobayashi is the designated alternate CI/CD owner.

### mcmanus-burnout-removal

# Decision: Burnout Multiplier Removed (#239)

**Author:** McManus  
**Date:** 2026-04-27  
**Status:** Implemented

## Decision

Burnout penalties have been removed. Scoring now uses a flat 1.0× base multiplier for all obstacle clears, regardless of proximity at press time.

## What Changed

- `burnout_system` is a no-op (removed from frame update path).
- `player_input_system` no longer banks `BankedBurnout` on obstacles.
- `scoring_system` ignores `BankedBurnout` and always applies `burnout_mult = 1.0f`.
- `SongResults::best_burnout` always stays 0.0.
- `ScorePopup::tier` (burnout tier) always emits 0.

## What Was NOT Removed

- `BurnoutState`, `BankedBurnout`, `BurnoutZone` structs in `burnout.h` — still compiled; safe to remove in a later pass once all reference sites are cleared.
- `burnout_helpers.h` — no longer included by any gameplay code; can be deleted in a later cleanup.
- `burnout_system.cpp` function body — no-op stub kept for link compatibility.
- `ScorePopup::tier` field — kept as zero; can be removed in UI/scoring cleanup.

## Rationale

Per issue #239: the multiplier system taught players to delay shape changes until obstacles were imminent, directly conflicting with the beat-mastery design pillar. On-beat shape changes with no nearby obstacle were functionally penalized by missing the ×1.5–×5.0 bonus window.

## Downstream Notes

- UI systems rendering `BurnoutZone` meter or `ScorePopup::tier` will silently display zero/None — no crash risk.
- The SFX enum values (`SFX::ZoneRisky`, `SFX::ZoneDanger`, etc.) are untouched.
- Keaton's magic_enum audio refactor is unaffected: no audio.h or sfx_bank.cpp changes.

### mcmanus-rngstate-spawning

# Decision: RNGState used via ctx().find/emplace in obstacle_spawn_system

**Author:** McManus  
**Date:** 2026-04-27  
**Issue:** #248

## Decision

`obstacle_spawn_system` now draws all random values through `RNGState::engine` (std::mt19937) using `std::uniform_int_distribution`, accessed via `reg.ctx()`.

The lazy-init pattern used is:
```cpp
if (!reg.ctx().find<RNGState>()) reg.ctx().emplace<RNGState>();
auto& rng = *reg.ctx().find<RNGState>();
```

EnTT v3.16.0 context does NOT have `get_or_emplace` — this is the correct fallback.

`RNGState` is now also explicitly emplaced in `make_registry()` (test helpers) for clean test setup.

## Impact

- All other systems that want ECS-resident randomness should follow this same pattern.
- `std::rand()` / `std::srand()` are no longer used anywhere in game systems.

### saul-324-burnout-signoff

# #324 — Burnout ECS Removal: Design Sign-Off

**Decision:** APPROVED_FOR_REMOVAL
**Author:** Saul (Game Designer)
**Date:** 2026-05-18
**Issue:** [P3] Remove dead burnout ECS surface after design sign-off (#324)
**Related prior decisions:** #167 (bank-on-action), #239 (burnout removed from gameplay)

---

## Decision

The burnout risk/reward mechanic was removed from the game design in GDD v1.2
(`design-docs/game.md` lines 4–9, 79–83) and from `design-docs/feature-specs.md`
SPEC 2 (header note lines 8–10). Code has already been collapsed accordingly:
`burnout_system` is a no-op (`app/systems/burnout_system.cpp`), `scoring_system`
hardcodes `burnout_mult = 1.0f` (`app/systems/scoring_system.cpp:108`), and
`player_input_system` no longer attaches `BankedBurnout` (verified by
`tests/test_burnout_bank_on_action.cpp`).

There is no remaining design intent that the burnout ECS surface serves. It is
safe to delete. **Saul signs off on removal.**

## Dead / no-op design concepts (safe to delete)

These concepts no longer correspond to any player-facing mechanic. The ECS
surface backing them should be removed, along with the tests that only assert
their no-op behavior.

| Surface | File(s) | Status |
|---|---|---|
| `enum class BurnoutZone` (Safe/Risky/Danger/Dead/None) | `app/components/burnout.h` | Dead — no zones in v1.2 design |
| `struct BurnoutState { meter, zone, threat_distance, nearest_threat }` | `app/components/burnout.h`, `play_session.cpp:46`, `game_loop.cpp:83`, `test_helpers.h:42` | Dead — never read by any system |
| `struct BankedBurnout { multiplier, zone }` | `app/components/burnout.h` | Dead — never emplaced; scoring ignores it (#239) |
| `void burnout_system(...)` declaration + no-op definition | `app/systems/all_systems.h:65`, `app/systems/burnout_system.cpp` | Dead — empty function, called every frame for nothing |
| `compute_burnout_zone(...)` + `multiplier_for_zone(...)` helpers | `app/systems/burnout_helpers.h` | Dead — only the helper file's own callsites remain |
| Constants `MULT_RISKY`, `MULT_DANGER`, `MULT_CLUTCH` | `app/constants.h:46–48` | Dead — `MULT_SAFE = 1.0` is the only multiplier scoring still uses, and it can be inlined or renamed |
| `SongResults::best_burnout` field | `app/components/song_state.h:64` | Dead — never written; results UI must not display it |
| Tests asserting burnout no-op | `tests/test_burnout_system.cpp` (entire file), `tests/test_haptic_system.cpp` lines 67–98 ("burnout haptic" cases), `tests/test_burnout_bank_on_action.cpp` (regression net for #239 — keep one minimal "scoring is flat 1.0×" assertion in `test_scoring_system.cpp` and delete the rest) | Replace with the surviving timing/energy assertions below |
| `BurnoutState` defaults test | `tests/test_components.cpp:69–95` | Delete with the component |

## Player-facing concepts that MUST remain

These are the live design promises. The cleanup must not weaken any of them.

1. **Energy bar = survival.** Single survival model. `EnergyState`,
   `energy_system`, `ENERGY_DRAIN_MISS`, `ENERGY_DRAIN_BAD`,
   `ENERGY_RECOVER_PERFECT/GOOD/OK`, `ENERGY_MAX`, `ENERGY_FLASH_DURATION`,
   `DeathCause::EnergyDepleted`. **Keep all of it.**
2. **Timing-graded scoring.** `TimingTier::{Perfect,Good,Ok,Bad}`,
   `timing_multiplier(...)`, `TimingGrade` component, `MissTag` flow. The
   timing axis is now the *only* skill-expression multiplier. **Keep.**
3. **Chain.** `ScoreState::chain_count`, `chain_timer`, `CHAIN_BONUS[]`,
   `SongResults::max_chain`, 2.0s chain window. **Keep.**
4. **Distance/time bonus.** `PTS_PER_SECOND`, `ScoreState::distance_traveled`,
   smooth `displayed_score`. **Keep** (separate balance issues exist — see
   #181/#206 — but they are not part of #324).
5. **LanePush exclusion from the scoring ladder.** Passive auto-pushes still
   must not pop, chain, or contribute to results. The exclusion branch in
   `scoring_system.cpp:100–105` stays; only the comment "excluded from the
   burnout ladder" should be reworded to "passive scenery — no scoring".
6. **Score popups, SFX, haptics on hit/miss.** Untouched by this removal.

### Explicit non-goals

- **No "near-miss" mechanic is intended.** GDD v1.2 §"Why This Works"
  intentionally removed the perverse incentive to delay shape changes. Do
  *not* re-introduce a near-miss bonus, proximity-based haptic, or proximity
  popup as part of #324. Any future near-miss feature must come back through
  design as a fresh proposal, not piggyback on this cleanup.
- **No `best_burnout` UI.** If any results-screen renderer still reads
  `SongResults::best_burnout`, remove the field and that renderer line.

## Implementation acceptance criteria for McManus

1. `app/components/burnout.h` deleted; all includes purged.
2. `app/systems/burnout_system.cpp`, `app/systems/burnout_helpers.h`, and the
   `burnout_system` declaration in `all_systems.h` deleted. Frame loop in
   `game_loop.cpp` no longer calls `burnout_system`.
3. `BurnoutState` removed from `play_session.cpp`, `game_loop.cpp`, and
   `test_helpers.h`. `BankedBurnout` and `BurnoutZone` deleted with the header.
4. `SongResults::best_burnout` field deleted; any reader (results screen, song
   complete writer) updated to omit it.
5. `MULT_RISKY`, `MULT_DANGER`, `MULT_CLUTCH` deleted from `constants.h`.
   `MULT_SAFE` may stay as `1.0f` if still referenced, otherwise inline/delete.
   `scoring_system.cpp:108` becomes a literal `1.0f` or the line collapses
   (`points = floor(base * timing_mult)`).
6. Tests:
   - Delete `tests/test_burnout_system.cpp` entirely.
   - Delete `tests/test_burnout_bank_on_action.cpp` entirely (keep one
     positive assertion in `test_scoring_system.cpp` that scoring uses
     `floor(base * timing_mult)` with no other multiplier — already covered
     by existing cases at line 162; verify).
   - Delete the "burnout haptic" `TEST_CASE` block in
     `tests/test_haptic_system.cpp` (lines ~67–98).
   - Delete the `BurnoutState` default test in `tests/test_components.cpp`.
   - Remove `static_cast<void>(reg.ctx().get<BurnoutState>())` line in
     `test_components.cpp:95`.
7. `decisions.md` entries that reference burnout ECS internals (`#167`
   bank-on-action design notes, `BurnoutState stale fields`,
   `BankedBurnout stale on miss`) get a one-line "superseded by #324" note in
   the same file. Do not rewrite history.
8. Build is clang/MSVC clean per the zero-warnings policy. All test suites
   pass with the burnout cases removed; `[scoring]`, `[energy]`, `[timing]`,
   `[chain]`, `[lane_push]` tags must still be green.
9. Search the repo for the literal strings `Burnout`, `burnout`, `BANKED`,
   `BANKED_BURNOUT`, `best_burnout` after the change — only allowed
   survivors are (a) historical decision-log entries and (b) GDD v1.2 prose
   that explains *why it was removed*.

## Out of scope for #324

- Re-balancing energy drain/recover (`#100`/diagnostic backlog).
- Cross-difficulty scoring parity (`#181`).
- Chain-bonus magnitude vs. multiplier stacking (`#206`).
- Any new "near-miss" feature — must come as a separate design issue.

---

**Sign-off:** Saul. Remove the surface. Energy + timing + chain are the
survival/skill/score axes going forward.

### saul-burnout-design-removal

# Decision Proposal — Burnout System Removal (issue #239)

**Owner:** Saul (Game Design)
**Status:** PROPOSED — design-doc pass complete; engineering follow-up required
**Issue:** https://github.com/yashasg/friendly-parakeet/issues/239

## Decision

The **Burnout** risk/reward scoring mechanic is removed from the game design.

- It is no longer a core pillar.
- It is no longer a scoring axis (no `× burnout_multiplier` term).
- It is no longer a player-facing system (no burnout meter, no burnout zones, no burnout popups, no "wait for more points" tutorial beat).
- It is no longer a failure path (the burnout DEAD-zone game-over rule is gone; failure is owned solely by the energy bar).

## Rationale

Burnout (the longer you wait, the higher your multiplier) **structurally conflicts with rhythm-on-beat play**. A player who is staying on the beat is, by definition, not "delaying" their input. The two pillars were sending opposite signals:

- *Rhythm pillar:* "Press exactly on the beat. Earlier or later both lose points."
- *Burnout pillar:* "Press as late as you can get away with. The closer to the obstacle, the higher the multiplier."

Diagnostic notes (recorded earlier in `.squad/agents/saul/history.md`) already showed burnout was never doing what the doc claimed it did at the code level — `COLLISION_MARGIN=40` < `ZONE_DANGER_MIN=140` collapsed the multiplier ladder onto a single value. Issue #167 patched the symptom (bank-on-action). #239 removes the cause.

## What Replaces It

The rhythm scoring model already in `rhythm-design.md` and `rhythm-spec.md`:

- **Scoring formula:** `base_points × timing_multiplier × chain_multiplier`
- **Timing multiplier:** Perfect / Good / Ok / Bad, computed from the input's distance to the beat at obstacle resolution.
- **Chain:** consecutive non-Miss hits.
- **Visual cue:** the **proximity ring** around shape buttons (already shipped) is the live timing readout that used to be served by the burnout meter.
- **Failure:** energy bar drain on Miss/Bad (see `energy-bar.md`).

**Crucial player-facing rule:** rhythmically on-beat shape changes are *valid play and not penalised* just because no obstacle is currently near arrival. The player is allowed (and encouraged) to feel the music with their inputs.

## Doc Changes (this pass)

Rewrites:
- `README.md` — removed `burnout` from the system pipeline diagram.
- `design-docs/game.md` (v1.1 → v1.2) — concept, core mechanic, obstacle table, scoring, difficulty, emotion arc, game loop, HUD, references.
- `design-docs/game-flow.md` — added doc-level deprecation banner; **rewrote Tutorial Run 4** from "Risk & Reward — Burnout meter intro" to "Stay on the Beat — On-beat timing intro" using the proximity ring + PERFECT/GOOD/OK/BAD popups; updated Run 5 summary.
- `design-docs/rhythm-design.md` — scoring formula now `base × timing × chain` (dropped `× burnout_mult`).
- `design-docs/energy-bar.md` — removed claim that "burnout multiplier remains the scoring risk/reward mechanic"; HUD slot text updated.
- `design-docs/tester-personas.md` — Pro player validation bullet rewritten ("Timing windows reward skilled on-beat play").
- `design-docs/tester-personas-tech-spec.md` — removed `burnout_system` from system pipeline and dependency chain; `hp_system` → `energy_system`.
- `design-docs/beatmap-integration.md` — pipeline diagram cleaned (burnout/hp removed, energy added); `SongResults.best_burnout` field removed.
- `docs/ios-testflight-readiness.md` — background-preservation table no longer lists burnout; chain count listed instead.
- `docs/sokol-migration.md`, `design-docs/normalized-coordinates.md`, `design-docs/isometric-perspective.md` — incidental burnout mentions cleaned.

Banners (engineering docs preserved as archival):
- `design-docs/architecture.md` — banner at top noting `BurnoutState`/`BurnoutZone`/`BankedBurnout`/`burnout_system`/`BURNOUT_*` constants are historical; rhythm-spec is authoritative.
- `design-docs/feature-specs.md` — banner noting SPEC 2 (Burnout Scoring System) is fully superseded.
- `design-docs/prototype.md` — banner noting BURNOUT meters and ×1→×5 popups in the ASCII frames are archival illustration only.

## Engineering Follow-Up (NOT done in this pass)

This decision is a *design* decision; the doc pass does not touch code or tests. The engineering team owns the corresponding cleanup, which at minimum will need to:

1. Remove `BurnoutState`, `BurnoutZone`, `BankedBurnout` components and `burnout_system.cpp` from `app/`.
2. Remove `multiplier_from_burnout` and the `× burnout_multiplier` term from `scoring_system.cpp`. Scoring becomes `base × timing_grade_multiplier × chain_multiplier`.
3. Remove `BURNOUT_*` constants and the burnout meter HUD draw call from `render_system.cpp`.
4. Remove `SongResults.best_burnout` and any tests/serialization that read it.
5. Drop `[burnout]`, `[burnout_bank]` Catch2 tags and the tests under them; keep `[scoring]` tests but expect the simpler formula.
6. Confirm the `LanePush` no-score path is still respected (now even simpler, with no banking).

Owner suggestion: McManus (gameplay) implements; Verbal/Baer covers tests; Kujan reviews; Saul signs off on player-facing scoring numbers when the new chain-only ladder is tuned.

## Non-Goals

- **No new mechanic** is being introduced to "fill the gap" left by burnout. The proximity ring and chain already carry that weight; adding anything else would re-introduce the same conflict with on-beat play.
- **Difficulty content (#125, #134, #135) is unchanged.** Easy/medium/hard contracts (shape-gate-only easy, taught-LanePush medium, bars on hard) remain intact.
- **Energy bar tuning is not revisited here** — it was tuned independently and continues to be the sole survival meter.

### saul-cleanup-miss-split-280

# #280 — Split Scroll-Past Miss Detection out of cleanup_system

**Author:** Saul (Game Design)
**Status:** ✅ APPROVED — ready for engineering pickup
**Issue:** https://github.com/yashasg/friendly-parakeet/issues/280
**Date:** 2026-05-17

## Decision

The proposed refactor is **design-correct**. `cleanup_system` is destroying entities AND mutating `EnergyState` / `SongResults` (cleanup_system.cpp:19–28); split the miss decision into its own system and let `scoring_system` remain the single owner of grade→energy effects.

## Required Behavior (Player-Facing — Must Not Regress)

1. An obstacle that scrolls past `DESTROY_Y` without ever being scored still counts as a miss.
2. That miss drains energy by `ENERGY_DRAIN_MISS`, clamped at 0.
3. That miss increments `SongResults::miss_count` by exactly 1.
4. That miss resets `ScoreState::chain_count` and `chain_timer` (currently handled by scoring_system's MissTag branch — preserve).
5. If the drain takes energy to 0, Game Over fires this frame, not next frame (current behavior also satisfies this only by coincidence — see Bonus Win).
6. `BankedBurnout`, `TimingGrade`, and `Obstacle` components are removed from the entity once the miss is processed (already covered by scoring_system's MissTag branch — keep).

## Engineering Constraints / Acceptance Criteria

**System layout**
- New system: `miss_detection_system(entt::registry&, float dt)` in `app/systems/`.
- Frame order in `game_loop.cpp` (run loop): `... collision_system → miss_detection_system → scoring_system → energy_system → ... → cleanup_system`.
  - **Must** run before `scoring_system` so the MissTag is processed the same frame.
  - **Must** run before `energy_system` so an energy-zeroing miss triggers Game Over without one-frame latency.

**miss_detection_system rules**
- Query: `reg.view<ObstacleTag, Obstacle, Position>(entt::exclude<ScoredTag>)`.
- For each entity where `pos.y > constants::DESTROY_Y`: `reg.emplace<MissTag>(e); reg.emplace<ScoredTag>(e);`.
- No direct mutation of `EnergyState`, `ScoreState`, or `SongResults` from this system. It is a pure tagger.
- LanePush obstacles are already `ScoredTag`-stamped by `collision_system.cpp:192` at pass-through, so they will not be tagged by this system. Do not add a kind-based exclusion; exclude-by-`ScoredTag` is the canonical guard.

**scoring_system extension (MissTag branch, scoring_system.cpp:36–44)**
- When processing a `MissTag` obstacle, additionally:
  - `energy->energy -= constants::ENERGY_DRAIN_MISS;` then clamp `energy->energy = max(0, energy->energy)`.
  - `results->miss_count++;` (guard `find<SongResults>()`).
  - Optionally trigger `energy->flash_timer = ENERGY_FLASH_DURATION` (parity with Bad timing — recommended for visual feedback consistency, but not required for sign-off).
- Existing chain reset and component cleanup remain as-is.

**cleanup_system reduction**
- After the change, `cleanup_system` only contains the `to_destroy.push_back(entity)` paths — i.e. destroy any entity past `DESTROY_Y` that has `ScoredTag` (or has `ObstacleTag` but no `Obstacle`, for already-stripped entities and decorations).
- Remove the `EnergyState` / `SongResults` writes entirely. No `MissTag`/`ScoredTag` emplace calls from this system.

**Edge cases**
- An obstacle that gets `ScoredTag` from `collision_system` *and* scrolls past in the same frame must not be double-counted. Exclude-by-`ScoredTag` in miss_detection_system handles this; do not key on `MissTag` presence.
- If `EnergyState` is absent from ctx (test fixtures), scoring_system's MissTag branch must no-op the energy drain gracefully (use the existing `find<EnergyState>()` pattern — already in place at scoring_system.cpp:34).

## Tests (must pass post-refactor)

- `test_death_model_unified` "cleanup miss drains energy" — keep green; consider renaming to "scroll-past miss drains energy" since cleanup is no longer the actor.
- Add a new test: a scroll-past miss whose drain takes energy from `ENERGY_DRAIN_MISS` to 0 transitions `GameState.phase` to `GameOver` **in the same frame** (this is the latency-bug regression guard the refactor enables).
- Add a new test: scroll-past miss does **not** double-drain (energy delta is exactly `-ENERGY_DRAIN_MISS`, miss_count delta is exactly 1).
- LanePush passing the player and scrolling off must not produce a `MissTag` or any energy delta (current behavior — keep green).

## Bonus Win (Call Out in PR Description)

The current cleanup-time energy drain runs *after* `energy_system` in the frame, so a scroll-past miss that should kill the player only triggers Game Over on the next frame. Moving the decision before `energy_system` closes that latency hole. Engineering should call this out so QA validates the same-frame Game Over assertion.

## Owners

- **Implementation:** McManus (gameplay systems)
- **Tests:** Verbal (edge cases) + Baer (regression)
- **Review:** Kujan
- **Design sign-off:** Saul (this note)

### verbal-burnout-removal-tests

### 2026-04-27 — Verbal: test cleanup for burnout removal (#239)

**Decision:** Delete `test_burnout_system.cpp` and `test_burnout_bank_on_action.cpp` entirely (not stub them) since CMakeLists uses `file(GLOB TEST_SOURCES tests/*.cpp)` — deleted files are automatically excluded.

**Decision:** `test_helpers.h :: make_registry()` still emplace `BurnoutState` because `test_haptic_system.cpp`, `test_death_model_unified.cpp`, and `test_components.cpp` still reference it. Removing it now would break those tests before McManus removes the component header. McManus/Hockney own that cleanup.

**Decision:** Added `[no_burnout]` tag to the new no-penalty test so future runs can target it in isolation.

**Out-of-scope flag:** `test_haptic_system.cpp` has 5 `burnout_system()` call sites (lines 75, 87, 99, 112, 125) that will break when McManus removes `burnout_system`. These need a follow-up task for Hockney or McManus.

---

### Archetype Candidate Audit (2026-04-27)

**Owners:** Keyser (ECS/archetype analysis), Keaton (duplicate audit), Coordinator (routing)  
**Status:** AUDIT COMPLETE — findings ranked for implementation

Read-only audit of `app/systems/` and `tests/test_helpers.h` identified entity construction patterns suitable for extraction into `app/archetypes/` factories or construction helpers.

#### Keyser's Findings: Five Ranked Candidates

**C1 — Obstacle base pre-bundle dedup · P1 · Under #344**
- **Current:** `beat_scheduler_system.cpp:50-53`, `obstacle_spawn_system.cpp:43-46` both emit 3-component preamble (ObstacleTag + Velocity + DrawLayer) before calling `apply_obstacle_archetype`
- **Proposed:** Fold preamble into `apply_obstacle_archetype` signature or create `create_obstacle_base(reg, speed)` helper
- **Preferred:** Option (a) — eliminates split contract entirely
- **Blocker:** Update `test_obstacle_archetypes.cpp` for new signature
- **Risk:** Low — mechanical change, both callers well-tested

**C2 — Player entity archetype · P1 · New issue recommended**
- **Current:** `play_session.cpp:146-165` (8 inline emplaces) — PlayerTag, Position, PlayerShape, ShapeWindow, Lane, VerticalState, Color, DrawSize, DrawLayer
- **Proposed:** `create_player_entity(reg) -> entt::entity` in `app/archetypes/player_archetypes.h/.cpp`
- **Blocker:** No existing player archetype test; must be written before or alongside implementation
- **Risk:** Low — construction is self-contained

**C3 — Score popup entity factory · P2 · New child issue of #344 or standalone**
- **Current:** `scoring_system.cpp:149-175` (7 inline emplaces in hit-pass loop)
- **Proposed:** `spawn_score_popup(reg, pos, points, has_timing, tier)` in `app/archetypes/popup_archetypes.h/.cpp`
- **Blocker:** None — `test_popup_display_system.cpp` coverage is sufficient
- **Risk:** Low

**C4 — Shape button orphan in play_session · P2 · New child issue**
- **Current:** `play_session.cpp:167-188` (3-entity loop with layout math) — all other button types in `ui_button_spawner.h`, shape buttons orphaned
- **Proposed:** Add `spawn_shape_buttons(reg)` inline function to `ui_button_spawner.h`
- **Blocker:** None
- **Risk:** Very low

**C5 — `app/gameobjects/` vs `app/archetypes/` · P3 · Deferred**
- **Current:** `spawn_obstacle_meshes` + `on_obstacle_destroy` in `app/gameobjects/shape_obstacle.cpp` are pure entity-assembly functions
- **Proposed:** Move to `app/archetypes/` alongside `obstacle_archetypes.cpp`
- **Blocker:** Avoid double-touching include graph; defer until after C1

**Explicit non-candidates:**
- `enter_game_over` / `enter_song_complete` (state transitions, not construction)
- `spawn_ui_elements` (data-driven JSON→entity loop; extracting would over-engineer)
- `ScoredTag/MissTag` emplaces (state mutations on existing entities, not construction)
- `ActiveTag` emplace (phase-gating structural tag, belongs in system)

**Recommended routing:**
- C1 → assigned to implementer under #344 (already in scope)
- C2 → new issue, assigned to implementer (factory + test)
- C3, C4 → child issues of #344 or standalone P2 queue
- C5 → P3 backlog, do after C1

#### Keaton's Findings: Duplicate Construction Audit

**P0: Test helpers must migrate to `apply_obstacle_archetype`** (BLOCKING)
- `tests/test_helpers.h` contains six obstacle factory helpers that bypass `apply_obstacle_archetype` and hardcode component values
- **Color contract divergences:**
  - `make_vertical_bar` uses yellow (should be purple `{180, 0, 255, 255}`)
  - `make_lane_push` uses cyan `{0, 200, 200, 255}` (should be `{255, 138, 101, 255}`)
  - `make_shape_gate` always uses white (archetype is shape-conditional)
- **Recommended action:** Before implementing P1 or P2, fix test helpers to call `apply_obstacle_archetype` + new `create_obstacle_base` helper

**P1a: Extract `create_obstacle_base` into `obstacle_archetypes.h`**
- `beat_scheduler_system.cpp` and `obstacle_spawn_system.cpp` both have identical 4-emplace base-header patterns
- Helper signature: `create_obstacle_base(entt::registry&, float speed) -> entt::entity`
- Eliminates 2-site duplication, becomes authoritative pre-archetype setup

**P1b: Extract `create_player_entity` into `app/archetypes/player_archetype.h`**
- `play_session.cpp` and `test_helpers.h::make_rhythm_player` construct logically identical player entities in slightly different ways
- Canonical factory ensures test and production use same component bundle and default field values

**P2: Refactor `ui_button_spawner.h` internally**
- Eight button creation sites all repeat same 6-emplace MenuButton pattern
- Solution: private `spawn_menu_btn_at(...)` inline helper within the header
- No new archetype file needed

**Not recommended:**
- Score popup: single production site, test helper diverges intentionally for isolation
- Particles: no production emitter yet
- UI elements (ui_navigation_system.cpp): data-driven from JSON, not duplicated construction

#### Integration Decision

**Migration order (Keaton + Keyser consensus):**
1. P0 test helper fix (Keaton → test_helpers.h color harmonization)
2. P1a obstacle base extraction (Keyser/implementer → obstacle_archetypes.h)
3. P1b player archetype extraction (Keyser/implementer → player_archetypes.h)
4. P2 button factory consolidation (Keaton/implementer → ui_button_spawner.h)
5. C3, C4 backlog or scheduled per capacity
6. C5 P3 backlog, schedule after C1 complete


---

### #SQUAD Comment Cleanup (2026-04-27)

**Owners:** Keaton (implementation), Kujan (review)  
**Status:** APPROVED / IMPLEMENTED

## Decision: shape_vertices.h and transform.h cleanup

The `#SQUAD` comment in `shape_vertices.h` claimed raylib 2D draw calls (`DrawCircle`, `DrawRectangle`, `DrawTriangle`) could replace the vertex tables. This was incorrect: `CIRCLE` is used in `game_render_system.cpp` for 3D `rlgl` ring geometry via `rlBegin(RL_TRIANGLES)`. There is no 2D equivalent for this path.

### shape_vertices.h — Partial cleanup (implemented)

**Removed:** `HEXAGON`/`HEX_SEGMENTS`, `SQUARE`, `TRIANGLE` — unused in production code.  
**Kept:** `CIRCLE`, `CIRCLE_SEGMENTS`, `V2` struct — required by the 3D floor renderer and its tests.  
**Test/benchmark files updated:** `tests/test_perspective.cpp` and `benchmarks/bench_perspective.cpp` modified to match deletions.

### transform.h — Position/Velocity retained (implemented)

The original `#SQUAD` comment suggested replacing `Position`/`Velocity` with `Vector2`. This is not straightforward:
- Both are used across 21 production system files as distinct ECS component types.
- Merging them would collapse two separate EnTT component pools, breaking structural view semantics.
- It's a broad rename with correctness implications, not a local cleanup.

**Decision:** Keep `Position` and `Velocity` as separate named structs. Comment updated to explain the rationale in-code: separate structs = separate EnTT component pools = no aliasing in registry views.

## Review Results

### Kujan Approval

- ✅ `HEXAGON`/`SQUARE`/`TRIANGLE` removal safe (zero production references confirmed)
- ✅ `CIRCLE` retention necessary (actively consumed by `game_render_system.cpp` for rlgl 3D geometry)
- ✅ `Position`/`Velocity` separation is correct ECS architecture decision
- ✅ Test/benchmark deletions align exactly with deleted data
- ✅ Circle and floor ring coverage preserved, no orphaned tests

**Non-blocking note:** `transform.h` retains orphaned `#include <cstdint>` (pre-existing, low priority).

## Validation

- `rg #SQUAD`: Zero matches (all markers replaced/resolved)
- Tests: `cmake --build build --target shapeshifter_tests` passed
- Coverage: `./build/shapeshifter_tests "[shape_verts],[floor]"` passed (60 assertions, 5 test cases)


---

### ECS Cleanup Wave: Approval Batch (2026-04-28)

**Owners:** Hockney, Fenster, Baer, Kobayashi, McManus, Keyser, Kujan  
**Status:** APPROVED / IMPLEMENTED  

## Decision: Render Tags Component → Folded into Rendering

**Author:** Hockney  
**Date:** 2026-04-28

`app/components/render_tags.h` deleted. `TagWorldPass`, `TagEffectsPass`, `TagHUDPass` moved to the end of `app/components/rendering.h`.

**Rationale:** Per directive and Kujan gate — no new component headers during cleanup passes. All three tags are empty structs with no dependencies, so folding them into an existing header is zero-cost.

**Impact:**
- Production includes: `render_tags.h` removed; `rendering.h` already present
- Tests: include replaced with comment; tags visible via `rendering.h`
- Build: zero warnings, all tests pass

## Decision: Non-Component Header Cleanup

**Author:** Fenster  
**Date:** 2026-04-28

Six headers removed from `app/components/` that were not ECS components:

| Deleted | Replacement |
|---|---|
| `audio.h` | `app/systems/audio_types.h` (duplicate) |
| `music.h` | `app/systems/music_context.h` (duplicate) |
| `settings.h` | `app/util/settings.h` (duplicate) |
| `shape_vertices.h` | `app/util/shape_vertices.h` (moved) |
| `obstacle_counter.h` | `app/systems/obstacle_counter_system.h` (duplicate) |
| `obstacle_data.h` | folded into `app/components/obstacle.h` |

**Impact:** `app/components/obstacle.h` now includes `player.h` and defines `RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction`. All 2978 test assertions pass.

## Decision: Dead ECS File / Build Cleanup

**Author:** Kobayashi  
**Date:** 2026-04-28

**Files Deleted:**
- `app/components/ring_zone.h` — `RingZoneTracker` had no consumers
- `app/systems/ring_zone_log_system.cpp` — diagnostic logger, no gameplay effect
- `app/archetypes/obstacle_archetypes.h/cpp` — superseded by `app/entities/obstacle_entity.h/cpp`
- `app/systems/obstacle_archetypes.h/cpp` — duplicate

**CMakeLists Change:** Added `file(GLOB ENTITY_SOURCES app/entities/*.cpp)` and wired into `shapeshifter_lib`.

**Build Policy:** C++20 designated initializers (`{.kind = ..., .x = ...}`) required for `ObstacleSpawnParams` to suppress `-Wmissing-field-initializers`.

**Validation:** Zero warnings, 887 test cases (2983 assertions) pass. Commit: `0d642e2`

## Decision: Obstacle Entity Layer — app/entities/ Introduction

**Author:** McManus  
**Date:** 2026-04-28

Introduced `app/entities/` as the canonical surface for named gameplay entity construction. Obstacle entity/component bundle logic now owned exclusively by `app/entities/obstacle_entity.*`.

**What Changed:**
- **New:** `app/entities/obstacle_entity.h` — `ObstacleSpawnParams` struct + `spawn_obstacle(reg, params, beat_info*)`
- **New:** `app/entities/obstacle_entity.cpp` — single implementation of obstacle bundle contract
- **Deleted:** `app/archetypes/obstacle_archetypes.h/cpp` — fully superseded
- **Updated:** `obstacle_spawn_system.cpp`, `beat_scheduler_system.cpp` — call `spawn_obstacle` directly

**API Contract:**
```cpp
struct ObstacleSpawnParams {
    ObstacleKind kind;
    float x = 0.0f, y = 0.0f;
    Shape shape = Shape::Circle;
    uint8_t mask = 0;
    int8_t req_lane = 0;
    float speed = 0.0f;
};

entt::entity spawn_obstacle(entt::registry& reg,
                             const ObstacleSpawnParams& params,
                             const BeatInfo* beat_info = nullptr);
```

**Why pointer for BeatInfo:** `std::optional<BeatInfo>` triggers `-Wmissing-field-initializers` on all positional brace-init callsites under `-Wextra -Werror`. Nullable pointer keeps struct trivially aggregate-initializable and zero-overhead.

## Decision: Cleanup Gate Fix — Evidence Report

**Author:** Keyser  
**Date:** 2026-04-28  
**Gate:** kujan-component-cleanup-review  
**Status:** GATE PASSED

**Three Blockers — Resolution:**
1. `obstacle_entity.cpp` stale include of `components/obstacle_data.h` — ✅ FIXED (only valid includes present)
2. `test_obstacle_archetypes.cpp` `-Wmissing-field-initializers` warnings — ✅ FIXED (uses `ObstacleSpawnParams` with trailing defaults)
3. `test_obstacle_archetypes.cpp` deleted include of `archetypes/obstacle_archetypes.h` — ✅ FIXED (now includes `entities/obstacle_entity.h`)

**Full Stale-Include Scan:** All 10 deleted headers scanned; zero hits in `app/` and `tests/`.

**Archetype Dedup:** All old archetype files confirmed absent; canonical surface (`entities/obstacle_entity.*`) present and wired into CMakeLists.

**Model.transform Scope:** `ObstacleScrollZ` emitted only for LowBar/HighBar. No expansion creep.

**Build Validation:** All commands clean; 2983 assertions / 887 test cases pass; zero warnings.

## Decision: Re-Review — Component Cleanup / Entity Layer / Model Scope

**Author:** Kujan  
**Date:** 2026-04-28  
**Status:** ✅ APPROVED  

**Prior Rejection Blockers — All Resolved:**

| Blocker | Status |
|---------|--------|
| `obstacle_entity.cpp` included deleted `components/obstacle_data.h` | ✅ FIXED |
| `test_obstacle_archetypes.cpp` `-Wmissing-field-initializers` | ✅ FIXED |
| Deleted include of `archetypes/obstacle_archetypes.h` | ✅ FIXED |

**Gate Criteria — All Pass:**

| Criterion | Result |
|-----------|--------|
| No stale includes of any deleted header | ✅ PASS |
| `obstacle_entity.*` present, compiles, wired into CMakeLists | ✅ PASS |
| `app/archetypes/obstacle_archetypes.*` deleted | ✅ PASS |
| `app/systems/obstacle_archetypes.*` deleted | ✅ PASS |
| No duplicate obstacle bundle API | ✅ PASS |
| `render_tags.h` gone as standalone header | ✅ PASS |
| No new component clutter created | ✅ PASS |
| User-called non-component headers deleted/relocated | ✅ PASS |
| `Model.transform` scope narrow (LowBar/HighBar only) | ✅ PASS |
| Build clean (zero warnings) | ✅ PASS |
| Tests pass (2983 assertions / 887 test cases) | ✅ PASS |

**Non-Blocking Notes:**
- `app/components/difficulty.h.tmp` leftover temp file (should be deleted in housekeeping)
- Test comments referencing old `render_tags.h` are stale but code is correct
- `ui_layout_cache.h` remains (acknowledged deferred item)

**Verdict:** APPROVED. Cleanup artifact meets all gate criteria; three prior blockers resolved; canonical entity surface correct; archetype duplication eliminated; render tags properly folded; Model.transform migration scope narrow.
