# McManus — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Gameplay Engineer
- **Joined:** 2026-04-26T02:07:46.544Z

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

## Learnings

- **EnTT collect-then-remove pattern:** Any `reg.remove<C>` where C is in the active view's component list is potential swap-and-pop UB. Always collect entities first, remove after. Static vectors avoid per-frame alloc.
- **Structural view split for branching:** When an `any_of<T>` branch is the primary discriminator inside a view loop, split into two structural views (`with T` / `entt::exclude<T>`). This is both safer and gives EnTT better cardinality info.
- **MissTag entities don't need Position:** Miss processing (energy drain, miss_count, chain reset) never reads position. The structural split lets the miss view drop Position from its component list entirely.
- **Build workaround (worktree):** The 315 worktree doesn't have vcpkg_installed. Used symlink + explicit `-D*_DIR` flags to point CMake at the main worktree's built packages.

### 2026-04-27 — Issue #315 Closure (EnTT-safe scoring_system iteration)

**Implementation complete; review approved; main branch integration validated.**

**Final outcome:**
- Commit 5f0ffb8: Scoring_system refactored with collect-then-remove pattern
- Two structural views (miss/hit) replace per-entity `any_of<MissTag>` branching
- Zero per-frame heap allocation (static vector with `.clear()`)
- All 2419 assertions / 768 test cases pass on main branch; zero compiler warnings
- Pattern documented and ready for reuse across other systems

## 2026 — EnTT ECS Audit (gameplay/rhythm/scoring/obstacle surface)

**Scope:** obstacle.h, song_state.h, rhythm.h, scoring_system, collision_system, beat_scheduler_system, obstacle_spawn_system, play_session, cleanup_system, miss_detection_system. Validated against docs/entt/Entity_Component_System.md and Core_Functionalities.md.

**Verdict:** Mostly clean. Two actionable findings; rest is solid.

