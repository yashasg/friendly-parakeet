# McManus — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Gameplay Engineer
- **Joined:** 2026-04-26T02:07:46.544Z

## 2026-04-27 — Issue #315 Closure (EnTT-safe scoring_system iteration)

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

---

### 2026-04-28 — Lifetime Component/System Removal Plan

**Task:** Read-only analysis pass — plan removal of `Lifetime` component and `lifetime_system`. Keaton is actively editing adjacent files (`game_loop.cpp`, `all_systems.h`, tests, benchmarks), so no code changes allowed yet.

**Key findings:**
- `Lifetime` is NOT attached to obstacle entities anywhere in game code. `obstacle_despawn_system` already owns correct positional despawn (camera-Z threshold). Status quo is correct for obstacles.
- Three active uses of `Lifetime` remain: popup alpha fade (`popup_display_system`), particle size ratio (`camera_system`), and entity destruction (`lifetime_system` for both).
- Replacement: add `remaining`/`max_time` to `ScorePopup` (popup timer) and `ParticleData` (particle timer). No new component needed — fields belong in the domain-specific structs per user directive.
- Timer countdown + particle destruction moves to `particle_system`. Popup timer countdown + destruction moves to `popup_display_system` (collect-then-destroy, static buffer).
- `benchmarks/bench_systems.cpp` references `cleanup_system` (stale name — renamed to `obstacle_despawn_system`). This is Keaton's active work; must not touch until Keaton's patch lands.

**Files to change after Keaton lands:** `lifetime.h` (delete), `lifetime_system.cpp` (delete), `scoring.h`, `particle.h`, `scoring_system.cpp`, `popup_display_system.cpp`, `camera_system.cpp`, `particle_system.cpp`, `all_systems.h`, `game_loop.cpp`, three test files, benchmarks.

**Plan filed:** `.squad/decisions/inbox/mcmanus-lifetime-removal-plan.md`

**Learning: Always audit ALL uses of a component before planning removal.** The user framed this as an obstacle issue, but obstacles never used `Lifetime` — the real usage was entirely in popups and particles. Read the code before accepting the framing.

---

### 2026-04-28 — Team Session Closure: ECS Cleanup Approval

**Status:** APPROVED ✅ — Deliverable logged; ready for merge.

Scribe documentation:
- Orchestration log written: .squad/orchestration-log/2026-04-28T08-12-03Z-mcmanus.md
- Team decision inbox merged into .squad/decisions.md
- Session log: .squad/log/2026-04-28T08-12-03Z-ecs-cleanup-approval.md

Next: Await merge approval.

### 2026-04-28 — Popup Entity Factory (#349 slice)

**Status:** COMPLETE

**What changed:**
- `app/entities/popup_entity.h` — Added `PopupSpawnParams` struct and `spawn_score_popup()` declaration (entt + rhythm.h included).
- `app/entities/popup_entity.cpp` — Implemented `spawn_score_popup()`: emplace WorldTransform at {x, y-40}, MotionVelocity {0, -80}, ScorePopup, Color (by timing tier), DrawLayer::Effects, TagHUDPass, PopupDisplay via init_popup_display.
- `app/systems/scoring_system.cpp` — Replaced 28-line inline popup bundle (lines 184-211) with 3-line `spawn_score_popup()` call; audio push stays in scoring_system.
- `tests/test_popup_display_system.cpp` — Added 8 factory contract tests under `[popup_entity][issue349]` tag.
- `tests/test_scoring_system.cpp` — Added full-contract popup test + LanePush no-popup test.

**Build:** Zero warnings, zero errors (clang -Wall -Wextra -Werror).
**Tests:** 52 test cases, 132 assertions — all passed.

**Key decision:** `spawn_score_popup` does not push audio (kept in scoring_system per task spec).

---

## Session: 2026-04-28T22:35:09Z — Popup Entity Factory Boundary Established

**Context:** Scribe consolidated inbox decisions into decisions.md and updated cross-agent context.

**Your work:** Established popup entity factory boundary: `spawn_score_popup(entt::registry&, PopupSpawnParams)` in `app/entities/popup_entity.h/.cpp` is authoritative constructor owning full component bundle (WorldTransform, MotionVelocity, ScorePopup, Color, DrawLayer::Effects, TagHUDPass, PopupDisplay). Audio push remains caller responsibility (scoring_system pushes SFX::ScorePopup after call). Follows spawn_obstacle pattern.

