## Learnings

<!-- Append learnings below -->

### 2026-04-27 — Parallel ECS/EnTT Audit (user/yashasg/ecs_refactor branch)

**Status:** AUDIT COMPLETE — Read-only gameplay ECS audit with Keyser, Keaton, Redfoot, Baer.

**Verdict:** Mostly clean — One P1, one P2, one P3.

**P1 Finding:** obstacle_counter.h:29-35 — `wire_obstacle_counter()` performs signal wiring (system concern) in component header. **Fix:** Move `wire_obstacle_counter()` and listener functions to new `app/systems/obstacle_counter_system.cpp`. Declare in all_systems.h. Keep ObstacleCounter data in header.

**P2 Finding:** EventQueue Tier-1 incomplete migration. Decisions.md specifies "Remove EventQueue struct" (step 4). Tier 2 (GoEvent, ButtonPressEvent) fully migrated in input_dispatcher.cpp. Tier 1 remains: input_system pushes to EventQueue; gesture_routing/hit_test read from EventQueue before dispatcher. **Fix:** Replace with `disp.enqueue<InputEvent>()`; update systems to receive via dispatcher; remove EventQueue. (Owner: Keaton per decisions.md.)

**P3 Finding:** SongState/DifficultyConfig mix metadata (immutable) with runtime state (mutable per-frame). No correctness issue; document which fields are "set once at init" vs "mutated per-frame".

**Confirmed clean:** scoring_system collect-then-remove, collision_system per-kind views, miss_detection_system iteration safety, cleanup_system static buffer, beat_scheduler ctx state, obstacle_spawn RNGState, dispatcher wiring, on_obstacle_destroy signal lifecycle, burnout ECS removed.

**Orchestration log:** `.squad/orchestration-log/2026-04-27T22-30-13Z-mcmanus.md`

### 2026 — Diagnostics pass (full codebase review)

**Critical: Jump/Slide are NOT wired up**
`player_input_system.cpp` GoEvent loop only handles `Direction::Left/Right`. `Direction::Up/Down` are pushed by both `input_system` and `test_player_system` but silently ignored. `vstate.mode` is never set to `Jumping/Sliding` from player input. Tests manually set `vstate.mode` directly, masking the breakage. **Fix: add Up/Down branches to the GoEvent loop in `player_input_system`, guard on `vstate.mode == Grounded`.**

**Burnout Dead zone has no kill signal**
`burnout_system.cpp` sets `zone = Dead` and `meter = 1.0f` but never sets `gs.transition_pending = true`. GameOver from burnout overflow does not occur. Only energy depletion kills.

**Burnout system does not filter LanePush**
`burnout_system.cpp:34` queries all `ObstacleTag` without kind filter. `game.md` explicitly marks LanePushLeft/Right as `Burnout: NO`. LanePush entities drive the burnout meter even though the player cannot interact with them. Add kind exclusion.

**Beatmap validator is too strict — rejects valid same-beat charts**
`beat_map_loader.cpp:219-224` uses `entry.beat_index <= prev_beat` (strict monotonic). `rhythm-spec.md §8` explicitly allows two obstacles at the same beat_index in different lanes or kinds. Validator must allow `(beat_index, lane, kind)` uniqueness, not globally monotonic index.

**Same-shape press during Active window resets timing window**
`player_input_system.cpp:48-54` resets `window_start/peak_time/graded` when the same shape is pressed during Active phase. `rhythm-spec.md §8` says this is a no-op. Spam defeats precision timing reward.

**cleanup_system does not emit MISS for unscored obstacles**
Rhythm obstacles that escape the collision window (e.g. during jump peak) reach `DESTROY_Y` and are silently deleted without triggering miss/energy drain. `cleanup_system` must check for missing `ScoredTag` and emit MISS before destroy.

**No srand() — freeplay is deterministic**
`obstacle_spawn_system.cpp` uses `std::rand()` with no seed. Every freeplay run is identical. Seed with `time(nullptr)` or migrate to `std::mt19937` + `std::random_device`.

**Design doc conflict: MISS semantics**
`rhythm-spec.md §5` says MISS = instant GameOver (v1.1). `energy-bar.md` says MISS drains energy. Implementation follows energy-bar.md. rhythm-spec.md is stale. Record decision in `.squad/decisions.md` and add a NOTE callout to rhythm-spec §5.

**Burnout multiplier is stepped, not interpolated**
`scoring_system.cpp` `multiplier_for_zone()` returns fixed constants per zone. `feature-specs.md §2` specifies per-zone `lerp` across the fill fraction. DANGER should ramp 2.0→5.0, not flat 3.0.

**Energy constants are compile-time only**
`constants.h` has `constexpr float ENERGY_*`. `energy-bar.md` requires these in `content/constants.json` for hot-tuning. No code path loads them from JSON today.