**Key file paths:**
- `app/components/obstacle.h` — ObstacleTag, ScoredTag, MissTag: clean tag/data split
- `app/components/song_state.h` — SongState, EnergyState, GameOverState, SongResults: ctx singletons, mixing metadata + runtime state (soft smell)
- `app/components/rhythm.h` — TimingTier, TimingGrade, BeatInfo, pure helpers: clean
- `app/components/obstacle_counter.h` — wire_obstacle_counter() is registry logic in a component header (F4 residual, not moved to .cpp)
- `app/systems/scoring_system.cpp` — collect-then-remove (#315 complete), structural view split (miss/hit), ctx hoisting: CLEAN
- `app/systems/collision_system.cpp` — per-kind structural views + entt::exclude<ScoredTag>, emplace during iteration: SAFE per EnTT docs §"What is allowed and what is not"
- `app/systems/miss_detection_system.cpp` — emplace MissTag+ScoredTag during view iteration: SAFE
- `app/systems/cleanup_system.cpp` — static vector + two-pass destroy: CLEAN
- `app/systems/beat_scheduler_system.cpp` — all state in ctx, no global state: CLEAN
- `app/systems/obstacle_spawn_system.cpp` — RNGState in ctx, mt19937: CLEAN
- `app/systems/play_session.cpp` — session reset, ctx singletons via insert_or_assign: CLEAN
- `app/components/input_events.h` — EventQueue struct still present (Tier 1 migration incomplete per decisions.md step 4)

**Findings:**
1. (P1) wire_obstacle_counter() in obstacle_counter.h — F4 residual, registry logic in component header
2. (P2) EventQueue not removed — Tier 1 raw InputEvent routing still uses EventQueue alongside dispatcher (decisions.md migration step 4 incomplete)
3. (P3/soft) SongState and DifficultyConfig mix metadata with mutable runtime state — no correctness issue

**EnTT confirmation:** Emplacing components during view iteration (collision, miss_detection) is explicitly allowed in EnTT docs: "Creating entities and components is allowed during iterations in most cases." The exclude<ScoredTag> pattern in collision_system is safe because the ScoredTag pool's modifications don't affect the leading iteration pool.

**Burnout surface:** Fully removed; no residual BankedBurnout, BurnoutState, or burnout-related ECS components in scope files.

### 2026-04-28 — #330 ObstacleCounter signal wiring refactor

**Branch:** `squad/330-obstacle-counter-system`
**Commit:** `a7c99bf`

**What:** Moved `wire_obstacle_counter()` and signal listener lambdas out of the component header (`obstacle_counter.h`) into a new system translation unit (`app/systems/obstacle_counter_system.cpp`). `ObstacleCounter` is now a plain data struct (`count`, `wired`) with no logic.

**Key decisions:**
- Signal listeners made `static` (TU-local) — callers only need the `wire_obstacle_counter` declaration from `all_systems.h`.
- `play_session.cpp` gained `#include "all_systems.h"` so it resolves `wire_obstacle_counter` through the system header, not the component header.
- No CMake changes needed: `file(GLOB SYSTEM_SOURCES app/systems/*.cpp)` picks up the new file automatically.
- `test_helpers.h` already included `all_systems.h` — zero test-helper changes required.

**Tests:** All 2714 assertions passed.

**Pattern to follow:** Any component header that contains signal wiring (`on_construct`/`on_destroy` + `wire_*`) violates the data-only component rule. Move to a `*_system.cpp` and declare in `all_systems.h`.

### 2026-04-28 — #340 SongState/DifficultyConfig ownership comments

**Issue:** #340 — [P3] Document init-time vs per-frame ownership in SongState and DifficultyConfig

**Work done:**
- Audited all systems touching `SongState` and `DifficultyConfig`:
  - `song_playback_system`: advances `song_time`, `current_beat`; clears `playing`/`finished`; consumes `restart_music`
  - `energy_system`: can also set `finished`/clear `playing` on death
  - `beat_scheduler_system`: advances `next_spawn_idx`
  - `setup_play_session` / `init_song_state`: session-init for all fields
  - `song_state_compute_derived`: calculates derived fields once at init
  - `difficulty_system`: ramps `speed_multiplier`, `scroll_speed`, `spawn_interval`, `elapsed` per-frame (skips in rhythm mode)
  - `obstacle_spawn_system`: decrements `spawn_timer`
- Added section comments (`Session-init`, `Derived`, `Per-frame`, `Beat-schedule cursor`) to `SongState`
- Added struct-level block comment and inline comments to `DifficultyConfig`
- No behaviour changes

**Build result:** `cmake --build build --target shapeshifter_lib` — `[100%] Built target shapeshifter_lib` (zero warnings/errors). Pre-existing unrelated errors in `play_session.cpp` prevent full `shapeshifter` binary link.

**Commit:** `1c1bf08`

**Closure-ready:** Yes — acceptance criteria fully met.

### 2026 — Model Authority Revision (BF-1..BF-4)

**Task:** Fix four Kujan blockers in the Model authority slice (original author Keaton, locked out).

**BF-1 Fixed:** Replaced `LoadModelFromMesh` in `app/gameobjects/shape_obstacle.cpp` with manual raylib `Model` construction: `RL_MALLOC` for `meshes`, `materials`, `meshMaterial`; `GenMeshCube` directly into `model.meshes[0]`; `LoadMaterialDefault()` for `model.materials[0]`; `RL_CALLOC` zeroes `meshMaterial`. No `UploadMesh` call (raylib 5.5 GenMesh* already returns uploaded meshes).

**BF-2 Fixed:** `camera_system.cpp` section 1b now queries `ObstacleModel + ObstacleParts + ObstacleScrollZ` and writes `om.model.transform = slab_matrix(pd.cx, oz.z + pd.cz, ...)` directly. No longer emits `ModelTransform` for LowBar/HighBar. `game_render_system.cpp` added `draw_owned_models()` that iterates `ObstacleModel + TagWorldPass` entities and calls `DrawMesh` per-mesh. Added `render_tags.h` include to render system.

**BF-3 Fixed:** Deleted `app/systems/obstacle_archetypes.cpp` and `app/systems/obstacle_archetypes.h` (duplicate of `app/archetypes/obstacle_archetypes.*`). Updated stale includes in `beat_scheduler_system.cpp`, `obstacle_spawn_system.cpp`, and `tests/test_obstacle_archetypes.cpp` to canonical `archetypes/obstacle_archetypes.h` path.

**BF-4 Fixed:** `ObstacleParts` replaced empty tag with POD geometry fields: `cx, cy, cz` (origin offsets), `width, height, depth` (world-space dimensions). `build_obstacle_model()` now populates all six fields at spawn.

**Validation:** cmake configure clean, `shapeshifter_tests` 2975/2975 pass, `shapeshifter` binary builds clean, `git diff --check` clean, no `LoadModelFromMesh(` calls, no stale `systems/obstacle_archetypes` includes.

### 2026-04-28 — #entity-layer Obstacle Entity Layer

**Task:** Introduce `app/entities/` as an entity construction surface for obstacles. Consolidate the component bundle contract that was split across archetypes and spawn sites.

**What was done:**
- Created `app/entities/obstacle_entity.h` — defines `ObstacleSpawnParams` (kind, x, y, shape, mask, req_lane, speed) and `spawn_obstacle(reg, params, beat_info*)`.
- Created `app/entities/obstacle_entity.cpp` — implements `spawn_obstacle`: emplaces ObstacleTag, Velocity, DrawLayer, optional BeatInfo, all kind-specific components, and calls `spawn_obstacle_meshes`. Single owner of the full component bundle contract.
- Deleted `app/archetypes/obstacle_archetypes.h` and `app/archetypes/obstacle_archetypes.cpp` — fully superseded.
- Updated `obstacle_spawn_system.cpp` and `beat_scheduler_system.cpp` to include `entities/obstacle_entity.h` and call `spawn_obstacle`. Both systems no longer manually emplace base components.
- Rewrote `tests/test_obstacle_archetypes.cpp` to test `spawn_obstacle` directly. Added BeatInfo-present/absent tests.
- Updated `tests/test_obstacle_model_slice.cpp` to use `spawn_obstacle` via helper.

**Key design decisions:**
- `beat_info` passed as `const BeatInfo*` (nullable pointer) rather than `std::optional<BeatInfo>` field in the struct, to avoid `-Wmissing-field-initializers` on positional brace-init callsites. Clean API, zero warning overhead.
- `spawn_obstacle_meshes` is called inside `spawn_obstacle` so callers get the full entity — logical + visual — in one call.
- `ObstacleSpawnParams` is a plain aggregate with all-defaulted trailing fields, same pattern as the old `ObstacleArchetypeInput`.

**Build/Tests:** Zero warnings. 2983 assertions in 904 test cases — all pass.