**Status:** Decision captured and merged into team decisions.

**Related:** `mcmanus-popup-entity-factory.md` merged into `.squad/decisions.md`

### 2026-04-29 — Gameplay boundary cluster (#276/#278/#279/#282)

- Shifted **energy writes** behind a single-writer boundary: `scoring_system` now accumulates `PendingEnergyEffects` intent; `energy_system` is the only system that mutates `EnergyState` (`energy`, `flash_timer`).
- Shifted **popup spawn + popup SFX** behind a dedicated bridge: `scoring_system` now emits `ScorePopupRequestQueue` intents; new `popup_feedback_system` owns `spawn_score_popup` + `audio_push(SFX::ScorePopup)`.
- Centralized **miss death-cause attribution** in `scoring_system` MissTag processing (first-cause-wins), removing direct `GameOverState::cause` writes from `collision_system` and `miss_detection_system`.
- Moved **energy-zero → GameOver transition ownership** to `game_state_system` (state-machine owner). `enter_game_over` now owns setting `SongState.finished/playing` on death.

### 2026-04-29 — #281 setup_play_session ownership split (surgical)

- Extracted player spawn call from `setup_play_session()` into focused helper `spawn_session_player(entt::registry&)`.
- Isolated runtime Playing HUD button spawning behind `spawn_playing_shape_buttons(entt::registry&)` and phase mutation behind `enter_playing_phase(GameState&)`.
- Added regression coverage for end-screen restart path: GameOver restart now explicitly validated across two ticks (choice tick + transition tick) to ensure fresh play-session setup.
- Validation: `cmake --build build-ralph --target shapeshifter_tests`, `./build-ralph/shapeshifter_tests "[gamestate]"`, `./build-ralph/shapeshifter_tests "[play_session]"`, and full `./build-ralph/shapeshifter_tests` all pass.

### 2026-04-29 — #277 game_state boundary extraction (surgical)

- Extracted terminal-phase side effects out of `game_state_system.cpp` into `game_state_enter_terminal_phase(...)` (`app/systems/game_state_terminal_phase_system.cpp`).
  - High-score compare/update/persist now lives outside the core state-machine file.
  - Terminal feedback (Crash SFX, DeathCrash/NewHighScore haptics with `SettingsState` gating) moved with it.
- Isolated end-screen transition loop behind `game_state_end_screen_system(...)` (`app/systems/game_state_end_screen_system.cpp`) and called it from `game_state_system`.
- Isolated menu/end-screen input mapping behind focused routing boundary `game_state_handle_end_screen_press(...)` (`app/input/game_state_end_screen_routing.cpp`), invoked by `game_state_handle_press`.
- Preserved behavior and timing guards (`0.4s` end-screen input, `0.5s` SongComplete choice application).
- Validation: `cmake --build build-ralph --target shapeshifter_tests`; `./build-ralph/shapeshifter_tests "[gamestate]"`; `"[haptic]"`; `"[high_score]"`; `"[redfoot]"`; full `./build-ralph/shapeshifter_tests` (pass, exit 0).

### 2026-04-29 — First shape-switch stutter audit/fix

- First tap-to-shape path was doing cold work: `hit_test_handle_input()` called `ensure_active_tags_synced()`, which structurally emplaced `ActiveTag` on the three Playing shape buttons on the first tap after `setup_play_session()`.
- `UIActiveCache` can also remain `valid=true, phase=Playing` across `reg.clear()` on restart, so setup must invalidate the cache whenever it respawns Playing shape buttons.
- Fixed by invalidating and syncing `ActiveTag` immediately after `enter_playing_phase()` in `setup_play_session()`, moving that ECS structural churn out of the first input frame.
- Also moved other cold first-input work earlier: `wire_input_dispatcher()` now warms Input/Go/ButtonPress event queues, and iOS haptic generators are warmed at startup / when haptics are re-enabled so `ShapeShift` feedback does not allocate its generator on the first shape press.
- Validation: focused `[haptic]`, `[input_pipeline]`, `[entt_dispatcher]`, `[gamestate][play_session]`, `[player][rhythm]`; full `shapeshifter_tests`; `shapeshifter` target build; `git diff --check`.