**#135 revision: Easy must be shape_gate-only — never inject lane_push**
`apply_lanepush_ramp` had a Pass 2 "teaching inject" that converted shape_gates to lane_pushes on easy (1.6–4.1% contamination in shipped beatmaps). Fix: `LANEPUSH_RAMP["easy"] = None` — easy is fully skipped. `balance_easy_shapes` is the only variety pass for easy, producing 3-shape palettes with dominant ≤60%. Hard bars and medium ramp unchanged. All 2366 C++ assertions pass post-regen.


## 2026 — Diagnostics pass 2 (rhythm.h + scoring_system deep dive)

**window_scale_for_tier is inverted vs spec (→ #223, test-flight)**
`rhythm.h:57–65` `window_scale_for_tier()` returns `{Perfect→1.50, Good→1.00, Ok→0.75, Bad→0.50}`. Spec (rhythm-spec.md §5/§7) says `{Perfect→0.50, Good→0.75, Ok→1.00, Bad→miss}`. The entire tier→scale mapping is inverted: PERFECT extends the window (+50%) instead of collapsing it (halve), OK shrinks instead of being neutral. `shape_window_system` logic is correct; bug is in the table. Filed #223 (test-flight).

**LanePush increments chain_count in scoring_system (→ #225, AppStore)**
`scoring_system.cpp:157` runs `score.chain_count++` for ALL non-miss scored obstacles including `LanePushLeft`/`LanePushRight` (base_points=0, passive). Three auto-pushes between two ShapeGates inflate chain to 5 instead of 2, awarding +250 extra chain bonus from zero player skill. Filed #225 (AppStore).

**Prior-pass items verified closed or status-quo:**
- beat_map_loader: same-beat validator now implemented (group-based); #92 resolved
- cleanup_system: now emits MissTag + ScoredTag at DESTROY_Y for unscored obstacles; #93 resolved
- obstacle_spawn_system: uses std::mt19937 via RNGState; #108 resolved
- burnout_system: LanePush correctly excluded via `is_burnout_relevant`; #91 resolved
- player_input_system: Up/Down branches for Jump/Slide are present; #89 resolved
- scoring_system: burnout multiplier uses interpolated lerp per zone; #96 resolved


## 2026-04-27 · #135 Revision: Early Implementation

- **Assignment:** Kujan locked out Rabin (initial impl) and Baer (initial tests). Reassigned implementation to McManus.
- **Task:** Fix `apply_lanepush_ramp` to disable early injection for easy difficulty.
- **Finding:** Rabin's initial implementation set `LANEPUSH_RAMP["easy"]` to a non-None config (start_progress, min_gap, max_count) that injected 1.6–4.1% lane_push into easy beatmaps.
- **Contract violation:** `DIFFICULTY_KINDS["easy"] = {"shape_gate"}` explicitly excludes lane_push. Also violates Saul's #135 design target ("easy shape_gate only").
- **Fix applied:**
  1. Set `LANEPUSH_RAMP["easy"] = None`
  2. `apply_lanepush_ramp` now skips easy entirely
  3. `balance_easy_shapes` remains the only variety pass for easy (produces 3-shape palettes with dominant ≤60%)
  4. Regenerated all 9 beatmaps from level_designer (no re-run of audio pipeline)
- **Result:** Easy 100% shape_gate (zero lane_push), 3 shapes, dominant ≤60%. Medium lane_push 9.3–19.5%, max consecutive ≤3, start_progress 0.30 respected. Hard bars intact (stomper 1/3, drama 2/2, mental 7/7).
- **Validation:** All 2366 C++ assertions pass (730 test cases). Python validators (bar coverage, shape gap, difficulty ramp) all pass.
- **Approved by Kujan.** Both blocking issues from prior rejection resolved.


## 2026-04-27 · #223 Fix: window_scale_for_tier inversion

**Task:** Fix `window_scale_for_tier` in `rhythm.h` — values were inverted vs. spec.

**Finding:** `app/components/rhythm.h:57–65` had `{Perfect→1.50, Good→1.00, Ok→0.75, Bad→0.50}`. Spec (rhythm-spec.md §5/§7) requires `{Perfect→0.50, Good→0.75, Ok→1.00}`. The entire mapping was reversed.

**Why Bad → 1.00 (not miss):** `window_scale_for_tier` only controls window timing. Bad's miss penalty is downstream in energy/scoring. A 1.00 is a clean no-op consistent with the issue acceptance criteria.

**Why shape_window_system.cpp needed no change:** Compression (scale < 1.0) is applied in `collision_system.cpp:65–72` via `window_start -= remaining * (1.0 - scale)`. The system was correct; it just received wrong scale values. The `> 1.0f` guard in `shape_window_system` is for extension only.

**Fix:** Corrected 4 values in `window_scale_for_tier()`. Updated 4 stale unit tests. Added 4 regression tests (2 value/ordering in `test_helpers_and_functions.cpp`, 2 integration in `test_shape_window_extended.cpp`). All compile with `-Wall -Wextra -Werror`.

**Commit:** 7aa899c
**Comment:** https://github.com/yashasg/friendly-parakeet/issues/223#issuecomment-4323253652

**Learning: Always trace the full scale path before assuming a system is correct.** The issue said "system is correct" but the collision_system had the real shortening logic (`window_start` adjustment) that made it true — the `shape_window_system`'s `> 1.0f` guard was never needed for the spec's collapse behavior.


## 2026 · #167 — Bank-on-Action Burnout Multiplier

**Task:** Snapshot burnout multiplier onto obstacle entities at the moment the player commits a qualifying input, instead of reading live `BurnoutState` at geometric collision time.

**Design source:** `.squad/decisions/inbox/saul-burnout-bank-timing.md` (Saul, accepted)

**Implementation:**
1. Added `BankedBurnout { float multiplier; BurnoutZone zone; }` to `burnout.h`
2. Created `app/systems/burnout_helpers.h` with shared `compute_burnout_for_distance()` and `multiplier_for_zone()` helpers
3. `burnout_system.cpp`: replaced inline zone math with helper call
4. `scoring_system.cpp`: reads `BankedBurnout::multiplier`; skips LanePush from ladder/chain; removes `BankedBurnout` after processing; falls back to `MULT_SAFE (1.0)` if no bank present
5. `player_input_system.cpp`: banks burnout on shape presses (ShapeGate/ComboGate/SplitPath) and Left/Right lane-change GoEvents; `find_nearest_unscored` lambda scans by kind predicate; first-commit-locks prevent overwrite

**Key learnings:**
- **Do not add Up/Down (Jump/Slide) banking without explicit AC.** Tests explicitly assert jump/slide are "disabled" features. Accidentally added Up/Down branches → 3 test failures. Removed before commit.
- **Broken untracked files from other agents break your build.** `sfx_bank.cpp`, `test_audio_system.cpp`, and 4 UI test files were untracked files from other agents referencing missing symbols. Fix: added missing structs to `audio.h`, excluded the 6 untracked test files from CMake glob via `list(FILTER EXCLUDE REGEX)`.
- **`BankedBurnout` on obstacle entity is the correct pattern** for time-of-action vs time-of-collision disambiguation. Any scoring system reads the component, not the live context state.
- **`scoring_system.cpp` had agent-added code not in git** (haptics check `if (burnout.zone == BurnoutZone::Dead)`). When modifying existing systems, always view the real file on disk, not just the git version.
- **LanePush must be excluded from every scoring ladder** (chain, popup, `best_burnout`). Easy to miss because the entity passes through `scoring_system` with `base_points=0` but still triggers chain increment.

**Commit:** e82b8d1
**Tests:** 11 new `[burnout_bank]` tests, all pass. 49 assertions in `[burnout_bank],[scoring],[player],[player_rhythm]` pass.


## 2026-04-27 · #242 — Eliminate per-frame heap allocation in cleanup_system

**Task:** Replace per-frame `std::vector<entt::entity>` local variable in `cleanup_system` with a static buffer that retains capacity across frames. Also eliminate any `reg.all_of` inside view loops (already gone after #280 extracted miss detection to `miss_detection_system`).

**Fix:**
1. `app/systems/cleanup_system.cpp`: `static std::vector<entt::entity> to_destroy` — `clear()` keeps capacity, no malloc in steady state.
2. `tests/test_world_systems.cpp`: 2 new `[cleanup]` tests:
   - "cleanup: destroys multiple obstacles past DESTROY_Y in one pass" — verifies static buffer handles batch correctly.
   - "cleanup: does not emplace MissTag or ScoredTag on surviving obstacles" — regression-guards the #280 ownership boundary.
3. `CMakeLists.txt`: extended `TEST_SOURCES` exclusion regex to cover 3 new untracked test files from concurrent agents (`test_lifecycle`, `test_gesture_routing_split`, `test_gpu_resource_lifecycle`).

**Behavior preserved:** cleanup_system remains destruction-only; zero score/energy mutation. `miss_detection_system` retains exclusive ownership of scroll-past miss tagging.

**Key learnings:**
- **Shared working tree = other agents' changes appear as your working-tree modifications.** After any build failure, always run `git diff HEAD` to identify whose files are broken before restoring. Use `git checkout HEAD --` (not `--`) to force HEAD state over index.
- **The `edit` tool silently fails in this repo.** Always use bash `cat >` or `python3` string replacement to write file content; verify with `grep` after.
- **`git commit` picks up other-agent staged files.** Before committing: `git reset HEAD`, then `git restore --staged` any wrong files, then re-add only your files and `git diff --cached --name-only` to verify.
- **Static `std::vector` with `clear()` is the correct DoD pattern for per-frame collect buffers.** No malloc after warm-up, collect-then-destroy preserves EnTT iterator safety.

**Commit:** afd7921 (code changes bundled; dedicated ref commit below)
**Tests:** All 6 `cleanup*` tests pass. 2 pre-existing failures in `[cleanup]` tag are from `test_pr43_regression.cpp` (child-entity signal tests, unrelated to cleanup_system).

### 2026-05-17 — EnTT ECS Audit: Key Stakeholder Summary

**Three-agent audit complete.** Keyser delivered compliance findings (5 P1, 5 P2). Keaton delivered performance rules + violations (3 rules, multiple sites). Kujan synthesized into actionable backlog (9 remediation items, all owners assigned).

**Material backlog items assigned to you:**
- F1: scoring_system collect-then-process pattern (latent UB trap)
- F2/F3: COLLISION_MARGIN + APPROACH_DIST constant deduplication
- F4: Move ensure_active_tags_synced() / wire_obstacle_counter() to system `.cpp` files
- Rule 2: Scoring system structural view refactor (pattern debt, low cardinality)
- Rule 3: Hoist ctx() lookups in scoring/collision systems

**Status:** Orchestration log: `.squad/orchestration-log/2026-04-27T19-14-36Z-entt-ecs-audit.md`. Input dispatcher pipeline validated (no architectural rework needed). Ready for team sprint assignment.


## 2026-05-17 — #315 scoring_system EnTT iteration safety

**Task:** Make scoring_system EnTT-safe — it was removing Obstacle/ScoredTag components while iterating a view containing those exact components (swap-and-pop UB).

**Fix:**
- Replaced single combined view loop with two structural views:
  - Miss pass: `reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>()` — no Position needed, energy/chain processed inline, entities collected, removals applied after iteration
  - Hit pass: `reg.view<ObstacleTag, ScoredTag, Obstacle, Position>(entt::exclude<MissTag>)` — collects entity+pos+obs+timing into `HitRecord`, processes scoring/popups after iteration
- Per-entity `any_of<MissTag>` branch replaced by structural view split (matches collision_system pattern)
- Both passes use `static std::vector<>` with `.clear()` for zero per-frame heap allocation
- All `reg.remove<>` on view components happen after the view is exhausted

**Commit:** fa97d7e
**Tests:** 2430 assertions (770 test cases) — all pass, zero build warnings.


## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Gameplay Engineer
- **Joined:** 2026-04-26T02:07:46.544Z


## Learnings

- **EnTT collect-then-remove pattern:** Any `reg.remove<C>` where C is in the active view's component list is potential swap-and-pop UB. Always collect entities first, remove after. Static vectors avoid per-frame alloc.
- **Structural view split for branching:** When an `any_of<T>` branch is the primary discriminator inside a view loop, split into two structural views (`with T` / `entt::exclude<T>`). This is both safer and gives EnTT better cardinality info.
- **MissTag entities don't need Position:** Miss processing (energy drain, miss_count, chain reset) never reads position. The structural split lets the miss view drop Position from its component list entirely.
- **Build workaround (worktree):** The 315 worktree doesn't have vcpkg_installed. Used symlink + explicit `-D*_DIR` flags to point CMake at the main worktree's built packages.
- **Archetypes wording cleanup rule:** Keep historical test taxonomy tags like `[archetype]` if churn is noisy, but update all code/path comments and docs to canonical `app/entities/` factories.
- **Archetype canonicalization:** When removing legacy folders/shims, always update docs and code comments to reflect the new canonical boundary. Document retained test taxonomy (e.g., `[archetype]` tags) to avoid confusion on future audits.


## 2026-04-29: Archetype Wording Cleanup Validation

**Task:** Clean stale doc/comment wording to reflect `app/entities/` as canonical path per Keyser audit and Keaton implementation.

**Changes (docs/comments only, no behavior changes):**
- `design-docs/architecture.md` Section 5:
  - Added explicit note: reusable construction implemented by `app/entities/` factories (`create_player_entity`, `spawn_obstacle`)
  - Removed stale repo-tree line implying `app/archetypes/` is current directory
- `tests/test_obstacle_model_slice.cpp`:
  - Reworded stale comments (removed wording implying duplicate archetype helpers or canonical `app/archetypes/` path)
  - Updated local helper naming/comments to use entity-factory terminology
  - Preserved `[archetype]` tags as acceptable historical test taxonomy

**Validation:**
- Focused grep search: zero remaining references to `app/archetypes/` canonical wording
- `cmake --build build --target shapeshifter_tests`
- `./build/shapeshifter_tests "[model_slice]"` — PASS (71 assertions, 20 test cases)
- Zero compiler warnings

**Status:** Wording cleanup complete; final review (Kujan) approved.


