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
