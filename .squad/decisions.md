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
