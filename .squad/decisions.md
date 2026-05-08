### 2026-05-08T11:47:09.246-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Floor rings should be drawn as 2D on the floor quad/texture in XY space, then the floor texture/quad should be rotated into the XZ floor plane; do not assume the ring must remain hand-emitted world-space geometry.
**Why:** User correction — captured for team memory
### 2026-05-08T11:47:09.246-07:00: Keaton re-evaluation — floor ring texture architecture

**Context**
- User clarified target architecture: draw floor ring in 2D (XY) on a floor texture/quad, then orient that textured quad onto the world floor (XZ).

**Decision Input**
- Under this architecture, `app/util/shape_vertices.h` can be deleted entirely after rewiring runtime ring rendering away from `shape_verts::CIRCLE` and updating tests/benchmarks that currently include it.
- Preferred raylib path to evaluate: `LoadRenderTexture` + `BeginTextureMode` + `ClearBackground` + `DrawRing`/`DrawCircleLines` (2D pass), then sample that texture on a world-floor mesh (`DrawMesh`/material diffuse map or `DrawPlane` with texture-capable shader/material path).

**Key Constraints**
- Current mesh shader (`app/systems/camera_system.cpp`) does not sample textures; it shades `colDiffuse` only, so a texture floor needs shader/material changes or a separate floor draw path.
- Current `RenderTargets` RAII (`world`, `ui`) will likely need an additional owned floor texture/render target resource and lifecycle coverage.
- RenderTexture Y-flip and UV mapping must be validated to avoid upside-down/mirrored floor rings.

**Likely rewiring scope**
- `app/systems/game_render_system.cpp` (replace immediate-mode annulus with textured-floor draw path)
- `app/systems/camera_system.cpp` + `app/rendering/camera_resources.h` (resource creation/ownership for floor texture and possibly floor material/shader)
- `tests/test_perspective.cpp`, `benchmarks/bench_perspective.cpp` (remove `shape_verts::*` dependencies)
- Optional: `tests/test_gpu_resource_lifecycle.cpp` (new RAII type-trait/idempotent-release checks for floor texture resource)

### 2026-05-08T11:53:19.588-07:00: **COMPLETED** shape_vertices.h removal

**Decision:** Floor ring geometry now generates circle points locally in `app/systems/game_render_system.cpp` using trig per segment. `app/util/shape_vertices.h` is deleted.

**Rationale:** Floor rings are a floor-rendering concern (2D ring logic on XZ plane), not shared reusable shape data. Removes stale custom vertex-table maintenance.

**Implementation:** 
- Rewired `draw_floor_rings()` off `shape_verts::CIRCLE` to local trig-based generation.
- Deleted `app/util/shape_vertices.h` and all references.
- Updated `tests/test_perspective.cpp` and `benchmarks/bench_perspective.cpp` to remove shape_vertices dependency.
- Verified: `./build.sh` + `./build/shapeshifter_tests` all pass (APPROVED by Kujan).

**Scope owned by:** Keaton (implementation) + Kujan (review, APPROVED).

### 2026-05-08T11:59:39.840-07:00: Floor render system split

**By:** Keaton  
**Requested by:** yashasg

**Decision**
- Move floor-only rendering helpers and draw orchestration from `app/systems/game_render_system.cpp` into `app/systems/floor_render_system.cpp`.
- Keep `game_render_system.cpp` responsible for pass flow (camera setup, floor pass call, world/effects meshes) while floor geometry/details live in the dedicated system.

**Why**
- Reduces file complexity and keeps floor rendering concerns isolated.
- Preserves ECS/raylib flow and behavior with a narrow interface: `floor_render_system(const entt::registry&)`.

**Implementation Notes**
- Added `app/systems/floor_render_system.cpp` and `app/systems/floor_render_system.h`.
- `game_render_system.cpp` now calls `floor_render_system(reg)` and no longer contains floor helper code.
- Added declaration in `app/systems/all_systems.h`.

### 2026-05-08T11:59:39.840-07:00: Kujan review — floor render split (approved)

**By:** Kujan  
**Requested by:** yashasg

**Decision**
- Approve Keaton's floor render split.

**Review outcome**
- `game_render_system.cpp` is materially cleaner and remains coherent as the world-pass orchestrator.
- Floor-specific drawing logic is isolated in `app/systems/floor_render_system.cpp` with a narrow ECS-facing entrypoint declared in headers.
- Build wiring is intact via existing systems glob; no CMake drift detected.
- No stale `shape_vertices` references remain in `app/`, `tests/`, or `benchmarks/`.
- Validation reproduced successfully: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh` and `./build/shapeshifter_tests`.

**Non-blocking note**
- Unrelated `.squad` churn (health reports/history updates) is present in working tree; keep product commit scope focused.

### 2026-05-08T13:03:11.140-07:00: Keaton cleanup — file_logger removal + session_logger scope decision

**By:** Keaton  
**Requested by:** yashasg

**Decision**
- Deleted dead `file_logger` module (`app/util/file_logger.h/.cpp`) and CMake entries.
- Deleted legacy forwarding header `app/components/camera.h`.
- Deleted obsolete `benchmarks/bench_file_logger.cpp`.
- Deferred raylib callback migration for `session_logger` to future work.

**Rationale for deferral**
- raylib `SetTraceLogCallback` is global process state; would capture unrelated TraceLog traffic.
- `session_logger` is scoped, structured, and ECS-driven (test-player telemetry only).
- Safe migration requires custom filtering/prefixing and callback lifecycle control (revisit later if needed).

**Implementation validated**
- Removed CMake source entries; stale references eliminated from `tests/test_camera_entity_contracts.cpp`.
- No build regressions or warnings.

### 2026-05-08T13:03:11.140-07:00: Kujan review — file_logger cleanup + session_logger deferral (APPROVED)

**By:** Kujan  
**Requested by:** yashasg
**Revision owner:** Keaton

**Findings**
- High-confidence removals were complete and safe. No stale references in app/tests/bench/CMake.
- CMake wiring correct after cleanup.
- Benchmark deletion appropriate; coverage maintained by other benchmark files.
- TraceLog callback deferral justified: callback is global state, current code emits many unrelated logs.
- No warnings or regression hazards.
- Validation: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh` and `./build/shapeshifter_tests` (2063 assertions, 758 test cases).

**Verdict:** APPROVE

### 2026-05-08T13:08:46.440-07:00: Keaton implementation audit — raylib replacement candidates

**By:** Keaton  
**Requested by:** yashasg  
**Scope:** Read-only audit of `app/` for homegrown implementations replaceable with direct raylib APIs.

**High-confidence replacements (replace-with-raylib)**
1. HUD hexagon path: replace manual trig + 6 `DrawTriangle` with `DrawPoly({cx, cy}, 6, radius, -90.0f, color)`.
2. Beatmap file loading: replace `std::ifstream` with `LoadFileText()` / `UnloadFileText()`.

**Medium-confidence needs design decision**
1. Floor geometry (`draw_floor_lines`/`draw_floor_rings`): immediate-mode `rlBegin`/`rlVertex3f` → possible `DrawLine3D`/`DrawTriangle3D`/textured-floor.
2. Input/display: `screen_to_virtual()` letterbox mapping and `Camera2D` alternatives.
3. Persistence: `settings_persistence.cpp` + `high_score_persistence.cpp` file I/O.

**Keep (do not replace)**
- Obstacle model assembly, session logger, platform display glue, procedural SFX, font loading, song playback logic, collision/timing math, haptics bridge.

**Proposed cleanup order**
1. Replace HUD hexagon with `DrawPoly`.
2. Replace beatmap file text ingestion with `LoadFileText`.
3. Decide floor rendering target architecture before touching rlgl floor primitives.
### 2026-05-08T13:15:08.642-07:00: Raylib cleanup stale-reference validation pattern

**By:** Baer  
**Requested by:** yashasg

**Pattern**
- When removing raylib-replacement leftovers (helpers/includes/files), run a scoped stale-reference grep against only `app/`, `tests/`, `benchmarks/`, and `CMakeLists.txt` for deleted symbols/paths, then run full build + test validation.

**Why it matters**
- Most regressions in cleanup passes are orphaned references in build wiring or test/benchmark includes, not runtime behavior changes.
- This catches dead-surface linkage failures quickly without widening noise to docs or archive files.

### 2026-05-08T13:15:08.642-07:00: Safe raylib API replacements for HUD/file I/O/floor lines

**By:** Keaton  
**Requested by:** yashasg

**Decision**
- Replace HUD hexagon fan triangulation with `DrawPoly(..., 6, radius, -90.0f, ...)`.
- Replace beatmap/constants full-file ingestion from `std::ifstream` iterators with `LoadFileText`/`UnloadFileText`.
- Replace floor lane/grid/beat `RL_LINES` emission with `DrawLine3D`, but keep floor ring annulus geometry on existing `RL_TRIANGLES` path (design-gated).

**Why**
- These substitutions reduce custom rendering/file-loading code while preserving behavior and diagnostics.
- They stay inside the approved cleanup surface and avoid touching design-gated rendering architecture.

### 2026-05-08T13:15:08.642-07:00: Kujan review: safe raylib API replacements (Keaton)

**Requested by:** yashasg  
**Revision owner:** Keaton  
**Verdict:** APPROVE

**Findings**
- Scope matches the approved replacement set and does not cross into design-gated floor annulus architecture changes.
- HUD hexagon fill replacement is correct and warning-safe: `DrawPoly(..., 6, radius, -90.0f, ...)`.
- Beatmap/constants loading correctly uses `LoadFileText`/`UnloadFileText`; constants parse failures now emit useful `TraceLog(LOG_WARNING, ...)` details without suppressing fallback behavior.
- Floor lane/grid/beat-line emission uses `DrawLine3D`; annulus ring geometry remains on the existing `RL_TRIANGLES` path (untouched by this review scope).
- No stale app/tests/bench/CMake references were found for removed helper surfaces relevant to this change set.

**Validation**
- Reproduced: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh`
- Reproduced: `./build/shapeshifter_tests`
- Result: all tests passed (2063 assertions in 758 test cases).
### 2026-05-08T13:38:14.844-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Move-into-system cleanup candidates should leave no useless `app/util/` files behind after references move, and directory creation/existence helpers should use raylib APIs such as `MakeDirectory`/`DirectoryExists` instead of `std::filesystem` wrappers.
**Why:** User request — captured for team memory
### 2026-05-08T13:32:04.383-07:00: Fenster app/util cleanup audit (read-only)

Requested by: yashasg

#### Team-relevant decisions from audit

1. **Move-only candidates (low risk, no raylib dependency):**
   - `app/util/test_player_helpers.h` → inline static helpers inside `app/systems/test_player_system.cpp` (or a local `test_player_system_helpers.h` under systems).
   - `app/util/enum_names.h` → move into logging/test-player surface (`app/systems/test_player_system.cpp` + `app/util/session_logger.cpp` local helper scope).
   - `app/util/safe_localtime.h` → move into `session_logger.cpp` (single logging concern) and call site in `test_player_session.cpp`.
   - `app/util/obstacle_counter.*` → move to systems/session ownership (`app/systems/` or `app/session/`) because it is wired and consumed only in phase/session transitions.

2. **Raylib-replace candidates (staged):**
   - `app/util/settings_persistence.cpp` and `app/util/high_score_persistence.cpp` can replace stream I/O with `LoadFileText`/`UnloadFileText` and `SaveFileText`.
   - Keep `persistence::Status` mapping behavior intact; current tests assert exact statuses for missing/corrupt/unwritable-path paths.

3. **Keep/design-gated surfaces:**
   - `app/util/beat_map_loader.*` stays (already using `LoadFileText`; parser/validator is shared runtime+tests).
   - `app/util/rhythm_math.h`, `app/util/session_logger.*`, `app/util/settings.h`, `app/util/persistence_policy.*`, `app/util/fs_utils.h` are not straight deletions without cross-system refactors or policy changes.

4. **Reference impact (must-update if moving/deleting):**
   - test/player logging helpers: `tests/test_test_player_system.cpp`, `tests/test_beat_log_system.cpp`, `app/systems/beat_log_system.cpp`, `app/session/test_player_session.cpp`, `app/game_loop.cpp`.
   - obstacle counter: `app/session/play_session.cpp`, `app/systems/game_state_system.cpp`, `tests/test_signal_lifecycle_nogated.cpp`, `tests/test_helpers.h`.
   - persistence helpers: `app/game_loop.cpp`, `tests/test_settings_persistence.cpp`, `tests/test_high_score_persistence.cpp`, `tests/test_high_score_integration.cpp`.

5. **Build/bench note:**
   - No benchmark references currently depend on these util headers.
   - `CMakeLists.txt` uses `file(GLOB UTIL_SOURCES app/util/*.cpp)`, so deleting/moving util `.cpp` files does not require manual source-list edits, but include-path call sites must be updated.
# Keaton app/util cleanup audit (read-only)

**When:** 2026-05-08T13:32:04.383-07:00  
**Requested by:** yashasg  
**Scope checked:** `app/`, `tests/`, `benchmarks/`, `CMakeLists.txt`  
**Implementation status:** No code changes made

## 1) Full `app/util/` inventory + classification

| File | Classification | Why |
|---|---|---|
| `app/util/beat_map_loader.h` | keep/domain utility | Core beatmap parsing/validation API used by runtime + many tests. |
| `app/util/beat_map_loader.cpp` | keep/domain utility | Owns parser/validator behavior and `LoadFileText` ingestion already. |
| `app/util/enum_names.h` | delete-now candidate | Thin wrappers over `magic_enum::enum_name`; can be inlined at call sites. |
| `app/util/fs_utils.h` | delete-now candidate | Single-use wrapper around `std::filesystem::create_directories`. |
| `app/util/high_score_persistence.h` | keep/domain utility | Persistence API used by session startup/terminal save. |
| `app/util/high_score_persistence.cpp` | keep/domain utility | Structured error/status handling and JSON clamping logic. |
| `app/util/obstacle_counter.h` | delete-now candidate | Signal-based counter can be replaced by direct EnTT obstacle presence query. |
| `app/util/obstacle_counter.cpp` | delete-now candidate | Wiring/listeners become unnecessary with direct query approach. |
| `app/util/persistence_policy.h` | keep/domain utility | Shared path/status contract used by settings + high-score flows. |
| `app/util/persistence_policy.cpp` | keep/domain utility | Platform path resolution used at startup and in persistence layers. |
| `app/util/rhythm_math.h` | keep/domain utility | Shared timing math consumed by collision/scoring/session init and tests. |
| `app/util/safe_localtime.h` | move-into-system | Extremely narrow helper only used by session logging flow. |
| `app/util/session_logger.h` | design-gated | Could migrate to raylib global trace callback, but callback is global state. |
| `app/util/session_logger.cpp` | design-gated | Current scoped session telemetry is safer than process-global callback. |
| `app/util/settings.h` | keep/domain utility | Shared settings singleton definition used across systems/UI/tests. |
| `app/util/settings_persistence.h` | keep/domain utility | Persistence API for settings and dirty-save contract. |
| `app/util/settings_persistence.cpp` | keep/domain utility | JSON validation/clamping + structured persistence failures. |
| `app/util/test_player_helpers.h` | move-into-system | Purely `test_player_system` action-queue helpers; narrow ownership. |

---

## 2) Delete/replace/move candidates (with refs, targets, steps, risks, confidence)

## A) `app/util/obstacle_counter.h/.cpp` — **delete-now candidate**

**Exact current references**
- Runtime:
  - `app/session/play_session.cpp:3,48-53`
  - `app/systems/game_state_system.cpp:5,108-110`
- Tests:
  - `tests/test_helpers.h:7,50-51`
  - `tests/test_components.cpp:113`
  - `tests/test_signal_lifecycle_nogated.cpp:19,23-137` (dedicated signal lifecycle coverage)

**Target API/system**
- Replace with direct EnTT query in owning system:
  - `reg.view<ObstacleTag>().begin() == reg.view<ObstacleTag>().end()` (or storage emptiness check) inside `game_state_system`.
- Keep ownership in `game_state_system`/`play_session` (remove signal wiring from session setup).

**Migration steps**
1. In `game_state_system.cpp`, replace `ObstacleCounter` check with direct obstacle presence query.
2. In `play_session.cpp`, remove context emplace/reset + `wire_obstacle_counter`.
3. Delete `app/util/obstacle_counter.h/.cpp`.
4. Update tests that assert signal-wiring semantics to assert runtime semantics (SongComplete only after obstacles drained).
5. Run stale-reference sweep in `app/ tests/ benchmarks/ CMakeLists.txt`.

**Risks**
- Medium: removes current signal-lifecycle contract tests; must preserve terminal-phase behavior coverage.
- Low perf risk: direct query is cheap at this scale; prior EnTT audit already flagged this as safe near-term win.

**Confidence:** **High**

---

## B) `app/util/fs_utils.h` — **delete-now candidate**

**Exact current references**
- `app/util/persistence_policy.cpp:54`
- `app/util/settings_persistence.cpp:129`
- `app/util/high_score_persistence.cpp:199`

**Target API/system**
- Replace with direct `std::filesystem::create_directories(..., std::error_code&)` in each persistence owner (`persistence_policy.cpp`, `settings_persistence.cpp`, `high_score_persistence.cpp`).

**Migration steps**
1. Inline create-directories/error handling at each call site.
2. Remove `#include "fs_utils.h"` from those files.
3. Delete `app/util/fs_utils.h`.
4. Verify status mapping (`DirectoryCreateFailed`) remains unchanged.

**Risks**
- Low: wrapper is purely pass-through and call sites already consume `error_code`.

**Confidence:** **High**

---

## C) `app/util/enum_names.h` — **delete-now candidate**

**Exact current references**
- Runtime:
  - `app/systems/test_player_system.cpp:11,280-322`
  - `app/util/session_logger.cpp:7,92-131`
- Tests:
  - `tests/test_helpers_and_functions.cpp:4,169-188`
  - `tests/test_obstacle_model_slice.cpp:64,121`

**Target API/system**
- Replace wrappers with direct `magic_enum::enum_name(...)` at each owner callsite.
- Keep name-format behavior in logging/tests by using `std::string_view` conversion where needed.

**Migration steps**
1. Remove `ToString` helper calls in test player + session logger + tests.
2. Include `magic_enum/magic_enum.hpp` directly where required.
3. Delete `app/util/enum_names.h`.

**Risks**
- Low: behavior should be equivalent; must ensure fallback handling for empty names remains intentional.
- Medium in tests: string conversion (`std::string_view` vs C-string) needs explicit normalization.

**Confidence:** **Medium-High**

---

## D) `app/util/safe_localtime.h` — **move-into-system**

**Exact current references**
- `app/session/test_player_session.cpp:6,51`
- `app/util/session_logger.cpp:13,23`

**Target API/system**
- Move localtime portability helper into owning logging/session implementation files (anonymous namespace static helper), not shared `util/`.

**Migration steps**
1. Copy helper into `session_logger.cpp` (and optionally `test_player_session.cpp`, or share via session module-local header).
2. Remove include usage from both files.
3. Delete `app/util/safe_localtime.h`.

**Risks**
- Low: tiny helper; risk is only duplicated platform `#ifdef` drift.

**Confidence:** **High**

---

## E) `app/util/test_player_helpers.h` — **move-into-system**

**Exact current references**
- Runtime:
  - `app/systems/test_player_system.cpp:13` + many calls (`test_player_*`)
- Tests:
  - `tests/test_test_player_system.cpp:5,209-210,268-305` (direct helper usage)

**Target API/system**
- Move action/planned-list helper functions into `app/systems/test_player_system.cpp` (owner).
- For tests, either:
  - switch to black-box system-behavior assertions, or
  - define minimal test-local helper shims inside `test_test_player_system.cpp`.

**Migration steps**
1. Move helper implementations into anonymous namespace in `test_player_system.cpp`.
2. Remove util header include from runtime.
3. Refactor tests away from util header direct coupling.
4. Delete `app/util/test_player_helpers.h`.

**Risks**
- Medium: current tests directly call these helpers; test refactor required.

**Confidence:** **Medium**

---

## 3) Files that should **NOT** be removed (and why)

- `beat_map_loader.h/.cpp`: central parser/validator contract used by runtime and broad shipped-beatmap test surface.
- `rhythm_math.h`: shared timing constants/functions used by collision/scoring/session init + many unit tests.
- `persistence_policy.h/.cpp`: shared platform path + status contract; removing would duplicate platform logic.
- `settings.h`, `settings_persistence.h/.cpp`: settings state and persistence API are used across game loop, UI controller, input/systems, and tests.
- `high_score_persistence.h/.cpp`: high-score lifecycle is wired into startup + terminal-phase save and has dedicated tests.
- `session_logger.h/.cpp`: currently scoped telemetry with explicit lifecycle; replacing with global raylib callback remains design-gated.

---

## 4) Build/reference surface check (`app`, `tests`, `benchmarks`, `CMakeLists.txt`)

- `CMakeLists.txt` compiles util via glob: `file(GLOB UTIL_SOURCES ... app/util/*.cpp)` at `CMakeLists.txt:97`.
- No benchmark references to util targets/symbols found in `benchmarks/`.
- Test surface impact:
  - `obstacle_counter` removal would require updates in `test_helpers.h`, `test_components.cpp`, and `test_signal_lifecycle_nogated.cpp`.
  - `test_player_helpers.h` removal would require updates in `tests/test_test_player_system.cpp`.
  - `enum_names.h` removal would require updates in `tests/test_helpers_and_functions.cpp` and `tests/test_obstacle_model_slice.cpp`.

No implementation performed in this pass.
### 2026-05-08T13:23:49.542-07:00: EnTT functionality audit (app/ read-only)

**By:** Keaton  
**Requested by:** yashasg

**Scope**
- Read-only audit of `app/` for homegrown structures/plumbing that can be replaced or simplified with EnTT v3.16 facilities.
- Parallel review input gathered from C++ Expert and DoD Architect passes.

**High-confidence safe-next changes**
1. Replace `ObstacleCounter` signal-based count (`app/util/obstacle_counter.*`, read in `app/systems/game_state_system.cpp`) with direct EnTT storage/view emptiness checks (`reg.view<ObstacleTag>().empty()`).
2. Replace test-player planned bookkeeping arrays (`TestPlayerState::planned/planned_count` in `app/components/test_player.h`, helper usage in `app/util/test_player_helpers.h`, `app/systems/test_player_system.cpp`) with an existential component tag such as `TestPlayerPlannedTag`.
3. Tighten popup fade loop from `view<ScorePopup>() + try_get` to a structural view over guaranteed components (`ScorePopup`, `PopupDisplay`, `Color`) in `app/systems/popup_display_system.cpp`.
4. Mechanical cleanup: use context `emplace` idempotently for wiring/scratch helpers and range-based `reg.destroy(begin,end)` where validity invariants are explicit.

**Design-gated items**
1. Migrate `PendingEnergyEffects` and `ScorePopupRequestQueue` (current context vectors in `app/components/gameplay_intents.h` and `app/systems/scoring_system.cpp`) to `entt::dispatcher` events.
2. Consider migrating audio/haptic bounded queues (`AudioQueue`, `HapticQueue`) to dispatcher-backed events only if bounded-drop policy and frame delivery ordering are explicitly preserved.
3. Convert lane mask fields (`BlockedLanes::mask`, `BeatEntry::blocked_mask`) from raw `uint8_t` shifts to a typed EnTT bitmask enum pattern (`_entt_enum_as_bitmask`) after loader and validation boundaries are planned.

**Keep (no EnTT replacement recommended)**
- Per-kind structural collision views in `collision_system` (data access is explicit and aligned with obstacle archetypes).
- Mesh/model lifetime ownership code in `obstacle_render_entity` (raylib GPU resource semantics dominate).
- Collect-then-remove safety pattern in scoring/despawn-like systems.
# EnTT Architecture Audit — app/ (Read-only)

**Date:** 2026-05-08T13:23:49.542-07:00  
**By:** Keyser  
**Requested by:** yashasg  
**Scope:** Identify where EnTT data structures/functionality should replace homegrown ECS-adjacent code in `app/`, and where EnTT should explicitly not be used.

## High-confidence cleanup candidates

### 1) Replace signal-maintained obstacle singleton counter with native EnTT storage size
- **File/path:**  
  - `app/util/obstacle_counter.h`  
  - `app/util/obstacle_counter.cpp`  
  - `app/session/play_session.cpp`  
  - `app/systems/game_state_system.cpp`  
- **Current pattern:** `ObstacleCounter` (`ctx`) + `on_construct<ObstacleTag>`/`on_destroy<ObstacleTag>` wiring and reset logic to detect “all obstacles drained.”
- **EnTT replacement/API:** `reg.storage<ObstacleTag>().empty()` (or `.size()` if count is needed).
- **Touched dependencies:** session setup/reset, game-state transition logic, signal wiring lifecycle.
- **Migration risks:** low; main risk is stale reads during transition if check placement changes.
- **Proposed order:** **#1** (lowest-risk, highest simplification).

### 2) Move singleton cameras from singleton entities to `registry.ctx()`
- **File/path:**  
  - `app/entities/camera_entity.h`  
  - `app/entities/camera_entity.cpp`  
  - `app/systems/game_render_system.cpp`  
  - `app/systems/ui_render_system.cpp`  
  - `app/systems/obstacle_despawn_system.cpp`
- **Current pattern:** `GameCamera`/`UICamera` are stored as entities; accessors enforce single-instance by scanning views.
- **EnTT replacement/API:** context singletons (`reg.ctx().emplace<GameCamera>()`, `reg.ctx().emplace<UICamera>()`) with `get/find`.
- **Touched dependencies:** render paths, despawn threshold lookup, camera init flow, tests expecting camera entities.
- **Migration risks:** medium-low; remove implicit “entity lifetime” semantics and update every accessor site consistently.
- **Proposed order:** **#2** after obstacle-counter cleanup.

### 3) Eager-init ctx scratch singletons and stop repeated lazy `find/emplace` in hot systems
- **File/path:**  
  - `app/systems/scoring_system.cpp` (`ScoringSystemScratch`, `PendingEnergyEffects`, `ScorePopupRequestQueue`)  
  - `app/systems/obstacle_despawn_system.cpp` (`ObstacleDespawnScratch`)  
  - `app/systems/particle_system.cpp` (`ParticleSystemScratch`)  
  - `app/systems/popup_display_system.cpp` (`PopupDisplayScratch`)
- **Current pattern:** per-frame helper functions do `ctx().find<T>()` then `ctx().emplace<T>()`.
- **EnTT replacement/API:** eager-emplace in init/session setup; use `ctx().get<T>()` in frame loops.
- **Touched dependencies:** game loop/session init contract, tests with minimal registries.
- **Migration risks:** low; initialization contract needs one authoritative owner.
- **Proposed order:** **#3**.

### 4) Drop manual wiring flags in favor of EnTT connection ownership
- **File/path:**  
  - `app/input/input_dispatcher.cpp` (`InputDispatcherWiringState`)  
  - `app/entities/obstacle_render_entity.cpp` (`ObstacleMeshLifetimeState`, `ObstacleModelLifecycleState`)  
  - `app/session/test_player_session.cpp` (`TestPlayerSessionSignals`)
- **Current pattern:** custom `ctx` structs with `bool wired` and manual connect/disconnect bookkeeping.
- **EnTT replacement/API:** store `entt::scoped_connection` (or a connection-owner struct) in `ctx` for deterministic connect/disconnect lifecycle.
- **Touched dependencies:** startup/shutdown wiring, test-player signal setup, model/mesh cleanup hooks.
- **Migration risks:** medium; disconnect timing must remain correct relative to `reg.clear()` and resource teardown.
- **Proposed order:** **#4**.

## Medium-confidence / design-gated candidates

### 5) Queue unification via `entt::dispatcher` for scoring side-effects
- **File/path:**  
  - `app/components/gameplay_intents.h`  
  - `app/systems/scoring_system.cpp`  
  - `app/systems/energy_system.cpp`  
  - `app/systems/popup_feedback_system.cpp`
- **Current pattern:** custom ctx vector queues (`PendingEnergyEffects`, `ScorePopupRequestQueue`) drained by downstream systems.
- **EnTT replacement/API:** `dispatcher.enqueue<EnergyEffectEvent>()` / `enqueue<ScorePopupRequest>()` + explicit drain boundary with `update<T>()`.
- **Touched dependencies:** scoring → energy/popup flow, fixed tick ownership, test fixtures depending on legacy fields.
- **Migration risks:** medium; event ordering semantics are gameplay-visible and must remain deterministic.
- **Proposed order:** **after** HC 1-4 and only with characterization tests.

### 6) Replace `any_of<BarObstacleTag>` branch with structural split for miss path
- **File/path:** `app/systems/scoring_system.cpp`
- **Current pattern:** inside miss loops, per-entity branch `reg.any_of<BarObstacleTag>(e)` to set death cause.
- **EnTT replacement/API:** two structural views (`... , BarObstacleTag` and `... , entt::exclude<BarObstacleTag>`) to remove branch and clarify intent.
- **Touched dependencies:** miss/death cause semantics.
- **Migration risks:** low-medium; primarily refactor risk and code churn.
- **Proposed order:** optional optimization after profiler confirmation.

### 7) Organizer adoption evaluation (likely defer)
- **File/path:**  
  - `app/systems/fixed_tick_runner.cpp`  
  - `app/systems/playing_systems_runner.cpp`
- **Current pattern:** explicit handwritten fixed-step ordering with extensive invariants in comments.
- **EnTT replacement/API:** `entt::organizer` for dependency declarations/task graph.
- **Touched dependencies:** all gameplay-system ordering.
- **Migration risks:** high; current ordering is semantic and intentionally explicit.
- **Proposed order:** defer unless parallel scheduling or growth pressure justifies it.

## Keep / false-positive guardrails (EnTT should NOT be used here)

### A) Keep explicit fixed-tick runner ordering
- **Why not EnTT here:** this is gameplay policy, not registry data-shape. Organizer indirection risks accidental reorder of phase-sensitive logic.

### B) Keep collect-then-remove buffers in structural mutation systems
- **File/path:** `scoring_system.cpp`, `obstacle_despawn_system.cpp`, `particle_system.cpp`, `popup_display_system.cpp`
- **Why not EnTT alternative:** current pattern is EnTT-safe; in-loop remove/destroy on active pools risks iterator invalidation and missed entities.

### C) Keep raylib/platform resource ownership as explicit RAII/domain code
- **File/path:** `app/systems/camera_system.cpp`, `app/rendering/camera_resources.h`, `app/audio/*`, `app/platform/*`
- **Why not EnTT resource/cache:** these are external API lifetimes (GPU/audio/platform callbacks), where explicit domain ownership is clearer and safer than generic cache indirection.

### D) Keep persistence/file I/O as domain services, not entity/component data
- **File/path:** `app/util/settings_persistence.cpp`, `app/util/high_score_persistence.cpp`, `app/util/beat_map_loader.cpp`
- **Why not EnTT here:** this is cold I/O and serialization logic; turning into ECS flow reduces clarity without meaningful data-locality wins.

## Recommended migration order
1. Replace `ObstacleCounter` with `storage<ObstacleTag>()` checks.  
2. Convert singleton cameras to `ctx()` singletons.  
3. Eager-init scratch ctx objects and remove lazy hot-path `find/emplace`.  
4. Replace ad-hoc wiring flags with EnTT connection ownership (`scoped_connection`).  
5. Decide if gameplay intent queues should be unified onto dispatcher.  
6. Only then consider structural micro-optimizations and organizer experiments.

## Parallel review convergence note
Parallel C++ and architecture passes converged on: (a) obstacle counter cleanup, (b) singleton management cleanup, (c) preserving explicit ordering and collect-then-remove safety, and (d) treating organizer/group migrations as design-gated rather than immediate cleanup.

### 2026-05-08T13:41:05.026-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Do not preserve local wall-clock time helpers for logging; prefer song time or time since game start, using values already provided by raylib/game runtime APIs.
**Why:** User request — captured for team memory

### 2026-05-08T13:43:06.866-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Use `docs/raylib/cheatsheet.md` as the local reference for available raylib APIs when evaluating raylib-based cleanup and replacements.
**Why:** User request — captured for team memory

### 2026-05-08T13:44:53.252-07:00: Keaton utility cleanup implementation

---
date: 2026-05-08T13:44:53.252-07:00
author: keaton
topic: utility-cleanup-implementation
---

## Context
Approved utility cleanup required deleting dead `app/util` surfaces while preserving behavior in session flow, persistence status handling, and runtime game-state transitions.

## Decision
- Replaced `ObstacleCounter` signal bookkeeping with direct `reg.view<ObstacleTag>().empty()` checks in `game_state_system`, and removed counter wiring from session/test setup.
- Replaced `fs_utils` wrappers with raylib `DirectoryExists`/`MakeDirectory` checks at persistence callsites, returning `DirectoryCreateFailed` with a concrete `std::errc::io_error` on failure.
- Removed `enum_names` and `test_player_helpers` by inlining owner-local helpers in `test_player_system.cpp` and switching tests to direct `magic_enum::enum_name(...)` usage.
- Removed local wall-clock timestamping (`safe_localtime`) from session/test logging; log headers now use runtime seconds and test-player log filenames use runtime milliseconds + monotonic sequence to avoid collisions without wall-clock time.

## Validation
- Stale-reference sweep over `app/`, `tests/`, `benchmarks/`, and `CMakeLists.txt` found no remaining includes/symbol references to deleted utility files.
- Full validation passed: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests` (`2052 assertions`, `754 test cases`).

### 2026-05-08T13:44:53.252-07:00: Baer utility cleanup validation gate

---
date: 2026-05-08T13:44:53.252-07:00
author: baer
topic: utility-cleanup-validation-gate
---

## Context
Validation for utility cleanup was requested while cleanup targets are still present in-tree.

## Observation
- Cleanup targets still exist: `app/util/obstacle_counter.*`, `fs_utils.h`, `enum_names.h`, `safe_localtime.h`, `test_player_helpers.h`.
- Stale-reference sweep shows active dependencies in `app/` and `tests/` for all five surfaces.
- Full validation currently passes (`2063 assertions`, `758 test cases`), so this is pre-cleanup baseline, not post-cleanup verification.

## Decision
Treat this run as **preflight baseline**. Post-cleanup validation must be rerun after Keaton lands production rewires/deletions.

## Next Validation Gate
After cleanup lands, require:
1. Zero references/includes for deleted headers/symbols across `app/`, `tests/`, `benchmarks/`, `CMakeLists.txt`.
2. Focused checks for signal lifecycle, test-player helper coupling removal, persistence directory-status paths, enum-name output replacement, and session/test logging time-source behavior.
3. Full build+tests green.

### 2026-05-08T13:57:50.741-07:00: Keaton systems raylib audit

**When:** 2026-05-08T13:57:50.741-07:00  
**Requested by:** yashasg  
**Scope:** Read-only audit of all `app/systems/` files against `docs/raylib/cheatsheet.md`  
**Implementation status:** No product code changes made

[Full audit findings attached as separate decision entries below]

**Safe candidates to consider after green signal**
1. Collapse gesture detection in `input_system.cpp` to one `GetGestureDetected()` read.
2. Replace `popup_display_system.cpp` manual alpha clamp with raymath `Clamp`.
3. Replace `collision_system.cpp` manual precision clamp with raymath `Clamp`.
4. Optionally replace floor ring rlgl vertex emission with two `DrawTriangle3D(...)` calls per segment, after measuring.

**Design-gated candidates**
1. Textured floor-ring architecture using `LoadRenderTexture` + `DrawRing` + textured floor mesh.
2. Removing `screen_to_virtual(...)` in favor of raylib mouse scale/camera mapping across all input.
3. Replacing `platform_get_display_size(...)` with raylib size APIs after web canvas behavior is redesigned.
4. Replacing audio/haptic/session-log queues with direct raylib APIs; current queue/lifecycle/testability contracts block direct substitution.
5. Replacing owned-model `DrawMesh(...)` paths with `DrawModel/DrawModelEx`; tint, transform, and ownership semantics must be proven first.

### 2026-05-08T13:57:50.741-07:00: Baer systems raylib audit test plan

**Date:** 2026-05-08T13:57:50.741-07:00  
**By:** Baer  
**Requested by:** yashasg  
**Scope:** Read-only regression/test audit for raylib replacement candidates across `app/systems/`, using `docs/raylib/cheatsheet.md` as the API reference. No product implementation performed.

**Executive verdict:** Proceed only in phases. The already-approved direct substitutions (`DrawLine3D`, `DrawPoly`, `LoadFileText`) are testable with stale-reference sweeps plus build/tests, but the remaining systems cleanup is not uniformly safe.

[Comprehensive coverage matrix and validation gates in separate decision log]

**Key high-risk behavior that must be preserved**
1. Input event timing must be delivered once per frame; no replay.
2. Touch/mouse source isolation on web/mobile.
3. Letterbox mapping must preserve virtual 720x1280 coordinate space.
4. Focus-loss pause uses edge-triggered `IsWindowFocused()`.
5. GPU headless safety for render targets/meshes.
6. Render pass ordering: floor → flush → disable depth → world/effects → flush → re-enable depth → EndMode3D.
7. Floor ring annuli live in world XZ space, not screen-space 2D.
8. Audio stream pumping `UpdateMusicStream()` runs every frame after music starts.
9. Song-time authority for scroll, shape windows, beat spawning, collision timing (not frame-accumulated time).
10. Terminal phase latch: finished songs must not restart or loop.
11. Bounded side-effect queues: audio/haptic drop when full, drain once per frame.
12. Collect-then-remove for scoring/despawn/popup/particle mutations (no active-iteration mutation).
13. Strict collision edges for lane overlap and beat-line crossing (raylib helpers may differ at touching boundaries).
14. High-score persistence semantics: terminal phase owns update/save/dirty behavior.

### 2026-05-08T13:57:50.741-07:00: McManus systems raylib audit

**When:** 2026-05-08T13:57:50.741-07:00  
**Requested by:** yashasg  
**Scope:** `app/systems/*` gameplay/rhythm/scoring/obstacle audit against `docs/raylib/cheatsheet.md`  
**Implementation status:** No code changes made.

[Audit findings cover collision/scoring/audio/haptic/coordinate systems; no implementation performed]

**Safe direct replacements identified:**
- Lane overlap: Replace `lane_overlaps` manual x-interval with `CheckCollisionRecs(Rectangle, Rectangle)` after boundary-parity testing.
- Precision clamp: Replace manual `if` clamps in `collision_system.cpp::grade_shape_timing` with raymath `Clamp(precision, 0.0f, 1.0f)`.
- Alpha clamp: Replace manual `if` clamps in `popup_display_system.cpp` with raymath `Clamp(alpha_ratio, 0.0f, 1.0f)`.

**Design-gated candidates:**
- Audio SFX dispatch callback removal (requires test injection architecture review).
- Song playback state duplication (`MusicContext::started` vs `IsMusicStreamPlaying()`; design-gated because pause/resume semantics).
- Display-size helper seam (`platform_get_display_size` → raylib `GetScreenWidth/Height`; web canvas sizing blocks this).
- Gamepad rumble as additive haptic backend (not replacement for iOS/phone haptics).

### 2026-05-08T13:57:50.741-07:00: Fenster systems raylib audit

**When:** 2026-05-08T13:57:50.741-07:00  
**By:** Fenster  
**Requested by:** yashasg  
**Scope:** `app/systems/` with extra focus on audio, song playback, beat scheduling/logging, haptics, and directly referenced audio/platform helpers.  
**Implementation status:** No product code changes made.

[Audit findings cover audio queue dispatch, song stream playback state, haptic platform bridge, collision lane overlap, popup alpha, floor ring rendering, and screen-to-virtual coordinate mapping]

**Safe cleanup candidates:**
1. Popup alpha `Clamp`/`Fade` (raylib API).
2. Collision lane overlap via `CheckCollisionRecs()` with edge tests.

**Design-gated review required before implementation:**
1. Audio callback removal/direct `PlaySound()` path (removes test injection seam).
2. Song playback state/duration authority (`IsMusicStreamPlaying`, `SeekMusicStream`, `GetMusicTimeLength`; pause/resume semantics block this).
3. Gamepad rumble as additive non-iOS haptic path.
4. Floor rendering architecture pass for `DrawRing()` on a floor texture (high visual regression risk).
5. Screen-to-virtual coordinate system as raylib `Camera2D` + `GetScreenToWorld2D()` (affects input/UI hit-testing).

### 2026-05-08T14:25:19.068-07:00: Edge/audio cleanup validation gate
**By:** Baer
**What:** For cleanup passes, validation should include scoped added/deleted/net LOC by product vs tests, live-code stale-reference sweeps, and targeted behavior checks before accepting “slim” as complete.
**Why:** This keeps cleanup accountable to the line-reduction goal while preserving edge collision and audio safety coverage.
**Evidence:** Edge/audio slice validated at app -23 LOC, tests +11 LOC, total -12 LOC; live stale refs for `SFXPlaybackBackend`, `sfx_playback_backend_init`, and `lane_overlaps` were zero; full build and tests passed.
# 2026-05-08T14:14:30.000-07:00: Baer edge collision/audio validation

**Decision/learning:** Edge-inclusive lane collision must be locked by tests whenever using raylib `CheckCollisionRecs`. Raylib's rectangle check excludes exact edge contact by default, so this cleanup keeps the raylib API path while minimally inflating the centered hitbox to make touching edges count as collisions.

**Validation:**
- Added/confirmed edge tests for exact touch and just-beyond-edge miss.
- Confirmed `SFXPlaybackBackend`, `sfx_playback_backend_init`, and `lane_overlaps` are absent from `app/`, `tests/`, `benchmarks/`, and `CMakeLists.txt`.
- Confirmed audio tests cover headless queue draining, capacity drain, no bank, invalid SFX, unloaded sounds, and no-audio-device bank lifecycle safety.
- Full validation passed: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests`.
### 2026-05-08T14:25:19.068-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Cleanup passes should reduce total lines of code where possible; adding large test/API surface during cleanup defeats the purpose unless clearly justified.
**Why:** User request — captured for team memory
### 2026-05-08T14:45:31.166-07:00: Keaton EnTT cleanup LOC audit

Scope: read-only scan of `app/systems`, `app/components`, and session wiring touchpoints.

Decision summary:
- EnTT can remove some bookkeeping, but most gameplay logic should remain in systems.
- Largest practical code reduction is in test-player planning state and small hot-path cleanup in popup/scratch helpers.
- Signal/dispatcher rewires are mostly architecture changes that move code rather than shrink it.

Quantitative estimate (net LOC):
- Safe now: ~45–80 LOC reduction.
- Medium (design-gated): ~35–90 LOC reduction, mostly moved/restructured.
- Not worth for LOC: <20 LOC or neutral; keep in systems.

Key safe candidates:
1. `app/systems/test_player_system.cpp` + `app/components/test_player.h`
   - Replace `planned[]/planned_count` + `reg.valid` cleanup with existential planned tag on obstacle entities.
   - Estimated net: -30 to -55 LOC.
2. `app/systems/popup_display_system.cpp`
   - Use structural view `<ScorePopup, PopupDisplay, Color>` instead of `view<ScorePopup> + try_get` checks.
   - Estimated net: -8 to -15 LOC.
3. Scratch singleton helpers (`particle_system`, `obstacle_despawn_system`, `popup_display_system`, parts of `scoring_system`)
   - Eager-init in setup and use `ctx().get<T>()` in systems.
   - Estimated net: -7 to -10 LOC (mostly helper deletion; some setup additions).

Medium candidates (mostly moves/restructure):
1. Signal wiring flags in session/input/obstacle lifecycle to `entt::scoped_connection` owners.
   - Estimated net: -5 to +10 LOC (cleanup/readability win, minimal LOC win).
2. `PendingEnergyEffects` / `ScorePopupRequestQueue` to dispatcher events.
   - Estimated net: -10 to +25 LOC (often code moves, not shrinks).
3. Camera singleton entity pattern to context singleton.
   - Estimated net: -20 to -55 LOC across camera accessors and checks, but touches many tests/assumptions.

Not-worth for LOC (keep in systems):
- `scoring_system` collect-then-remove passes.
- `collision_system` per-kind structural loops.
- `particle_system` / `obstacle_despawn_system` collect-then-destroy safety pattern.

Rationale:
- EnTT helps best where we currently emulate entity state in manual arrays or repeated lookup helpers.
- For phase-sensitive gameplay rules, moving logic out of systems would mostly hide behavior and increase coupling without meaningful code reduction.
# Keyser — EnTT lifecycle audit decision (read-only)

Date: 2026-05-08T14:45:31.166-07:00  
Requested by: yashasg

## Decision

Use EnTT APIs to remove orchestration/plumbing from systems (signal wiring state, lazy ctx scratch setup, queue handoff boilerplate), but **do not** move gameplay rules into component/entity callbacks. Keep components plain data and systems as deterministic free-function phase steps.

## Why

- Project architecture explicitly depends on strict fixed-step ordering and unidirectional writes.
- Callback-heavy gameplay (on_update/on_construct-driven rule evaluation) would make order, replayability, and tests harder to reason about.
- EnTT lifecycle hooks are best for ownership/lifetime side-effects, not core game decisions.

## Scope guidance

1. **Good EnTT moves**
   - Replace ad-hoc `wired` bool state with owned EnTT connection objects in context.
   - Eager-init ctx scratch/queue holders in session/init and use `ctx().get<T>()` in hot loops.
   - Consider dispatcher unification for intent queues if fixed-tick drain boundaries remain explicit.
   - Keep using storage/view helpers for structural checks (`view<...>().empty()`, `storage<T>()`).

2. **Do not move**
   - Collision/scoring/energy/game-state transitions into callbacks (`on_construct/on_update/on_destroy`).
   - Fixed tick ordering into implicit signal chains or observer-triggered state transitions.
   - External resource ownership (raylib model/audio/window lifetime) into generic ECS callback paths.

## Expected impact (high-level)

- Plumbing reduction in systems/input/session glue: moderate (tens to low-hundreds LOC).
- Gameplay logic reduction inside systems: low and intentionally capped.
- Determinism/testability risk: low only if gameplay remains in explicit phase-run systems.
# Keyser — Systems-Wide Raylib Rewire/Removal Plan

**Date:** 2026-05-08T13:57:50.741-07:00  
**By:** Keyser (Lead Architect)  
**Requested by:** yashasg  
**Status:** Plan only — no implementation. Awaiting green signal.  
**Sources:** keaton-systems-raylib-audit.md, mcmanus-systems-raylib-audit.md, fenster-systems-raylib-audit.md, baer-systems-raylib-audit-tests.md + spot-checked `app/systems/` against `docs/raylib/cheatsheet.md`.

---

## 1. Safe First-Pass Rewires

These are low-risk, self-contained substitutions. Zero design decisions required. Order within this group is flexible.

### 1a. `input_system.cpp` — Collapse repeated `IsGestureDetected()` fan-out to one `GetGestureDetected()` read

- **File:** `app/systems/input_system.cpp` lines 155–168  
- **Symbol:** Five `IsGestureDetected(GESTURE_SWIPE_*)` / `IsGestureDetected(GESTURE_TAP)` calls → redundant per-frame polling  
- **Raylib API:** `int GetGestureDetected(void)` (cheatsheet)  
- **Steps:**
  1. Read `const int gesture = GetGestureDetected();` once inside the touch-release block.
  2. Replace the five `if (IsGestureDetected(...))` branches with `if (gesture == GESTURE_SWIPE_RIGHT)` etc.
  3. Remove the fallback `gesture = GetGestureDetected()` line (now redundant).
  4. Keep `SetGesturesEnabled(...)` and `InputState::gestures_configured` — these are still needed.
- **Stale-reference sweep:** `rg IsGestureDetected app/systems/input_system.cpp` must return zero hits after.
- **Tests:** `test_input_pipeline_behavior.cpp`, `test_player_action_rhythm.cpp`, `test_event_queue.cpp`; manual web/mobile gesture smoke for swipe and tap.

### 1b. `collision_system.cpp` — Replace manual precision clamp with `Clamp`

- **File:** `app/systems/collision_system.cpp` lines 76–77  
- **Symbol:** `if (precision < 0.0f) precision = 0.0f; if (precision > 1.0f) precision = 1.0f;` inside `grade_shape_timing` lambda  
- **Raylib API:** `float Clamp(float value, float min, float max)` from `<raymath.h>`  
- **Steps:**
  1. Ensure `#include <raymath.h>` is present (already likely via raylib).
  2. Replace the two `if` clamp branches with `precision = Clamp(precision, 0.0f, 1.0f);`.
  3. Remove now-deleted branches; leave surrounding code untouched.
- **Tests:** `test_collision_system.cpp`, `test_collision_extended.cpp`, `test_scoring_system.cpp`; confirm timing-tier assignment unchanged.

### 1c. `popup_display_system.cpp` — Replace manual alpha clamp with `Clamp`

- **File:** `app/systems/popup_display_system.cpp` lines 38–39  
- **Symbol:** `if (alpha_ratio < 0.0f) alpha_ratio = 0.0f; if (alpha_ratio > 1.0f) alpha_ratio = 1.0f;`  
- **Raylib API:** `float Clamp(float value, float min, float max)` from `<raymath.h>`  
- **Steps:**
  1. Replace the two manual branches with `alpha_ratio = Clamp(alpha_ratio, 0.0f, 1.0f);`.
  2. The `pd->a` write stays as-is.
- **Tests:** `test_popup_display_system.cpp`, `test_scoring_extended.cpp`; test alpha at full life, mid-life, expired (negative remaining), and `max_time <= 0`.

### 1d. `floor_render_system.cpp` — Replace rlgl immediate-mode ring emission with `DrawTriangle3D` (pending perf check)

- **File:** `app/systems/floor_render_system.cpp` lines 93–128  
- **Symbol:** `rlBegin(RL_TRIANGLES)` / `rlColor4ub` / `rlVertex3f` / `rlEnd()`  
- **Raylib API:** `void DrawTriangle3D(Vector3 v1, Vector3 v2, Vector3 v3, Color color)` (cheatsheet)  
- **Steps:**
  1. For each annulus segment (currently 6 vertices = 2 triangles), emit two `DrawTriangle3D(...)` calls using the same XZ-plane vertex coordinates.
  2. After the loop, `rlEnd()` and `rlBegin` wrappers disappear.
  3. Remove `#include <rlgl.h>` from `floor_render_system.cpp` if no other rlgl calls remain in that file.
  4. Measure: if `DrawTriangle3D` call-overhead exceeds batched immediate-mode by a measurable frame budget, revert to rlgl.
- **Behavior gate:** Visual parity — ring thickness, winding, culling, alpha, zero new compiler warnings (no unused-variable from removed rlgl calls).
- **Tests:** `test_perspective.cpp` (floor-ring angular sweep), full suite; manual render smoke on native + WASM.

---

## 2. Medium-Risk Rewires

These require design confirmation or additional tests before proceeding.

### 2a. `collision_system.cpp` — Replace inline `lane_overlaps` lambda with `CheckCollisionRecs`

- **File:** `app/systems/collision_system.cpp` line 100 lambda  
- **Symbol:** `lane_overlaps` — `(dx > -PLAYER_SIZE) && (dx < PLAYER_SIZE)` open-interval  
- **Raylib API:** `bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2)` — **uses closed interval (`<=`)**  
- **What must be preserved:** Current lambda uses strict open-interval (`<`, not `≤`). `CheckCollisionRecs` uses `rec1.x + rec1.width >= rec2.x`, i.e., touching edges collide. A same-result border case change is gameplay-visible (hit becomes miss at exact edge).
- **Steps:**
  1. Add boundary tests: `just_inside`, `exactly_at_edge` (currently a miss), `just_outside` for all shape gates.
  2. If tests confirm `CheckCollisionRecs` closed-interval produces the same gameplay outcome (edge is still a miss because player/obstacle sizes prevent exact touching in practice), then replace.
  3. Delete the lambda comment and `lane_overlaps` local after swap.
  4. Align with `test_player_system.cpp` `lane_overlap_rect()` for shared semantics.
- **Tests:** `test_collision_system.cpp`, `test_collision_extended.cpp`, plus newly written boundary edge tests.

### 2b. `audio_system.cpp` — Remove `SFXPlaybackBackend` callback seam, call `PlaySound` directly

- **File:** `app/systems/audio_system.cpp` + `app/audio/audio_types.h` + `app/audio/sfx_bank.h/.cpp`  
- **Symbol:** `SFXPlaybackBackend::dispatch`, `sfx_playback_backend_init()`  
- **Raylib API:** `void PlaySound(Sound sound)`, `bool IsAudioDeviceReady(void)`, `bool IsSoundValid(Sound sound)`  
- **What must be preserved:** Headless/test injection seam. Removing the callback eliminates the mock path that tests use to observe SFX delivery.
- **Steps:**
  1. Replace `SFXPlaybackBackend::dispatch` call with: guard `IsAudioDeviceReady()` + `IsSoundValid(bank.sounds[idx])` → `PlaySound(...)`.
  2. Delete `SFXPlaybackBackend` struct from `audio_types.h`.
  3. Delete `sfx_playback_backend_init()` from `sfx_bank.h/.cpp`.
  4. Update game-loop and test wiring that currently sets the callback.
  5. Provide a compile-time or headless guard for no-audio context (WASM audio unlock, headless test builds) via `IsAudioDeviceReady()` check.
- **Tests:** `test_audio_system.cpp`; update/replace callback-capture tests with `IsSoundValid`/`IsAudioDeviceReady` guarded assertions; manual native + WASM audio smoke.

### 2c. `song_playback_system.cpp` — Evaluate `IsMusicStreamPlaying` to replace `MusicContext::started`

- **File:** `app/systems/song_playback_system.cpp` + `app/audio/music_context.h`  
- **Symbol:** `MusicContext::started`  
- **Raylib API:** `bool IsMusicStreamPlaying(Music music)`  
- **What must be preserved:** `started` means "stream has been started and may be paused" — NOT "currently playing." `IsMusicStreamPlaying` returns false while paused, which would break the pause-resume-terminal latch sequence.
- **Steps:**
  1. Audit every use of `started` flag across `song_playback_system.cpp` and `setup_play_session()`.
  2. If the flag only gates `UpdateMusicStream` pumping (must pump even while paused), keep an explicit ECS-owned playback-state enum (`Idle | Started | Paused | Stopped`) instead of `started` bool, so `IsMusicStreamPlaying` is not misused.
  3. Only delete `started` if all sites provably only need "currently playing" semantics.
- **Tests:** `test_song_playback_system.cpp`, `test_song_playback_extended.cpp`, terminal-phase regression; manual: start → pause → resume → song complete; verify no restart.

---

## 3. Design-Gated / No-Go Items

Do NOT implement until explicit design decision is made.

### 3a. Floor rings via `DrawRing` + textured floor plane

- **What it would touch:** `floor_render_system.cpp` (remove rlgl path entirely), `camera_system.cpp` + `camera_resources.h` (new floor `RenderTexture` lifecycle), possibly `game_render_system.cpp` (texture pass ordering), floor-sampling shader/material, `tests/test_gpu_resource_lifecycle.cpp`.
- **Why gated:** Requires: (a) shader/material changes to sample texture diffuse (current shader only uses `colDiffuse`), (b) new RAII floor texture ownership and lifecycle, (c) UV/Y-flip validation (RenderTexture is flipped Y vs screen convention), (d) render-target headless guard. This is a feature refactor, not cleanup. The safe `DrawTriangle3D` swap in §1d is the near-term substitute.

### 3b. `screen_to_virtual` removal via `GetScreenToWorld2D` + `Camera2D`

- **Why gated:** `ScreenTransform` is used by the blit path in `game_loop.cpp`, UI mouse offset/scale in `ui_render_system.cpp`, and gameplay tap hit testing in `input_system.cpp`. Moving ownership to a `Camera2D` changes the input/camera/blit contract across three systems simultaneously. Risk of letterbox hit-testing regression is high.

### 3c. `platform_get_display_size` removal via `GetScreenWidth/Height`

- **Why gated:** Web path owns CSS canvas synchronization and HiDPI buffer sizing. Native-only inlining is safe but changes only `platform_display.cpp`, not a system. Requires web canvas redesign before the seam can be deleted.

### 3d. Audio/haptic queue inlining to direct raylib

- `haptic_system.cpp` / platform haptics bridge: `SetGamepadVibration` is gamepad-only; not equivalent to iOS/phone haptic engines. Keep platform bridge. Gamepad rumble is an *additive* backend in `platform::haptics::trigger`, not a replacement.
- `beat_log_system.cpp` / `SetTraceLogCallback`: callback is process-global; not equivalent to per-session structured BEAT telemetry. Keep scoped logger.

### 3e. `DrawModel/DrawModelEx` to replace owned-model `DrawMesh` paths

- **Why gated:** `game_render_system.cpp` uses local material copies for per-entity tint and shared-mesh safety. `DrawModel/DrawModelEx` semantics do not map cleanly without first proving transform, tint, and shared-material ownership equivalence.

### 3f. `SeekMusicStream` for restart / `GetMusicTimeLength` for duration authority

- **Why gated:** Beatmap and music file durations may intentionally differ; changing authority shifts `SongComplete` timing. `SeekMusicStream` behavior at stream end needs explicit validation in non-looping mode.

---

## 4. Files/Systems With No Useful Raylib Equivalent

These systems are pure ECS/game-domain logic. No rewiring is warranted from this audit.

| File | Why no useful replacement |
|---|---|
| `beat_scheduler_system.cpp` | Beatmap spawn timing, overshoot compensation, lane masking — no raylib equivalent |
| `scroll_system.cpp` | Song-time-derived obstacle position math; `GetFrameTime` is wrong here |
| `shape_window_system.cpp` | Song-time state machine |
| `motion_system.cpp` | ECS integration; component math doesn't benefit from raylib helpers |
| `miss_detection_system.cpp` | Threshold tag logic |
| `particle_system.cpp` | Particle lifetime/gravity ECS state |
| `obstacle_despawn_system.cpp` | Collect-then-remove ECS destruction |
| `player_movement_system.cpp` | Already uses raymath `Clamp`/`Lerp`; jump trajectory is custom |
| `energy_system.cpp` | Already uses raymath `Clamp`; flash timer is ECS |
| `scoring_system.cpp` | Score/combo/multiplier accounting; `std::floor` for point conversion is correct |
| `player_input_system.cpp` | Dispatcher-event consumption; downstream of raylib input |
| `game_state_system.cpp` | Phase transitions, dispatcher drains, terminal check |
| `game_state_end_screen_system.cpp` | End-screen phase resolution |
| `game_state_terminal_phase_system.cpp` | High-score persistence trigger; not a raylib API concern |
| `playing_systems_runner.cpp` | Phase gate and system order |
| `fixed_tick_runner.cpp` | Fixed-step contract |
| `popup_feedback_system.cpp` | Score-popup spawn queue and SFX enqueue |
| `test_player_system.cpp` | Automation AI; `CheckCollisionRecs` already used for lane rects |
| `input_system.cpp` (WebInputPolicy) | Platform device detection; `GetTouchPointCount` doesn't replace user-agent/maxTouchPoints |
| `camera_system.cpp` (matrix work, RAII, world-to-screen) | Already uses direct raylib/raymath; remaining code is resource ownership |
| `ui_render_system.cpp` | Already uses direct raylib; `SetMouseOffset/Scale` scoped pattern is the correct approach |
| `song_playback_system.cpp` (beat/phase logic) | MusicContext state ownership, beat advancement, and SongComplete latch are game policy |
| `audio_system.cpp` (queue semantics) | Bounded ECS audio queue, drop policy, and SFX enum→bank mapping are game architecture |
| `haptic_system.cpp` | Platform haptics bridge; no full raylib equivalent |
| `beat_log_system.cpp` | Scoped session telemetry; `TraceLog` is not a substitute |
| `all_systems.h`, `camera_system.h`, `floor_render_system.h` | Declarations only |

---

## 5. Recommended Implementation Order (after green signal)

### Wave 1 — Pure substitutions, full suite green before Wave 2

1. **`popup_display_system.cpp`** — `Clamp` alpha (§1c). Smallest changeset, easy to verify.
2. **`collision_system.cpp`** — `Clamp` precision (§1b). Same `<raymath.h>` include touch.
3. **`input_system.cpp`** — `GetGestureDetected()` collapse (§1a). Requires manual gesture smoke.
4. **`floor_render_system.cpp`** — `DrawTriangle3D` ring path (§1d). Measure frame budget before committing; remove `<rlgl.h>` after.

**Between waves:** Run stale-reference sweeps:
```
rg "IsGestureDetected" app/systems/
rg "rlBegin|rlVertex3f|rlColor4ub|rlEnd|<rlgl.h>" app/systems/floor_render_system.cpp
```
Both must return zero. Run `./run.sh test` for full pass confirmation.

### Wave 2 — Boundary-confirmed medium risk

5. **`collision_system.cpp`** — `CheckCollisionRecs` lane overlap (§2a). **Only** after boundary edge tests are written and confirmed.
6. **`audio_system.cpp`** — Direct `PlaySound` path (§2b). Needs updated headless SFX tests.

**Between waves:** Stale-reference sweep:
```
rg "SFXPlaybackBackend\|sfx_playback_backend_init\|lane_overlaps" app/
```
Must return zero. Full suite + WASM build.

### Wave 3 — Design-decision-required items (schedule separately)

7. **`song_playback_system.cpp`** — `MusicContext::started` audit (§2c). Requires terminal latch regression and real audio manual validation.
8. **Floor texture architecture** (§3a) — Treat as a feature task. New GPU lifecycle tests, visual/UV validation, shader/material changes; full separate spec before implementation.
9. **Haptics gamepad branch** (§3d partial) — Additive only; keep iOS bridge intact.

### Post-implementation deletion sweeps

After each wave, run targeted sweeps to catch stale includes and dead helpers:
- `rg "<rlgl.h>" app/` — should only survive in files that still directly need it (game_render_system, camera_system)
- `rg "screen_to_virtual\|ScreenTransform" app/` — must remain until design decision (§3b)
- `rg "SFXPlaybackBackend\|sfx_playback_backend_init" app/` — must be zero after Wave 2 item 6
- `rg "IsGestureDetected" app/systems/input_system.cpp` — must be zero after Wave 1 item 3
- Full suite: `./run.sh test`; native build: `cmake --build build`; WASM: `./run.sh wasm` if available

---

## Invariants That Must Survive Every Wave

1. **Collect-then-remove:** no structural EnTT mutations during iteration.
2. **Song-time authority:** scroll, shape windows, spawning, and collision timing derive from audio/song position — never `GetFrameTime`.
3. **Letterbox coordinate:** input and raygui hit testing stay in virtual 720×1280 space; `SetMouseOffset/Scale` scoped in `ui_render_system` is not removed.
4. **GPU headless safety:** all resource load/unload stays RAII-guarded for no-GPU test context.
5. **Audio stream pumping:** `UpdateMusicStream` runs every frame after stream start, including while paused.
6. **Render pass ordering:** floor first → flush → disable depth for effects → flush → re-enable → `EndMode3D`.
7. **Bounded queues:** audio/haptic queues intentionally drop when full and drain once per frame.
8. **Zero warnings:** all changes compile with `-Wall -Wextra -Werror`; check for unused vars after removing rlgl calls.
# Decision: Add test-results/ to .gitignore

**Author:** Kobayashi (CI/CD Release Engineer)  
**Date:** 2026-05-08T14:37:19.336-07:00  
**Status:** Completed

## Summary

Added `/test-results/` directory to `.gitignore` to prevent CI/CD test artifacts from being committed to the repository.

## Rationale

- The `test-results/` directory is a local artifact directory used for storing test execution reports and outputs
- This directory should never be committed to source control — only shipped/committed artifacts belong in the repo
- Placement: Grouped with other "Local run/test artifacts" in the `.gitignore` file for consistency and discoverability

## Change Made

Added single line to `.gitignore` (line 33):
```
/test-results/
```

**No duplicates found** — the directory was not previously ignored.

## Files Modified

- `.gitignore` — added one line entry

## Verification

✅ Entry added at the correct location (Local run/test artifacts section)  
✅ No equivalent entries already present  
✅ Follows existing `.gitignore` format conventions  
✅ Git status confirms change applied
# Kujan Final Pre-Commit Review — Edge/Audio/Floor/Util Consolidation

**Reviewer:** Kujan  
**Date:** 2026-05-08T14:36:36.423-07:00  
**Requested by:** yashasg  

## Scope

Consolidated cleanup across multiple prior sessions:
- Removed: `file_logger`, `camera.h`, `shape_vertices`, `obstacle_counter`, `fs_utils`, `enum_names`, `safe_localtime`, `test_player_helpers`, `SFXPlaybackBackend`
- Added: `floor_render_system` (split from `game_render_system`)
- Replaced: HUD/beatmap/floor raylib APIs; audio direct guarded playback; collision `CheckCollisionRecs` closed-edge semantics

## Review Findings

| Area | Finding | Status |
|------|---------|--------|
| Stale symbol references | None found in app/, tests/, benchmarks/, CMakeLists.txt | ✓ Clean |
| `audio_system.cpp` queue drain | `audio->count = 0` is unconditional — drains even without audio device | ✓ Correct |
| `game_state_system.cpp` obstacle wait | `reg.view<ObstacleTag>().empty()` replaces ObstacleCounter — ECS-idiomatic | ✓ Correct |
| `beat_map_loader.cpp` memory | `file_text` nulled before `json::parse`; catch block guards correctly | ✓ Sound |
| Collision edge semantics | `CheckCollisionRecs` + `kHitboxEdgePadding=1.0e-4f`; edge test added | ✓ Approved |
| CMakeLists.txt | `file_logger.cpp` removed from all three target lists | ✓ Clean |
| Unrelated deletions | None identified | ✓ Clean |

## Commit-Scope Caveat (Required Action Before Push)

`app/systems/floor_render_system.cpp` and `app/systems/floor_render_system.h` are **untracked new files**. They are `#include`d by `game_render_system.cpp` and declared in `all_systems.h`. They **must** be explicitly staged:

```
git add app/systems/floor_render_system.cpp app/systems/floor_render_system.h
```

Without this, the commit is broken at checkout (missing header and missing CMake glob source).

## Files to Exclude From Product Commit

The following untracked squad/process artifacts should NOT be committed:
- `.squad/health-report-2026-05-08-scribe-session.txt`
- `.squad/health-report-2026-05-08-scribe.txt`
- `.squad/health-report-scribe-session.txt`
- `.squad/scribe-health-report-2026-05-08T20-57-50Z.md`
- `.squad/skills/raylib-3d-floor-annulus/`

## Verdict

**APPROVED** — all code changes are sound. Commit must include the two new `floor_render_system` files and must exclude the squad health-report artifacts listed above.
# McManus collision early-out audit

**When:** 2026-05-08T14:27:31.641-07:00  
**Requested by:** yashasg  
**Scope:** Read-only audit of `app/systems/collision_system.cpp`; referenced `tests/test_collision_system.cpp` and `tests/test_collision_extended.cpp`.  
**Implementation status:** No product code changes made.

## Bottom line

There are safe indentation/line-count wins, but keep the existing per-kind structural views and the current collect/defer ownership model. Do not merge BeatInfo and non-BeatInfo passes casually: collision order is behavior, because the first cleared rhythm obstacle can mutate `ShapeWindow::graded`, `window_scale`, and `window_start`.

The approved lane-overlap direction is already reflected in the current code via `CheckCollisionRecs(...)` in `hitboxes_overlap`. Any cleanup must preserve that closed-edge hitbox behavior and the existing edge-padding shim unless edge tests are deliberately updated.

## Safe mechanical candidates

### 1) `grade_shape_timing` guard-return for already-graded windows

- **Current area:** `app/systems/collision_system.cpp:78-100`.
- **Current shape:** `TimingGrade` is always emplaced, then window mutation is nested under `if (!p_window.graded)`.
- **Proposed early-out shape:** keep `reg.emplace<TimingGrade>(...)` first, then:
  ```cpp
  if (p_window.graded) return;

  const float scale = window_scale_for_tier(tier);
  const float remaining = song.window_duration - p_window.window_timer;
  if (remaining > 0.0f && scale < 1.0f) {
      p_window.window_start -= remaining * (1.0f - scale);
  }
  p_window.window_scale = scale;
  p_window.graded = true;
  ```
- **Behavior risk:** Low if `TimingGrade` remains before the guard. Medium if someone returns before emplacing `TimingGrade`, because later cleared obstacles still need their own grade component even when the player window was already graded.
- **Tests to guard:** existing rhythm grade tests in `tests/test_collision_extended.cpp` lines 58-103 and window-start tests in `tests/test_collision_system.cpp` lines 335-400. Add/keep a multi-obstacle rhythm test if this function is touched.
- **Keaton pass?** Safe only if Keaton is already touching collision timing cleanup; otherwise separate follow-up.

### 2) Extract a local `resolve_shape_result` helper with an internal miss guard

- **Current area:** repeated shape match + optional grade + resolve sequences at `collision_system.cpp:139-142`, `153-154`, `168-171`, `182-183`, `197-200`, `211-212`, `226-227`, `241-242`, `256-257`.
- **Current shape:** each loop computes `shape_ok`/`shape_match`, conditionally grades in rhythm loops, then calls `resolve`.
- **Proposed early-out shape:** local helper after `grade_shape_timing`:
  ```cpp
  auto resolve_shape_result = [&](entt::entity e, float y, Shape required, const BeatInfo* beat) {
      const bool cleared = player_matches_required_shape(p_shape, p_window, required);
      if (!cleared) {
          resolve(e, y, false);
          return;
      }
      if (beat) grade_shape_timing(e, beat->arrival_time);
      resolve(e, y, true);
  };
  ```
  Then each lane-passing branch calls the helper.
- **Behavior risk:** Low if calls happen in the same loops and same order. Medium if helper is combined with view merging or if `resolve` happens before grading, because current behavior grades before tagging `ScoredTag`.
- **Tests to guard:** shape gate pass/fail (`test_collision_system.cpp:5-34`), combo and split pass/fail (`175-280`), Hexagon rejection (`test_collision_extended.cpp:7-54`), rhythm grade tests (`58-139`).
- **Keaton pass?** Good separate follow-up; compact enough for a focused collision-only slimming PR.

### 3) Extract lane-fail guards per obstacle kind without changing miss semantics

- **Current area:** lane/hitbox guards at `collision_system.cpp:134-138`, `148-152`, `163-167`, `177-181`, `192-196`, `206-210`, `221-225`, `236-240`, `251-255`.
- **Current shape:** compute lane condition, miss+continue on failure, then shape logic.
- **Proposed early-out shape:** keep the same `resolve(e, y, false); continue;` semantics, but hide repeated boilerplate behind tiny local helpers, e.g. `miss_if_no_hitbox(e, y, wt.position.x)` or `lane_blocked(blocked)` plus direct guard.
- **Behavior risk:** Low if failing lane/hitbox still tags `MissTag` + `ScoredTag` through `resolve`; high if cleanup changes failure to a pure `continue`, because that stops energy/scoring miss processing.
- **Tests to guard:** shape gate hitbox edge handling (`test_collision_system.cpp:36-53`), combo lane fail (`222-236` and `test_collision_extended.cpp:187-218`), split wrong lane (`266-280`).
- **Keaton pass?** Separate follow-up unless current slimming work already owns this file.

### 4) Invert the `can_grade_shape` branch to reduce one indentation level

- **Current area:** `collision_system.cpp:128-260`.
- **Current shape:** `if (can_grade_shape) { rhythm views + non-BeatInfo views } else { all shape views without grading }`.
- **Proposed early-out shape:** after lane blocks and vertical bars:
  ```cpp
  if (!can_grade_shape) {
      // existing no-grade ShapeGate / ComboGate / SplitPath loops
      return;
  }

  // existing graded BeatInfo loops plus non-BeatInfo loops
  ```
- **Behavior risk:** Low mechanically if all pre-return work remains before the guard. Watch for future code after the branch; an early `return` would skip it.
- **Tests to guard:** no-player/no-phase smoke tests (`test_collision_system.cpp:282-301`), plus all collision obstacle tests. No new behavior coverage required if the branch body is moved unchanged.
- **Keaton pass?** Could be in Keaton's slimming pass if he is intentionally reducing this file; otherwise better as follow-up because it touches a large range.

## Behavior-sensitive candidates to avoid in a casual cleanup

### A) Hoisting `obs_z < player_timing_y` as an early loop `continue`

- **Current area:** `resolve` guard at `collision_system.cpp:63-65`, called after lane/shape checks in rhythm loops.
- **Why tempting:** avoids shape/lane work for obstacles behind the player.
- **Risk:** Current rhythm BeatInfo loops can call `grade_shape_timing(...)` before `resolve(...)` decides the obstacle is behind the timing line. Moving the Y guard earlier may remove a `TimingGrade` side effect for behind-line matching obstacles. That may be desirable, but it is behavior, not mechanical cleanup.
- **Tests needed before changing:** explicit behind-line BeatInfo tests for scored/missed/tagged/graded state; keep beat-line crossing test (`test_collision_system.cpp:145-162`).
- **Recommendation:** Separate follow-up only.

### B) Merging BeatInfo and non-BeatInfo loops with `try_get<BeatInfo>`

- **Current area:** split rhythm/non-rhythm views under ShapeGate, ComboGate, SplitPath (`collision_system.cpp:131-155`, `160-184`, `189-213`).
- **Why tempting:** removes duplicate loops.
- **Risk:** View iteration order can change. Since `grade_shape_timing` mutates `ShapeWindow::graded/window_start` only for the first cleared rhythm obstacle, order is player-visible timing behavior. Structural views were previously approved as a keep pattern in decisions.
- **Tests needed before changing:** multi-obstacle same-frame tests with multiple BeatInfo obstacles and mixed BeatInfo/non-BeatInfo obstacles, asserting `TimingGrade`, `window_start`, and miss/scored tags.
- **Recommendation:** Do not include in Keaton's current slimming pass.

### C) Replacing lane-overlap math or removing hitbox padding

- **Current area:** `centered_hitbox_rect` and `hitboxes_overlap` (`collision_system.cpp:16-20`, `102-105`).
- **Why tempting:** current helper looks small and custom.
- **Risk:** Recent approved collision direction is to use raylib `CheckCollisionRecs` closed-edge semantics. Removing the helper/padding or returning to manual interval checks risks edge regressions.
- **Tests needed before changing:** current edge test (`test_collision_system.cpp:36-53`) plus exact touch-on-edge cases for both sides of player/obstacle hitboxes.
- **Recommendation:** Leave as-is during early-out cleanup.

## Mutation safety notes

- Current loops emplace `MissTag`, `ScoredTag`, and `TimingGrade` on the current entity while iterating structural views. Do not introduce `remove` or `destroy` inside these loops.
- Any future cleanup that removes components or destroys obstacles should use collect-then-remove after the active view is exhausted.
- Keep `resolve` as the single tag-writing seam or an equivalent helper; scattering direct tag writes increases the chance of structural mutation drift.

## Recommended staging

1. **Keaton current pass:** only take the tiny `grade_shape_timing` guard-return if collision timing is already in scope. Otherwise do not broaden the edge/audio cleanup.
2. **Separate collision follow-up:** helper extraction for repeated lane/shape/grade/resolve branches, preserving the same view split and order.
3. **Design/test follow-up:** only then consider Y-guard hoisting or BeatInfo/non-BeatInfo loop merging, after adding explicit ordering and behind-line rhythm tests.



# Baer — EnTT Round 2 coverage update (2026-05-08)

Added focused regression coverage in `tests/test_components.cpp` for approved Round 2 cleanup behavior:
- dispatcher rewire-after-unwire does not duplicate semantic delivery (`[ecs][dispatcher][regression]`),
- repeated `unwire_input_dispatcher` is idempotent before rewire (`[ecs][dispatcher][shutdown]`),
- collision path keeps a stable `SongState` ctx singleton across ticks (`[collision][song_state][regression]`),
- obstacle mesh/model lifecycle unwire rewires remain idempotent and preserve destroy-signal behavior (`[ecs][obstacle][lifecycle]`).

Existing coverage relied on (not duplicated):
- dispatcher idempotent wiring + external-listener preservation (`tests/test_components.cpp` existing cases),
- obstacle spawn/destroy signal semantics via parent-child cleanup (`tests/test_obstacle_archetypes.cpp`).

Known gap:
- Direct `test_player_init` repetition regression test remains blocked in headless/unit context due runtime raylib dependency path causing a hard crash before assertions; should be validated in desktop smoke/integration harness where `game_loop_init(..., test_player_mode=true, ...)` is exercised.

Validation run:
- `cmake --build build --target shapeshifter_tests`
- `./build/shapeshifter_tests "[ecs][dispatcher]"`
- `./build/shapeshifter_tests "[ecs][dispatcher][regression],[ecs][dispatcher][shutdown],[collision][song_state][regression],[ecs][obstacle][lifecycle]"`

Suite note:
- Full suite currently hits an existing unrelated crash at
  `test_redfoot_testflight_ui.cpp:114`
  (`redfoot/#168: collision flags MissedABeat for a missed shape gate`) after rebuild.

# Baer: input-tier collapse test update (2026-05-08)

## Decision
Refactor tests to treat `GoEvent`/`ButtonPressEvent` as the only input contract.

## Why
Production now emits semantic events directly from `input_system`, and `game_state_system` owns the authoritative dispatcher drain. Any test asserting `InputEvent`/gesture-routing internals is now brittle and no longer reflects runtime behavior.

## What changed in test strategy
- Removed `InputEvent`/`InputType`/`run_input_tier1` usage from test helpers and input pipeline tests.
- Updated pipeline/rhythm tests to execute a semantic tick (`game_state_system` drain) before asserting player outcomes.
- Rewrote dispatcher contract coverage around semantic pools only.
- Preserved behavior coverage: same-tick effects, no second-tick replay, phase gating, lane boundary clamping, mixed swipe+button behavior, and listener execution order (documented as current EnTT sink behavior: last connected first).

## Notes for teammates
If listener ordering assumptions change in EnTT or wiring, update `tests/test_entt_dispatcher_contract.cpp` first; many behavioral tests rely on that contract implicitly.

# Baer revision — popup partial-bundle expiry fix

Date: 2026-05-08
Requested by: yashasg
Revision owner: Baer

## Decision

Fix popup expiry regression in `popup_display_system` by covering all structural `ScorePopup` bundles:
- keep optimized fade path for full bundle `ScorePopup + PopupDisplay + Color`
- add expiry paths for partial bundles:
  - `ScorePopup + PopupDisplay` (no `Color`)
  - `ScorePopup + Color` (no `PopupDisplay`)
- keep existing expiry path for `ScorePopup` with neither render component

This restores invariant: every `ScorePopup` decrements `remaining` and expires.

## Tests

Updated `tests/test_popup_display_system.cpp` with two new regression tests:
- `ScorePopup+PopupDisplay expires without Color`
- `ScorePopup+Color expires without PopupDisplay`

## Validation

- `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests "[popup_display]"`
- `./build/shapeshifter_tests`

# Baer — Redfoot/testflight UI segfault coverage (2026-05-08)

## Scope
- Investigated intermittent segfault reported at `tests/test_redfoot_testflight_ui.cpp:114` (`redfoot/#168`) without touching product-side EnTT cleanup work.

## Reproduction notes
- Initial repro observed with:
  - `./build/shapeshifter_tests "[ui][redfoot][game_over][wiring]"`
- Crash signature observed:
  - `SIGSEGV` in test case `redfoot/#168: collision flags MissedABeat for a missed shape gate`.
- Flakiness observed afterward: same filter and full suite passed on repeated reruns.

## Suspected cause
- `collision_system` hard-requires `SongState` via `reg.ctx().get<SongState>()`.
- The crashing test setup previously did not emplace `SongState`, creating undefined behavior in this path.
- Working tree now includes a targeted test setup addition at this case:
  - `reg.ctx().emplace<SongState>();`

## Validation of fix path
- Rebuilt and validated with test-side `SongState` setup present.
- Commands:
  - `./build/shapeshifter_tests "[ui][redfoot][game_over][wiring]"`
  - `./build/shapeshifter_tests "[ui][redfoot]"`
  - `./build/shapeshifter_tests`
- Result: all passed in this run.

## Regression coverage expectation
- Keep the existing wiring test (`MissedABeat for missed shape gate`) as the direct guard.
- Ensure this test always initializes minimal required ctx for systems under test, including `SongState`.
- Optional hardening (future): explicit precondition tests for systems requiring ctx singletons to fail fast in debug builds.

## Baer decision
- No additional regression test added; existing test remains the correct guard once setup is complete.

### 2026-05-08T15:20:16.880-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Treat the `test_redfoot_testflight_ui.cpp:114` segfault as related to the current change stack because it worked before the recent changes and failed after them; do not dismiss it as unrelated.
**Why:** User correction — captured for team memory and review criteria

### 2026-05-08T15:46:54.355-07:00: User directive
**By:** yashasg (via Copilot)
**What:** If the input dispatchers are collapsed, delete the redundant dispatcher-system-style layer; target the input dispatcher surface rather than keeping a pass-through system.
**Why:** User request — captured for team memory

### 2026-05-08T15:53:09-07:00: Fenster revision — semantic input drain consolidation

**By:** Fenster  
**Requested by:** yashasg  
**Scope:** Input pipeline + tests

**Decision**
- Keep `game_state_system` as the only production semantic dispatcher drain for `GoEvent` and `ButtonPressEvent`.
- Remove HUD-side `disp.update<ButtonPressEvent>()`; HUD now enqueues only.
- Remove `player_input_system` from `tick_playing_systems`; keep handler callbacks wired via dispatcher.
- Shift affected tests to a semantic tick helper (`run_semantic_input_tick`) that models production (`game_state_system` drain).

**Why**
- Eliminates competing authoritative drains and same-frame race assumptions.
- Aligns runtime behavior and tests to one contract, reducing future regressions from stale test plumbing.

**Validation**
- `cmake -B build -S . -Wno-dev`
- `cmake --build build`
- `./build/shapeshifter_tests` (all passing)

# Keaton — EnTT first-pass implementation note

Date: 2026-05-08T14:50:05.765-07:00
Requested by: yashasg

## Decision

Implemented only the safe first-pass EnTT cleanup with clear net reduction:
1. Replaced test-player `planned[]/planned_count` bookkeeping with existential `TestPlayerPlannedTag`.
2. Tightened popup fade loop to structural `view<ScorePopup, PopupDisplay, Color>` and kept a separate expiry-only pass for `ScorePopup` entities lacking render components.

Skipped ctx scratch helper migrations in scoring/particle/popup/despawn for now because this pass did not prove a clear net reduction without broad lifecycle setup churn.

## Validation

- `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh`
- `./build/shapeshifter_tests "[test_player]"`
- `./build/shapeshifter_tests "[popup_display]"`
- `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests`

# Keaton — EnTT Round 2 implementation notes

Date: 2026-05-08T15:09:47.770-07:00

Implemented approved round-2 scope only:

1) `test_player_init` eager ctx access
- `test_player_init` now uses eager `ctx().get<TestPlayerState>()` and `ctx().get<SessionLog>()`.
- `game_loop_init` now eagerly emplaces `TestPlayerState` and `SessionLog`.

2) `input_dispatcher` wiring-state bool removal
- Replaced `InputDispatcherWiringState{ bool wired; }` with `InputDispatcherConnections` owning `entt::scoped_connection` handles.
- `wire_input_dispatcher` is idempotent via owner-pointer guard.
- `unwire_input_dispatcher` now resets owned connections only.

3) `collision_system` eager SongState
- Replaced lazy `find/emplace` with eager `reg.ctx().get<SongState>()`.

4) obstacle lifecycle wiring bool removal
- Replaced obstacle mesh/model lifecycle `wired` bool state with owner + `entt::scoped_connection` ownership in context.
- `wire_*` remains idempotent.
- `unwire_*` resets scoped owners.

Validation performed:
- Targeted tests passed before shared-worktree test-target conflict surfaced:
  - `[ecs][dispatcher]`
  - `[entt_dispatcher]`
  - `[collision]`
  - `[archetype][render][cleanup]`
- Full `shapeshifter_tests` rebuild is currently blocked by unrelated shared-worktree file `tests/test_entt_round2_regression.cpp` (undeclared `spawn_obstacle`), outside this scoped change.
- `cmake --build build --target shapeshifter_lib shapeshifter` passes warning-free.

Notes for reviewer:
- Behavior intentionally unchanged; this is lifecycle bookkeeping cleanup + eager ctx contract tightening.
- No dispatcher/request-queue migration, audio/haptic queue migration, global scratch-init conversion, or gameplay callback migration was performed.

# Keaton — Round 2 EnTT cleanup LOC audit (read-only)

Date: 2026-05-08T15:02:44.235-07:00  
Requested by: yashasg

## Scope audited
- `app/` + `tests/` only, read-only pass.
- Tracks:
  1) manual wired flags / signal lifetime,
  2) custom queues/request structs → `entt::dispatcher`,
  3) ctx lazy-init helpers → eager setup + `ctx().get<T>()`.

## SAFE bucket (high deletion, bounded risk)

### S1) `input_dispatcher` wiring state (`bool wired`) → scoped connections
**Current pattern:** `InputDispatcherWiringState{ bool wired }` with 7 manual connect + 7 manual disconnect calls.  
**Files/functions touched:**
- `app/input/input_dispatcher.cpp` (`wire_input_dispatcher`, `unwire_input_dispatcher`, local wiring state)
- Optional: `app/input/input_routing.h` (if adding small wiring-state type exposure)
- Tests likely touched minimally: `tests/test_components.cpp`, `tests/test_entt_dispatcher_contract.cpp`

**Likely net LOC delta:**
- Product: **-12 to -20 LOC**
- Tests: **0 to +6 LOC**

**Deletes vs moves complexity:** mostly **true deletion** (disconnect boilerplate + wired-flag plumbing).

**Validation/tests required:**
- `test_components` dispatcher idempotence + unwire external-listener behavior
- `test_entt_dispatcher_contract`
- Full suite smoke (dispatcher call-order invariants)

**Risk:** **Low–Medium** (mostly lifecycle correctness + preserving listener order).

---

### S2) `collision_system` SongState lazy-emplace fallback → strict `ctx().get<SongState>()`
**Current pattern:**
- `find<SongState>()`, if missing `emplace<SongState>()` inside hot path.

**Files/functions touched:**
- `app/systems/collision_system.cpp` (`collision_system`)
- Possibly `tests/*` only if any bare-registry collision call appears (not found in this pass)

**Likely net LOC delta:**
- Product: **-3 to -6 LOC**
- Tests: **0 LOC** (expected)

**Deletes vs moves complexity:** **true deletion** (defensive branch removed).

**Validation/tests required:**
- collision-focused suites: `test_collision_system`, `test_collision_extended`, `test_model_authority_gaps`
- full suite regression

**Risk:** **Low** (registry setup already supplies `SongState` in normal/test helpers).

---

### S3) `test_player_init` ctx lazy-emplace of `TestPlayerState`/`SessionLog`/signal-state → eager `get<T>()`
**Current pattern:** `find + emplace` for required session singletons.

**Files/functions touched:**
- `app/session/test_player_session.cpp` (`test_player_init`)
- `app/game_loop.cpp` (ensure all required ctx types exist before call)

**Likely net LOC delta:**
- Product: **-8 to -14 LOC**
- Tests: **0 LOC**

**Deletes vs moves complexity:** mostly **deletion** (removes repeated lazy guards).

**Validation/tests required:**
- `test_test_player_system`
- end-to-end test-player runs (`run_good.sh`, `run_pro.sh` equivalent CI path)

**Risk:** **Low–Medium** (only if some nonstandard call path invokes `test_player_init` without full setup).

## MEDIUM bucket (possible win but coupling/order risk)

### M1) `ScorePopupRequestQueue` → dispatcher event (`ScorePopupRequest` as event)
**Current pattern:**
- Producer: `scoring_system` pushes to `ScorePopupRequestQueue.requests`
- Consumer: `popup_feedback_system` drains vector

**Files/functions touched:**
- `app/components/gameplay_intents.h`
- `app/systems/scoring_system.cpp`
- `app/systems/popup_feedback_system.cpp`
- likely `app/systems/fixed_tick_runner.cpp` (drain/update placement contract)
- tests: `tests/test_phase_runner.cpp`, `tests/test_scoring_system.cpp` (+ any queue-specific assertions)

**Likely net LOC delta:**
- Product: **-6 to -16 LOC**
- Tests: **+8 to +20 LOC**

**Deletes vs moves complexity:** mixed; some deletion, but **ordering complexity moves** into dispatcher drain contract.

**Validation/tests required:**
- `test_phase_runner` (score-feedback chain ordering)
- `test_scoring_system`, `test_scoring_extended`, popup-related suites
- full regression for no dropped popups

**Risk:** **Medium** (event drain timing must remain strictly after scoring writes).

---

### M2) `PendingEnergyEffects` vector/legacy fields → dispatcher energy events
**Current pattern:**
- `scoring_system` writes per-hit/per-miss deferred events
- `energy_system` applies in-order and also supports legacy `delta/flash`

**Files/functions touched:**
- `app/components/gameplay_intents.h`
- `app/systems/scoring_system.cpp`
- `app/systems/energy_system.cpp`
- tests: `tests/test_energy_system.cpp`, `tests/test_phase_runner.cpp`, multiple scoring/collision suites

**Likely net LOC delta:**
- Product: **-4 to +8 LOC**
- Tests: **+12 to +35 LOC** (if legacy compatibility removed)

**Deletes vs moves complexity:** mostly **moves complexity** unless legacy compatibility remains (then little gain).

**Validation/tests required:**
- `test_energy_system` (clamp order + flash semantics)
- scoring/collision integration tests
- full suite for death-cause and energy-transition behavior

**Risk:** **Medium–High** (ordering semantics are gameplay-critical; easy to regress).

---

### M3) Obstacle lifecycle wiring bools → scoped connections (`wire_obstacle_*`)
**Current pattern:** `ObstacleMeshLifetimeState/ObstacleModelLifecycleState` with manual connect/disconnect + wired bool.

**Files/functions touched:**
- `app/entities/obstacle_render_entity.cpp` (`wire/unwire_obstacle_mesh_lifetime`, `wire/unwire_obstacle_model_lifecycle`)
- `app/game_loop.cpp` (shutdown wiring assumptions)
- tests: `tests/test_obstacle_archetypes.cpp`, `tests/test_obstacle_model_slice.cpp`

**Likely net LOC delta:**
- Product: **-8 to -18 LOC**
- Tests: **0 to +8 LOC**

**Deletes vs moves complexity:** moderate deletion, but lifecycle coupling remains.

**Validation/tests required:**
- obstacle archetype + model lifecycle tests
- startup/shutdown lifecycle tests

**Risk:** **Medium** (wrong disconnect timing can leak/double-free mesh/model resources).

## NOT-WORTH bucket (for round 2)

### N1) `AudioQueue`/`HapticQueue` → dispatcher
**Why not worth now:**
- Huge test surface (`test_audio_system`, `test_haptic_system`, many gameplay tests assert queue counts directly).
- These queues encode bounded capacity + explicit frame drain semantics cleanly today.
- Dispatcher migration likely **adds** adapter code to preserve capacity/backpressure.

**Likely net LOC delta:**
- Product: **+10 to +30 LOC**
- Tests: **+20 to +60 LOC**

**Net effect:** mostly complexity shift, not deletion.

**Risk:** **High** (platform-side effects and timing are sensitive).

---

### N2) Global eager-only replacement of all scratch lazy-init helpers
Targets observed: `ScoringSystemScratch`, `PopupDisplayScratch`, `ObstacleDespawnScratch`, `ParticleSystemScratch`.

**Why not worth now:**
- Runtime can eagerly seed these, but several tests use bare registries (not helper-built), especially popup tests.
- Full eager-only conversion forces many test fixtures to seed scratch ctx manually.

**Likely net LOC delta:**
- Product: **-20 to -35 LOC**
- Tests: **+20 to +45 LOC**

**Net effect:** system code shrinks, but test burden rises enough to erase practical win this round.

**Risk:** **Medium** (mostly fixture churn risk, low runtime risk).

## Recommended round-2 cut line
If optimizing for **real net deletion with low risk**, do:
1. **S1 input_dispatcher scoped-connection pass**
2. **S2 collision SongState eager-get cleanup**
3. **S3 test_player_init eager-get cleanup**

Expected combined LOC:
- Product: roughly **-23 to -40 LOC**
- Tests: roughly **0 to +12 LOC**

This is the cleanest “deletes more than it moves” slice from the audited tracks.

# Keaton Decision — Input Dispatch Collapse

- **Date:** 2026-05-08T15:52:28.945-07:00
- **Scope:** Remove raw InputEvent/InputType tier and gesture routing listener.

## Decision

Collapse input dispatch to a single semantic tier:

1. `input_system` now emits `GoEvent` directly for swipe gestures (keyboard path already emitted semantic events).
2. `game_state_system` remains the sole authoritative dispatcher drain for `GoEvent` and `ButtonPressEvent`.
3. `player_input_system` no longer drains dispatcher queues; it is retained as a no-op hook while `player_input_handle_go` and `player_input_handle_press` remain registered listeners.
4. Removed `InputEvent`/`InputType` and deleted `app/input/gesture_routing.cpp`.

## Rationale

This removes redundant pre-drain plumbing while preserving listener registration order and same-tick gameplay behavior. Event ownership is now explicit: producers enqueue semantics, and one system drains.

# Keaton: Redfoot UI segfault fix (2026-05-08)

## Issue
`tests/test_redfoot_testflight_ui.cpp` case `redfoot/#168: collision flags MissedABeat for a missed shape gate` segfaulted.

## Root cause
`collision_system` now unconditionally reads `SongState` from registry context (`reg.ctx().get<SongState>()`).
The test fixture created `GameState/EnergyState/ScoreState/SongResults/GameOverState` but omitted `SongState`, so the context lookup faulted during collision processing.

## Fix
Add `reg.ctx().emplace<SongState>();` to the failing test setup.

## Validation
- `./build/shapeshifter_tests "[ui][redfoot]"` ✅
- `./build/shapeshifter_tests "[ui]"` ✅
- `./build/shapeshifter_tests "[hud]"` ✅
- `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests` ✅ (2051 assertions, 756 cases)

# Keyser — EnTT round 2 architecture audit (read-only)

Date: 2026-05-08T15:02:44.235-07:00  
Requested by: yashasg

## Scope reviewed

- Wiring/lifecycle: `app/input/input_dispatcher.cpp`, `app/entities/obstacle_render_entity.cpp`, `app/session/test_player_session.cpp`, `app/game_loop.cpp`
- Queue/event flow: `app/systems/scoring_system.cpp`, `app/systems/popup_feedback_system.cpp`, `app/systems/energy_system.cpp`, `app/components/gameplay_intents.h`, `app/systems/fixed_tick_runner.cpp`
- Ctx setup/contracts: `app/game_loop.cpp`, `tests/test_helpers.h`, `tests/test_components.cpp`, plus scratch users in `popup_display_system.cpp`, `particle_system.cpp`, `obstacle_despawn_system.cpp`, `scoring_system.cpp`
- Contract tests: `tests/test_entt_dispatcher_contract.cpp`, `tests/test_phase_runner.cpp`, `tests/test_energy_system.cpp`, `tests/test_event_queue.cpp`, `tests/test_obstacle_archetypes.cpp`

## Ranked recommendation (best fit → worst fit)

### 1) Ctx setup cleanup (eager ctx + `ctx().get<T>()` contracts)
**Fit:** High  
**Why:** Best chance to reduce repeated lazy `find/emplace` boilerplate without changing gameplay semantics or system ordering. Aligns with existing `make_registry` singleton-contract tests and strict phase-order model.

**Likely touched files:**
- `app/game_loop.cpp`
- `tests/test_helpers.h`
- `tests/test_components.cpp`
- `app/systems/scoring_system.cpp`
- `app/systems/popup_display_system.cpp`
- `app/systems/particle_system.cpp`
- `app/systems/obstacle_despawn_system.cpp`

**Design risks:**
- Bare-registry tests that currently rely on lazy setup may start throwing.
- Need a clear boundary: only eager-init scratch/resources that are true runtime contracts.

**Approval needed before implementation:** **No extra approval** (safe/mechanical), if limited to net code reduction + contract-test updates.

---

### 2) Signal/connection cleanup (replace manual `wired` booleans where appropriate)
**Fit:** Medium  
**Why:** There is real cleanup potential (`InputDispatcherWiringState`, obstacle lifetime states, test-player session signal state), but risk is lifecycle regressions at shutdown and in test harnesses.

**Likely touched files:**
- `app/input/input_dispatcher.cpp`
- `app/entities/obstacle_render_entity.cpp`
- `app/session/test_player_session.cpp`
- `app/game_loop.cpp`
- `tests/test_components.cpp`
- `tests/test_obstacle_archetypes.cpp`

**Design risks:**
- Must preserve explicit teardown behavior and idempotent wiring.
- Must not break “external listener preserved” contract in `test_components.cpp`.
- No gameplay logic may leak into callbacks.

**Approval needed before implementation:** **Yes (light approval)** due lifecycle/teardown sensitivity.

---

### 3) Dispatcher/event cleanup (replace popup/energy request queues with dispatcher)
**Fit:** Low  
**Why:** Current queue model is explicit and deterministic in fixed-step order (`scoring -> popup_feedback -> energy`) and has dedicated regression coverage. Moving these to dispatcher likely relocates complexity (pool order, update timing, phase leakage hazards) instead of reducing code.

**Likely touched files:**
- `app/components/gameplay_intents.h`
- `app/systems/scoring_system.cpp`
- `app/systems/popup_feedback_system.cpp`
- `app/systems/energy_system.cpp`
- `app/systems/fixed_tick_runner.cpp`
- `tests/test_phase_runner.cpp`
- `tests/test_energy_system.cpp`
- `tests/test_scoring_extended.cpp`
- possibly `tests/test_entt_dispatcher_contract.cpp`

**Design risks:**
- Reintroducing one-tick latency/order hazards already documented in dispatcher contract tests.
- Weakening explicit deterministic system-order semantics.
- Higher coupling between gameplay outcomes and listener registration order.

**Approval needed before implementation:** **Yes (explicit architecture approval required)**; recommend defer unless a measurable reduction plan is presented first.

## Recommendation summary

Proceed with **Track 3 first (ctx setup/contracts)**, then **Track 1 selectively** where connection ownership is unambiguous. **Defer Track 2** for now.

# Kujan re-review — EnTT first-pass cleanup

Timestamp: 2026-05-08T14:50:05.765-07:00 (request context)

Verdict: APPROVED

Scope reviewed:
- app/components/test_player.h
- app/systems/test_player_system.cpp
- app/systems/popup_display_system.cpp
- tests/test_test_player_system.cpp
- tests/test_popup_display_system.cpp

Key checks:
- Verified popup_display structural-view optimization now preserves expiry behavior for partial bundles:
  - ScorePopup + PopupDisplay (no Color)
  - ScorePopup + Color (no PopupDisplay)
  - ScorePopup-only fallback remains
- Confirmed collect-then-destroy pattern is retained.
- Confirmed test coverage includes both new partial-bundle regressions.
- Confirmed test-player planned-state migration uses existential `TestPlayerPlannedTag` and removes stale raw-entity cleanup plumbing as intended in approved safe-first-pass direction.

Validation run:
- `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh`
- `./build/shapeshifter_tests "[popup_display]"`
- `./build/shapeshifter_tests "[test_player]"`
- `./build/shapeshifter_tests`

Result: all commands passed.

# Kujan review — EnTT first-pass cleanup

Date: 2026-05-08
Verdict: REJECTED

## Blocking issue

1. **Popup expiry regression for partial render bundle entities**
   - File: `app/systems/popup_display_system.cpp`
   - Change split processing into:
     - `view<ScorePopup, PopupDisplay, Color>`
     - `view<ScorePopup>(exclude<PopupDisplay, Color>)`
   - This leaves entities with `ScorePopup + PopupDisplay` (missing `Color`) **or** `ScorePopup + Color` (missing `PopupDisplay`) in neither view, so `remaining` is never decremented and they never expire.
   - Prior logic decremented/expired all `ScorePopup` entities regardless of render components.

## Required revision (owner must not be Keaton)

Assign to **Baer**:
- Restore invariant: every `ScorePopup` decrements and expires regardless of render components.
- Keep alpha fade path optimized for full bundle entities.
- Add tests for partial-bundle cases:
  - `ScorePopup + PopupDisplay` (no `Color`) expires.
  - `ScorePopup + Color` (no `PopupDisplay`) expires.
  - Keep existing `max_time <= 0`/clamp behavior coverage intact.

## Commit-scope notes

Stage for revision only:
- `app/systems/popup_display_system.cpp`
- `tests/test_popup_display_system.cpp`
- (if needed) small related test helper adjustments

Do **not** stage unrelated workspace changes (`.squad/agents/*/history.md`, docs, health reports, etc.).

# Kujan Review — EnTT Round 2 + SongState Regression

Date: 2026-05-08  
Verdict: **APPROVED**

## Scope reviewed
- app/session/test_player_session.cpp
- app/game_loop.cpp
- app/input/input_dispatcher.cpp
- app/systems/collision_system.cpp
- app/entities/obstacle_render_entity.cpp
- tests/test_components.cpp
- tests/test_redfoot_testflight_ui.cpp

## Findings
- `collision_system` now uses `reg.ctx().get<SongState>()` (strict contract), which is consistent with runtime initialization and with test helper setup.
- Redfoot regression fix (`reg.ctx().emplace<SongState>()` in the wiring test setup) is correct: it restores required runtime singleton context instead of masking behavior.
- Dispatcher and obstacle lifecycle wiring moved to `entt::scoped_connection` with owner guards and idempotent unwire/rewire behavior; regression coverage was added.
- `test_player_init` eager `ctx().get<>` contract is correctly paired with `game_loop_init` eager singleton initialization.

## Validation run
- `cmake --build build`
- `./build/shapeshifter_tests "[ui][redfoot][game_over][wiring]"`
- `./build/shapeshifter_tests "[ui][redfoot]"`
- `./build/shapeshifter_tests "[ecs][dispatcher]"`
- `./build/shapeshifter_tests "[ecs][obstacle][lifecycle]"`
- `./build/shapeshifter_tests "[collision][song_state][regression]"`
- `./build/shapeshifter_tests` → All tests passed (2051 assertions in 756 test cases)

## Commit scope guidance
Stage only:
- app/session/test_player_session.cpp
- app/game_loop.cpp
- app/input/input_dispatcher.cpp
- app/systems/collision_system.cpp
- app/entities/obstacle_render_entity.cpp
- tests/test_components.cpp
- tests/test_redfoot_testflight_ui.cpp

Exclude as unrelated/generated:
- `.squad/agents/*/history.md`
- `.squad/health-report-*.txt`
- `.squad/scribe-health-report-*.md`
- `.squad/skills/raylib-3d-floor-annulus/`
- `docs/raylib/`
- this review note file unless coordinator wants decision inbox entries committed

# Kujan Review — EnTT Cleanup Round 2

Date: 2026-05-08
Verdict: APPROVED

## Scope reviewed
- app/session/test_player_session.cpp
- app/game_loop.cpp
- app/input/input_dispatcher.cpp
- app/systems/collision_system.cpp
- app/entities/obstacle_render_entity.cpp
- tests/test_components.cpp

## Findings
- Scoped `entt::scoped_connection` adoption in input dispatcher and obstacle lifecycle wiring is correct and idempotent.
- `test_player_init` eager `ctx().get<>` assumptions are satisfied by `game_loop_init` singleton setup.
- `collision_system` eager `SongState` access is deterministic and safe under current registry contract.
- New tests cover rewire/unwire idempotence and SongState singleton stability with targeted, low-bloat additions.
- Full-suite failure remains the known unrelated `redfoot/#168` crash in `test_redfoot_testflight_ui.cpp:114`.

## Validation run
- `cmake --build build --target shapeshifter_tests`
- `./build/shapeshifter_tests "[ecs][dispatcher]"`
- `./build/shapeshifter_tests "[ecs][dispatcher][regression],[ecs][dispatcher][shutdown],[collision][song_state][regression],[ecs][obstacle][lifecycle]"`
- `cmake --build build --target shapeshifter_lib shapeshifter`
- `./build/shapeshifter_tests` (fails only on known unrelated redfoot/#168 segfault)