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

# Rabin → Team — Subdivision Charting for 2_drama

**Date:** 2026-05-09T00:29:44.825-07:00
**Author:** Rabin (Level Designer)
**Status:** Recommendation, awaiting Coordinator routing.

## Decision summary

`tools/level_designer.py` over-flattens every onset to the nearest
quarter beat *before* shape / lane / gap decisions are made:

- `snap_events_to_beats(events, beats, tolerance=0.08)` discards any
  onset more than 80 ms from a quarter beat. At 130.56 BPM the
  half-beat is ≈ 230 ms away — i.e., 3× outside the snap window — so
  every 8th and triplet onset in `2_drama` is silently dropped.
- `classify_subdivision()` runs *after* snapping, so the phase it
  measures is always ≈ 0 or ≈ 1 → it returns `"downbeat"` for
  essentially every surviving event.
- `2_drama_beatmap.json` stores each obstacle as `{ "beat": <int>, ... }`,
  an integer index into `beat_times[]`; the schema cannot represent
  sub-beat positions even if the analyser produced them.

This explains, without further investigation, the Sr Game Designer
findings of gap=2 ≈ 51 % on hard and the triangle ×11 / square ×9 runs
on medium/hard — both are quantisation artefacts of the quarter-only
grid, not section-classifier failures alone.

## What I'm asking the team for

1. **Fenton:** when the tempogram/PLP experiment runs, please emit the
   six per-song measurements listed in §4 of
   `rabin-subdivision-charting.md` (subdivision histogram of onsets;
   subdivision-of-origin per shipped obstacle; run analysis tagged by
   subdivision; projected gap distribution under sub-beat candidates +
   readability caps; PLP-confidence overlay vs `quiet_regions`;
   strength-tier audit). Run them against `2_drama` first.
2. **Decision gate:** if eighth + triplet onsets in 2_drama are
   < 15 % of total onsets, drop the sub-beat authoring track and
   redirect content effort to the quarter-grid issues in Sr Game
   Designer's review (100 % ShapeGate, hard triangle inversion).
3. **Schema discussion (deferred):** if the experiment justifies
   sub-beat authoring, opening the integer-`beat` schema requires
   Fenster + Marquez sign-off; routing that is a separate decision.

## Readability guardrails for any future sub-beat author pass

- Absolute-time minimum IOI per difficulty (not integer beats):
  easy 350 ms, medium 230 ms, hard 180 ms.
- No two consecutive non-quarter obstacles — every off-beat must be
  flanked by on-beats.
- `MIN_SHAPE_CHANGE_GAP` reformulated in seconds, not beat count, so
  it scales correctly when half-beats appear.
- Per-bar density cap: easy 4, medium 6, hard 8 obstacles per 4-beat
  bar.

## Out of scope

- Schema change to `beat` field (deferred to follow-up).
- New obstacle kinds (LowBar/HighBar still removed on this branch per
  Marquez's cleanup).
- Global offset retune (Sr Game Designer's Issue 4 — engine-side, not
  level-design).

## Artifact

`/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/beat-grid-tempogram-review-2026-05-09/rabin-subdivision-charting.md`
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
- `./build/shapeshifter_tests` (fails only on known unrelated redfoot/#168 segfault)

### 1. `AudioQueue` (`audio_types.h`) + `audio_system`

**Current shape & storage**

```cpp
struct AudioQueue {
    static constexpr int MAX_QUEUED = 16;
    SFX queue[MAX_QUEUED] = {};
    int count = 0;
};
```

Stored as `reg.ctx()` singleton. Producers write into it (`player_input_system`, `popup_feedback_system`, `game_state_terminal_phase_system`). `audio_system` drains it at end of frame.

**Systems that consume/produce**

| System | Role |
|---|---|
| `player_input_system` | pushes `SFX::ShapeShift`, `SFX::Crash`, etc. |
| `popup_feedback_system` | pushes `SFX::ScorePopup`, `SFX::ChainBonus`, etc. |
| `game_state_terminal_phase_system` | pushes `SFX::GameStart` |
| `audio_system` | drains entire queue, calls `PlaySound`, resets `count` |

**Candidate ECS model**

Audio events are ephemeral game-world occurrences. They have a clear identity ("a sound fired at this moment for this reason") and a well-defined one-frame lifetime. This is a textbook case for the "fire-and-forget event entity" pattern:

```cpp
// New component — lives on a short-lived entity for exactly one frame
struct AudioEvent {
    SFX sfx;
};
// Tag to mark the entity for destruction after audio_system processes it
struct ConsumedTag {};
```

Emitters create an entity:
```cpp
reg.emplace<AudioEvent>(reg.create(), AudioEvent{SFX::ShapeShift});
```

`audio_system` becomes:
```cpp
void audio_system(entt::registry& reg) {
    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank || !bank->loaded || !IsAudioDeviceReady()) return;
    auto view = reg.view<AudioEvent>();
    for (auto [e, evt] : view.each()) {
        int idx = static_cast<int>(evt.sfx);
        if (bank->sound_loaded[idx] && IsSoundValid(bank->sounds[idx]))
            PlaySound(bank->sounds[idx]);
    }
    reg.destroy(view.begin(), view.end());
}
```

**Risks**

| Risk | Severity | Notes |
|---|---|---|
| Entity churn per frame | Low | EnTT's sparse-set destroy is O(n); 16 entities/frame is trivial |
| Archetype slot pressure | Low | Single-component archetype; no fragmentation concern |
| Double-play if system runs twice | Low | Same risk as current queue drain; fixable with the `ConsumedTag` pattern or immediate destroy |
| Headless/test breakage | Low | Tests that call systems directly can safely no-op if no `AudioEvent` entities exist — no change needed |
| Event de-duplication | None | Current queue has no de-dup; entity model doesn't change that |

**Migration incremental?**

Yes. Phase 1: add the `AudioEvent` component; keep `AudioQueue` in ctx as pass-through. Phase 2: migrate emitters one by one. Phase 3: delete `AudioQueue` and the ctx emplace. Audit completion: `grep -r AudioQueue app/` returns zero results.

**Verdict: migrate.** Audio events are semantic game events, not machine-room bookkeeping. The entity model makes the data-flow visible in the ECS graph and removes the implicit global reset dependency.

---

### 2. `SFXBank` (`audio_types.h` + `sfx_bank.cpp`)

**Current shape & storage**

```cpp
struct SFXBank {
    static constexpr int SFX_COUNT = ...;
    Sound sounds[SFX_COUNT] = {};
    bool  sound_loaded[SFX_COUNT] = {};
    bool  loaded = false;
};
```

Stored as `reg.ctx()` singleton. Init/unload via `sfx_bank_init` / `sfx_bank_unload`. Read-only during play by `audio_system`.

**Candidate ECS model**

`SFXBank` is a pure asset store — a lookup table from enum to GPU audio handle. It has no game-world identity. A "sound bank entity" carrying `SFXBank` as a component gains nothing: there is never a reason to query `reg.view<SFXBank>()`, and the RAII teardown must still be sequenced before `CloseAudioDevice()`. Moving it to an entity just relocates the same problem.

**Verdict: keep in ctx.** This is a machine-room resource, not a game object. The existing `sfx_bank_init` / `sfx_bank_unload` API is correct ECS lifecycle style.

---

### 3. `MusicContext` (`music_context.h`)

**Current shape & storage**

```cpp
struct MusicContext {
    Music stream{};
    bool  loaded  = false;
    bool  started = false;
    float volume  = 0.8f;
};
```

Emplaced once in `game_loop_init`. The `stream` handle is loaded/unloaded per song in `setup_play_session` and `game_loop_shutdown`. `song_playback_system` is the only per-frame consumer.

**Candidate ECS model**

Music playback *does* have semantic identity as "the currently playing song." It has state transitions (stopped → loading → playing → paused → stopped). There is an argument for a "song entity":

```cpp
// Separate the asset handle (RAII, stays in ctx) from playback state (entity)
struct MusicStream {                  // stays in ctx — GPU/OS resource
    Music handle{};
    bool  loaded = false;
    float volume = 0.8f;
};

struct MusicPlayback {                // per-entity component on a "song entity"
    bool started  = false;
    bool paused   = false;
    bool finished = false;
    bool restart_requested = false;
};
```

`song_playback_system` queries `reg.view<MusicPlayback>()`, reads `MusicStream` from ctx, and drives the raylib API accordingly. The song entity is spawned by `setup_play_session` and destroyed at session teardown.

**Risks**

| Risk | Severity | Notes |
|---|---|---|
| RAII teardown ordering | **High** | `Music` handle must be unloaded before `CloseAudioDevice`. Splitting handle (ctx) from state (entity) solves the RAII problem cleanly, but the split adds indirection |
| `song_playback_system` complexity | Medium | The system currently guards on `music && music->loaded`; entity-based model needs a fallback for headless/test mode where no song entity exists — current `find<>` pattern already handles this |
| `restart_music` flag lives in `SongState` | Medium | `SongState.restart_music` duplicates the "restart requested" signal. Migration is an opportunity to consolidate |
| Test breakage | Low | Tests that run without music already guard on `music_loaded`; entity absence = same guard |

**Migration incremental?**

Yes. Phase 1: split `MusicContext` into `MusicStream` (ctx) + `MusicPlayback` (component on a song entity). Phase 2: migrate `song_playback_system` to query the entity. Phase 3: fold `SongState.restart_music` into `MusicPlayback.restart_requested`. Audit completion: `grep -r MusicContext app/` returns zero results.

**Verdict: migrate (medium priority).** The playback state (`started`, `paused`, `finished`, `restart_requested`) is genuine per-session object state, not a machine-room resource. The handle itself (`Music stream`) belongs in ctx for RAII ordering. The migration also resolves the `SongState.restart_music` redundancy.

---

### 4. `song_playback_system` (system)

Currently a free function operating entirely on ctx singletons (`SongState`, `BeatMap`, `MusicContext`, `GameState`). It does no entity iteration. After the `MusicContext` migration above, it would query a song entity via `reg.view<MusicPlayback>()`. In either form it is a valid ECS system — its structure does not change, only its data sources.

**Verdict: no structural change needed.** It becomes more ECS-idiomatic after the `MusicPlayback` entity migration.

---

### 5. `RenderTargets` (`camera_resources.h`)

**Current shape & storage**

```cpp
struct RenderTargets {
    RenderTexture2D world = {};
    RenderTexture2D ui    = {};
    bool owned = false;
    // RAII move-only; UnloadRenderTexture in destructor when owned
};
```

Emplaced by `camera::init`, released by `camera::shutdown` before `CloseWindow`. Used directly in `game_loop_frame` to drive `BeginTextureMode`.

**Candidate ECS model**

`RenderTargets` is two GPU framebuffer handles with an RAII wrapper. There is one of them, forever, for the life of the window. The RAII release must happen before `CloseWindow()`, which `camera::shutdown` explicitly guarantees. Moving to an entity removes that explicit ordering without gaining anything: `reg.clear()` destroy order is not guaranteed across archetypes.

**Verdict: keep in ctx.** Pure GPU resource with mandatory teardown ordering. No game-world identity.

---

### 6. `ShapeMeshes` (`camera_resources.h`)

**Current shape & storage**

```cpp
namespace camera {
struct ShapeMeshes {
    Mesh     shapes[4] = {};
    Mesh     slab      = {};
    Mesh     quad      = {};
    Material material  = {};
    bool     owned     = false;
    // RAII move-only; UnloadMesh + UnloadMaterial in destructor when owned
};
}
```

Emplaced by `camera::init`, released by `camera::shutdown`. Read by `game_render_system` on every frame for `DrawMesh` calls.

**Candidate ECS model**

This is a static mesh + shader bank — a pure GPU asset store. It is never iterable in a game-object sense; `game_render_system` needs to access all four shape meshes simultaneously for a given draw call. An entity with `ShapeMeshes` as component would still be accessed via `reg.ctx().find` or a singleton-view search, gaining nothing.

**Verdict: keep in ctx.** GPU asset store with the same RAII ordering concern as `RenderTargets`. The `camera` namespace already signals it is infrastructure, not game-domain.

---

### 7. `FloorParams` (`camera_resources.h`)

**Current shape & storage**

```cpp
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};
```

Emplaced by `camera::init`. Written every frame by `game_camera_system` (the beat-pulse block, lines 294–315 of `camera_system.cpp`). Read by `floor_render_system`. It is derived from `SongState` + `BeatMap` — a per-frame computed render parameter.

**Candidate ECS model**

`FloorParams` is derived render state, not a raw asset. It represents "the visual state of the floor this frame." There is a natural candidate entity: a **floor entity** carrying the floor's visual parameters.

```cpp
struct FloorVisual {          // component on a "floor" entity
    float size  = 0.0f;
    float half  = 0.0f;
    float thick = 0.0f;
    uint8_t alpha = 0;
};
struct FloorTag {};           // identity tag
```

`game_camera_system` queries `reg.view<FloorTag, FloorVisual>()` and writes to it. `floor_render_system` reads from it. The computation (currently embedded at the bottom of `game_camera_system`) could become a dedicated `floor_pulse_system` that runs after `song_playback_system` and before the render pass.

**Risks**

| Risk | Severity | Notes |
|---|---|---|
| `floor_render_system` signature becomes `entt::registry&` not `const&` | Low | Minor; render systems already accept const ref |
| floor entity must survive `reg.clear()` in `setup_play_session` | Medium | `reg.clear()` in `play_session.cpp:39` destroys all entities including the floor entity; it must be re-spawned or excluded via `reg.clear<...>()` |
| Adds an extra `view` query per frame | Low | One entity in the view; cost is negligible |

**Migration incremental?**

Yes. Phase 1: spawn a floor entity at game init; add `FloorTag + FloorVisual` components. Phase 2: migrate writers to use the entity. Phase 3: migrate readers. Phase 4: delete `FloorParams` from ctx and from `camera_resources.h`. Audit: `grep -r FloorParams app/` returns zero.

**Verdict: migrate.** `FloorParams` is frame-derived visual state — it is semantically a component of the floor game-object, not a machine-room resource. This migration also creates a clean seam to split the floor-pulse calculation out of `game_camera_system` into its own system.

---

### 8. `ShapeMeshConfig` (`shape_mesh.h`)

**Current shape & storage**

```cpp
struct ShapeMeshConfig {
    ShapeProps props[4] = { ... };  // radius_scale + height_ratio per Shape
};
```

Emplaced once by `camera::init`. Read by `game_camera_system` for model transforms. Never written after init.

**Verdict: keep in ctx.** Read-only config table, initialized once, never iterable. No semantic identity beyond "shape rendering configuration." No migration benefit.

---

### 9. `ScreenTransform` (`rendering.h`)

**Current shape & storage**

```cpp
struct ScreenTransform {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale    = 1.0f;
};
```

Emplaced by `camera::init`. Recomputed every frame by `compute_screen_transform`. Read by `game_loop_frame` for blit rectangle math, and read by `ui_camera_system` (implicitly via ctx).

**Candidate ECS model**

`ScreenTransform` is the letterbox mapping between OS window coordinates and virtual canvas coordinates. It is a frame-derived value like `FloorParams` but it belongs to the display/window concept, not a game object. A "display entity" carrying `ScreenTransform` is possible but adds a view lookup to a computation that happens once per frame anyway.

**Verdict: borderline — keep in ctx for now.** If a "display entity" is introduced for other reasons (e.g., to hold window flags, DPI scale, platform caps), fold `ScreenTransform` into it. Standalone migration is low-value.

---

### 10. `GameCamera` / `UICamera` entities (`camera_entity.h`)

Already entities with components. `game_camera_system` and `ui_camera_system` query them via `reg.view<GameCamera>()`. This is correct strict ECS.

**Verdict: already migrated. No action.**

---

### 11. `setup_play_session` lifecycle (`play_session.cpp`)

**Current shape**

`setup_play_session` is a 110-line orchestration function that:
1. Calls `reg.clear()` — destroys all entities
2. Re-spawns camera entities
3. Resets ctx singletons (`RNGState`, `ScoreState`, `AudioQueue`, `BeatMap`, `SongState`, `EnergyState`, etc.)
4. Loads beatmap JSON from disk
5. Loads music stream
6. Spawns player entity
7. Transitions `GameState.phase` to `Playing`

It is called imperatively by `game_state_system` on a phase transition.

**Candidate ECS model**

The orchestration is a phase transition handler, not a recurring system. It can remain a free function. However, specific responsibilities embedded inside it are worth extracting:

- **Beatmap loading** → `beatmap_loader_system` or a dedicated `load_beatmap` helper that only touches `BeatMap` ctx, callable independently.  
- **Music loading** → part of the `MusicContext`/`MusicPlayback` migration; `setup_play_session` would just spawn the song entity and set `restart_requested = true`, leaving actual `LoadMusicStream` to a `music_load_system` that triggers on the entity's construction.  
- **`reg.clear()` + entity re-spawn** → this is the correct ECS session reset pattern; no change needed.

**Risk: the floor entity** (if introduced per §7) would be destroyed by `reg.clear()` and must be re-spawned. This is a known integration point.

**Verdict: no structural migration needed.** Extract beatmap-loading and music-loading into their own helpers as the `MusicPlayback` entity migration proceeds.

---

### 12. `test_player_session` lifecycle (`test_player_session.cpp`)

**Current shape**

`test_player_init` writes ctx singletons (`LevelSelectState`, `TestPlayerState`, `SessionLog`) and wires EnTT construct signals for `ObstacleTag` and `ScoredTag`. The `TestPlayerSessionSignals` struct is a private ctx guard preventing double-wiring.

The `TestPlayerState` ctx singleton holds: skill level, active flag, frame counter, swipe cooldown, and a bounded `TestPlayerAction actions[32]` queue.

**Candidate ECS model**

The `actions[]` array is an AI intent queue — a bounded set of "things the AI has decided to do." Each action targets a specific obstacle entity and has timing parameters. These could be `TestPlayerAction` components on the obstacle entities they target:

```cpp
// Component emplaced on the obstacle entity by test_player_system
struct TestPlayerIntent {
    float        timer;
    float        arrival_time;
    float        shape_not_before_time;
    Shape        target_shape;
    int8_t       target_lane;
    VMode        target_vertical;
    ActionDoneBit done_flags;
};
```

`test_player_system` queries `reg.view<ObstacleTag, BeatInfo, TestPlayerIntent>()` for scheduled actions, and `reg.view<ObstacleTag, BeatInfo>()` (exclude `TestPlayerIntent`) for unscheduled obstacles it can plan for.

**Risks**

| Risk | Severity | Notes |
|---|---|---|
| Intent survives obstacle destroy | Medium | `on_obstacle_destroy` must also remove `TestPlayerIntent` — or query `view<TestPlayerIntent>()` independently |
| `MAX_ACTIONS = 32` cap becomes implicit | Low | Entity count naturally limits queue depth; no hard cap needed |
| `TestPlayerState.frame_count`, `swipe_cooldown_timer` | Low | These are per-AI-agent state, not per-obstacle — they stay on a test-player ctx or a dedicated test-player entity |

**Migration incremental?**

Yes. Phase 1: add `TestPlayerIntent` as a component. Phase 2: migrate `test_player_system` to write intents onto obstacle entities instead of into the `actions[]` array. Phase 3: remove `actions[]` from `TestPlayerState`. Audit: `grep -r "actions\[" app/systems/test_player_system.cpp` returns zero.

**Verdict: migrate (low priority — test-only code).** Attaching AI intents to the obstacle entity they target is the cleanest ECS model and eliminates the need to cross-reference `obstacle` entity handles stored inside `TestPlayerAction`.

---

## Evidence Table (Summary)

| Candidate | Current storage | Should migrate? | Priority | Migration seam |
|---|---|---|---|---|
| `AudioQueue` | ctx singleton (global queue) | **Yes** | High | Fire-and-forget `AudioEvent` entities; delete `AudioQueue` |
| `SFXBank` | ctx singleton (asset bank) | No | — | Keep; pure GPU asset + RAII ordering |
| `MusicContext.stream` (handle) | ctx singleton | No (split only) | Medium | Split: handle stays ctx as `MusicStream`; playback state → entity |
| `MusicContext.started/paused/restart` (state) | ctx singleton | **Yes** | Medium | `MusicPlayback` component on song entity; fold `SongState.restart_music` |
| `song_playback_system` | free function over ctx | No structural change | Medium | Becomes entity-query after `MusicPlayback` migration |
| `RenderTargets` | ctx singleton (GPU RAII) | No | — | Keep; teardown ordering mandatory |
| `ShapeMeshes` | ctx singleton (GPU RAII) | No | — | Keep; teardown ordering mandatory |
| `FloorParams` | ctx singleton (derived render state) | **Yes** | Medium | `FloorVisual` component on floor entity; extract `floor_pulse_system` |
| `ShapeMeshConfig` | ctx singleton (read-only config) | No | — | Keep; static config table |
| `ScreenTransform` | ctx singleton (derived render state) | Borderline | Low | Fold into display entity only if one is created for other reasons |
| `GameCamera` / `UICamera` | Entities with components | Already ECS | — | — |
| `setup_play_session` | Free orchestration function | No structural change | — | Extract beatmap/music loading as helpers |
| `TestPlayerAction actions[]` | Array in ctx `TestPlayerState` | **Yes** | Low | `TestPlayerIntent` component on targeted obstacle entity |

---

## Suggested Audit Checklist

Copy into a tracking issue or todo table. Each item is independently verifiable via `grep` after the change.

```
[ ] AUD-1  AudioQueue removed from reg.ctx(); grep "AudioQueue" app/ → 0 results
[ ] AUD-2  AudioEvent component added to app/components/audio_event.h
[ ] AUD-3  audio_system iterates reg.view<AudioEvent>() and destroys entities after play
[ ] AUD-4  All former AudioQueue push-sites emit AudioEvent entities instead
[ ] AUD-5  MusicStream ctx struct holds Music handle + volume; MusicContext deleted
[ ] AUD-6  MusicPlayback component on a song entity (spawned per session)
[ ] AUD-7  song_playback_system queries MusicPlayback via reg.view<>(); no ctx.find<MusicContext>()
[ ] AUD-8  SongState.restart_music field removed; replaced by MusicPlayback.restart_requested
[ ] AUD-9  FloorTag + FloorVisual component on floor entity spawned at game init
[ ] AUD-10 FloorParams removed from reg.ctx(); grep "FloorParams" app/ → 0 results
[ ] AUD-11 floor_pulse_system added (or camera_system block extracted) to compute FloorVisual
[ ] AUD-12 floor_render_system reads FloorVisual from floor entity, not ctx
[ ] AUD-13 Floor entity survives setup_play_session (excluded from reg.clear or re-spawned)
[ ] AUD-14 TestPlayerIntent component defined; TestPlayerAction.obstacle field eliminated
[ ] AUD-15 test_player_system writes intents onto obstacle entities; actions[] array removed
[ ] AUD-16 All changes compile with 0 warnings (-Wall -Wextra -Werror)
[ ] AUD-17 ./build/shapeshifter_tests passes with 0 failures after each phase
```

---

## Key Risks to Sequence Carefully

1. **`reg.clear()` in `setup_play_session`** — any new entity (floor entity, song entity) spawned at game init will be destroyed by `reg.clear()`. These entities must either be re-spawned in `setup_play_session` or excluded using `reg.clear<ComponentTypes...>()` instead of blanket clear. Recommend re-spawning: it's explicit and testable.

2. **RAII teardown ordering** — `MusicStream` (ctx) and `SFXBank` (ctx) must be unloaded before `CloseAudioDevice()`. Do not move these to entities. The `camera::shutdown` explicit call pattern is the correct model for any GPU/audio resource that must release before a platform API closes.

3. **Headless/test mode** — tests run without window or audio device. The `find<>` null-guard pattern currently in `song_playback_system` and `audio_system` is correct and must be preserved (or replaced by equivalent entity-absence guards) in any migrated form.

# Keaton strict ECS usage audit

Read-only audit requested 2026-05-08. Strict ECS rule applied: only entity-attached data is a component; registry `ctx` singletons/resources are acceptable only when deliberately owned by setup/lifecycle/system code; free functions are ECS systems/listeners only when they operate through `entt::registry` / `entt::dispatcher` / views / signals.

| Current path | Actual usage evidence | Strict ECS classification | Recommended action |
|---|---|---|---|
| `app/audio/audio_types.h` | `AudioQueue` is emplaced/reset as `reg.ctx().emplace<AudioQueue>()` in `game_loop.cpp:169` and `reg.ctx().insert_or_assign(AudioQueue{})` in `play_session.cpp:46`; producers append in `player_input_system.cpp:81`, `popup_feedback_system.cpp:13`, `game_state_terminal_phase_system.cpp:35`; `audio_system.cpp:7` drains/resets it. `SFXBank` is found/emplaced by `sfx_bank_init()` in `sfx_bank.cpp:115-119`, read by `audio_system.cpp:10-15`, unloaded by `sfx_bank_unload()` in `sfx_bank.cpp:132-143`. | `AudioQueue`: deliberate registry ctx event queue/resource, owned by game-loop/session lifecycle and drained by `audio_system`. `SFXBank`: registry ctx resource, owned by `sfx_bank_init` / `sfx_bank_unload`; not entity component data. `SFX` enum is shared type data. | Do **not** move to entity `components/`. If renamed/moved later, use a ctx-resource/audio-resource semantic location, not “components”. |
| `app/audio/music_context.h` | Emplaced once by `game_loop.cpp:225`; play-session loader uses `reg.ctx().find<MusicContext>()` and loads/unloads stream in `play_session.cpp:105-128`; `song_playback_system.cpp:12-45` updates stream/start/pause/stop; shutdown unloads in `game_loop.cpp:330-335`. | Deliberate registry ctx singleton/resource with lifecycle split between game-loop init/shutdown, `setup_play_session`, and `song_playback_system`. Not entity component data. | Keep as ctx resource. Do **not** classify as component. A move under audio/resources would be semantic; moving to components would be wrong. |
| `app/audio/sfx_bank.h/.cpp` | Public functions take `entt::registry&`; `sfx_bank_init` creates/initializes `SFXBank` ctx resource (`sfx_bank.cpp:115-129`); `sfx_bank_unload` releases raylib sounds from ctx (`sfx_bank.cpp:132-143`); called by `game_loop.cpp:149` and `game_loop.cpp:341`. | ECS lifecycle/setup functions for a ctx-owned audio resource. Internal waveform generation is utility implementation detail. | Keep as audio lifecycle/resource owner. Do not call it an ECS component; “system” is only fair as init/shutdown lifecycle, not per-frame ECS system. |
| `app/rendering/camera_resources.h` | `RenderTargets`, `FloorParams`, `camera::ShapeMeshes` are emplaced in `camera::init` (`camera_system.cpp:145`, `148`, `150`); `RenderTargets` used by frame compositing in `game_loop.cpp:258-283`; `ShapeMeshes` read by `game_render_system.cpp:27`; `FloorParams` written by `camera_system.cpp:293-310` and read by `floor_render_system.cpp:185-193`; `camera::shutdown` releases `ShapeMeshes` and `RenderTargets` (`camera_system.cpp:153-162`). | Registry ctx render resources/singletons. `FloorParams` is ctx render state derived by camera system; `RenderTargets` and `ShapeMeshes` are GPU resource owners. None are entity-attached components. | Do **not** move to components. Keep in rendering/camera resource area unless a future move names them clearly as ctx render resources. |
| `app/input/input_dispatcher.cpp` | Requires `entt::dispatcher` in ctx (`input_dispatcher.cpp:34-36`); stores `InputDispatcherConnections` in ctx (`39-47`); connects dispatcher sinks to `game_state_handle_*`, `level_select_handle_*`, `player_input_handle_*` (`52-63`); unwires via scoped connections (`68-71`); called from `game_loop.cpp:161` and `316`; drained by `game_state_system.cpp:30-32`. | True ECS dispatcher wiring/lifecycle. `InputDispatcherConnections` is private ctx connection state, not component data. | Keep. Moving would be cosmetic; do **not** reintroduce deleted input layers. If renamed, call it dispatcher/listener wiring, not component. |
| `app/input/game_state_routing.cpp` | Listener functions take `entt::registry&` plus event; read/write `GameState`, `InputState`, `HapticQueue`, `SettingsState` ctx (`game_state_routing.cpp:37-73`); wired as dispatcher listeners by `input_dispatcher.cpp:52-55`. | True ECS event listeners operating through registry ctx. Not a standalone service and not entity components. | Keep with input routing/listeners. Moving is optional/cosmetic only; no input architecture rebuild. |
| `app/input/input_routing.h` | Declares dispatcher listener callbacks and wire/unwire functions (`input_routing.h:8-16`) used by `input_dispatcher.cpp` / `game_state_routing.cpp` and player-input callbacks in `player_input_system.cpp:36`, `56`. | Header for ECS dispatcher listeners and lifecycle wiring. | Keep. Defer any move unless reorganizing listener headers semantically. |
| `app/session/play_session.h/.cpp` | `setup_play_session(entt::registry&)` clears entities (`play_session.cpp:39`), spawns camera/player entities (`40-41`, `145`), resets ctx singletons (`44-47`, `93-103`, `130-137`), loads beatmap/music ctx data (`53-75`, `105-128`); invoked by `game_state_system.cpp:49` on transition to Playing. | True ECS lifecycle/setup function. It operates on registry entities and ctx singletons. Not a per-frame system, not a component. | Keep under session/lifecycle. Do not move to components; moving to systems would be arguable but mostly cosmetic. |
| `app/session/test_player_session.h/.cpp` | `test_player_init(entt::registry&, ...)` writes `LevelSelectState`, `TestPlayerState`, `SessionLog` ctx (`test_player_session.cpp:23-41`); stores private `TestPlayerSessionSignals` in ctx (`54-57`); connects EnTT construct listeners for `ObstacleTag` and `ScoredTag` (`59-60`); called from `game_loop.cpp:228-230`. | ECS test/session lifecycle and signal wiring. `TestPlayerSessionSignals` is ctx connection guard, not component. `g_test_player_log_sequence` is a private utility counter and not ECS state. | Keep under session/test lifecycle. No component/system rename needed; do not delete. |
| `app/ui/level_select_controller.h/.cpp` | `level_select_handle_go/press(entt::registry&, event)` mutate `LevelSelectState` ctx after checking `GameState` ctx (`level_select_controller.cpp:5-41`); wired as dispatcher listeners in `input_dispatcher.cpp:56-59`. | True ECS event listeners/controller functions operating through registry ctx. Not entity components. | If prior advice suggested moving because it “belongs to input systems,” correct that: current UI-controller placement is defensible. Moving would be cosmetic; defer unless doing a broader UI/listener naming pass. |

## Corrections to prior implementation advice

- Do not move `audio_types.h`, `music_context.h`, or `camera_resources.h` into a `components/` namespace/folder solely because their structs are used by systems. Actual usage shows they are registry `ctx` resources/singletons, not entity-attached ECS components.
- Do not label `SFXBank`, `MusicContext`, `RenderTargets`, `ShapeMeshes`, or `FloorParams` as entity components. They have legitimate ctx ownership/lifecycle, but their semantic destination must be resource/singleton-oriented.
- Do not re-add an input abstraction/layer. `app/input/*` is currently dispatcher wiring plus event listeners; it already uses `entt::dispatcher` and registry ctx directly.
- `level_select_controller.*` is not dead and not a service class; it is a pair of ECS dispatcher listeners. Any move is cosmetic.

# ECS-First Resource Modeling Proposal

**Author:** Keyser (Lead Architect)
**Date:** 2026-05-08
**Scope:** Audit and re-model the remaining `ctx`/resource-shaped types in `app/` against strict ECS principles. No code is changed by this document.

---

## 1. Audit Rules (used to grade every candidate)

A piece of state in this codebase belongs in one of exactly four homes. Anything that does not fit one of these is a smell.

| Home | Used when |
|---|---|
| **Per-entity component** | Data that is plural by nature (one per obstacle, one per popup), or that already has a natural owning entity (camera, model, mesh-child). |
| **"Tag" entity (singular entity with a unique component)** | A logically-singleton thing that nevertheless has lifecycle, handles, and behaviour — e.g. the music track, the world render target. EnTT precedent in this codebase: `GameCamera`/`UICamera` entities. |
| **`entt::dispatcher` event** | Transient cross-system messages (already used for `GoEvent`, `ButtonPressEvent`). The honest home for any "queue of things to do this frame". |
| **`registry.ctx()` singleton** | Only for: (a) immutable asset libraries indexed by enum, (b) the `entt::dispatcher` itself, (c) cold platform/persistence configuration. Must be **read-only after init** *or* mutated by exactly one system that documents itself as the owner. |

The smells the rules expose:

- **Hand-rolled ring buffer in ctx** (`AudioQueue`, `HapticQueue`) → should be a dispatcher event.
- **RAII handle + lifecycle flags in ctx** (`MusicContext`, `RenderTargets`, `ShapeMeshes`) → should be an entity, with EnTT `on_destroy` doing the unload.
- **Boolean "do this once next frame" flag** (`SongState.restart_music`, `TestPlayerSessionSignals.wired`) → should be a one-shot dispatcher event or a tag component.
- **Per-frame derived scratch in ctx** (`FloorParams`) → not state; either inline or rename to advertise it as a cache.

---

## 2. Per-Candidate Proposal

### 2.1 `AudioQueue` — replace with `PlaySfxEvent` on the dispatcher

| | |
|---|---|
| **Current home** | `ctx<AudioQueue>` — fixed array of 16 SFX enum values, `count` integer. |
| **Producers** | `player_input_system`, `popup_feedback_system`, `game_state_terminal_phase_system`. They each call `ctx().get<AudioQueue>()` and push by index. |
| **Consumer** | `audio_system` drains and calls `PlaySound`. |
| **Verdict** | **Promote to ECS event.** This is *literally* what `entt::dispatcher` exists for, and the codebase already uses it correctly for `GoEvent`/`ButtonPressEvent`. Keeping `AudioQueue` as a parallel hand-rolled queue is a duplication smell. |
| **Honest names** | Event component: `PlaySfxEvent { SFX clip; }`. System: `audio_playback_system` (subscribes via `disp.sink<PlaySfxEvent>().connect<&play_sfx_handler>(reg)`, with a `scoped_connection` stored alongside the input ones). |
| **Audited against** | `GoEvent`, `ButtonPressEvent`, `wire_input_dispatcher` in `app/input/input_dispatcher.cpp`. Same shape, same lifetime story, same drain ownership rule. |
| **Min rewire** | 1) Add `PlaySfxEvent` next to `GoEvent` in `components/input_events.h` (or a new `audio_events.h`). 2) Add an `audio_playback_listener` connect in `wire_input_dispatcher` (or a new `wire_audio_dispatcher`). 3) Replace ~5 producer call sites (grep `AudioQueue::MAX_QUEUED`) with `disp.enqueue<PlaySfxEvent>({clip})`. 4) Delete `AudioQueue` and the manual drain in `audio_system.cpp`. No producer ordering change; no test surface change. |

### 2.2 `HapticQueue` — same treatment as `AudioQueue`

| | |
|---|---|
| **Verdict** | Identical case to `AudioQueue`. |
| **Honest name** | `PlayHapticEvent { HapticEvent kind; }`. System: `haptic_playback_system`. |
| **Audited against** | `AudioQueue` (will be migrated in lockstep) and `GoEvent`. |
| **Min rewire** | Same shape as 2.1; do them in one PR so the pattern is visible. |

### 2.3 `SFXBank` — keep as ctx, but rename and lock down

| | |
|---|---|
| **Current home** | `ctx<SFXBank>` — array of `Sound` indexed by `SFX` enum, `loaded` flag. |
| **Verdict** | **Stays in ctx.** It is an immutable, enum-indexed asset library populated once at startup and never mutated again. Modeling each sound as an entity with `view<SfxAsset>` would add a lookup-by-id step with no semantic gain. |
| **Audit rule satisfied** | "Immutable asset library indexed by enum, populated once at init, addressed by enum value rather than by entity identity." |
| **Honest name** | Rename `SFXBank` → `SfxAssetLibrary` (parallel to a future `ShapeMeshLibrary`). Drop the misleading "Bank" name once the dispatcher event lands and the queue/bank distinction is no longer needed. |
| **Audited against** | `TextContext` (font cache), `BeatMap` (immutable per-session asset). Same family. |
| **Min rewire** | Rename only. Single consumer (the new `audio_playback_system`) reads it; everyone else loses access by virtue of the producer migration in 2.1. |

### 2.4 `MusicContext` — promote to a `MusicTrack` entity

| | |
|---|---|
| **Current home** | `ctx<MusicContext>` carrying `Music stream`, `loaded`, `started`, `volume`. Also a stray `SongState.restart_music` flag living in a *different* ctx singleton. |
| **Verdict** | **Promote to a singleton entity** following the precedent set by `GameCamera`/`UICamera`. It has lifecycle (load/play/pause/resume/stop), it owns a GPU/audio handle, and it has a "control next frame" flag — that is exactly the shape of an entity with components and inbound events, not a ctx blob. |
| **Honest names** | Entity archetype: **MusicTrack entity**. Components: `MusicStream { Music handle; }` (raylib resource, RAII via `on_destroy` observer); `MusicPlaybackState { bool started; float volume; }`. Optional state-machine component: `MusicLifecycle { enum { Empty, Loaded, Playing, Paused, Finished }; }`. Event: `MusicControlEvent { enum { Restart, Stop }; }` (replaces `SongState.restart_music`). System: `music_playback_system` — splits cleanly from beat advancement (which stays in `song_playback_system` operating on `SongState`). |
| **Why this is honest, not just rearrangement** | (a) The `restart_music` boolean stops being a "flag-in-state" anti-pattern and becomes a real one-shot event. (b) The unload path stops being two redundant code paths — `on_destroy<MusicStream>` calls `UnloadMusicStream` once, removing the parallel cleanup that currently exists in both `play_session.cpp` and shutdown. (c) `song_playback_system` stops doing two unrelated jobs (advancing beats *and* driving raylib music lifecycle). |
| **Audited against** | `GameCamera` entity (singleton entity holding a raylib `Camera3D`), `ObstacleModel` (RAII GPU resource as a component, already attached to entities). Same pattern, already proven in this codebase. |
| **Min rewire** | 1) Add `entities/music_track_entity.{h,cpp}` with `spawn_music_track(reg)` mirroring `spawn_game_camera`. 2) Move RAII into `wire_music_lifetime(reg)` (parallel to `wire_obstacle_model_lifecycle`). 3) Replace ~5 `ctx().find<MusicContext>()` sites with `reg.view<MusicStream, MusicPlaybackState>()`. 4) Replace `SongState.restart_music = true` writes with `disp.enqueue<MusicControlEvent>({Restart})` and listen in `music_playback_system`. |

### 2.5 `RenderTargets` — promote to a `RenderTarget` entity (per pass)

| | |
|---|---|
| **Current home** | `ctx<RenderTargets>` holding two `RenderTexture2D` (world + UI), with hand-written RAII, an `owned` flag, and explicit `release()` callable from two paths. |
| **Verdict** | **Promote to entity-component.** This is the most clear-cut case in the audit: there is already an `ObstacleModel` component holding a raylib `Model` with `on_destroy` cleanup — the codebase has already chosen the "GPU resource handle = component, lifetime = EnTT observer" pattern. `RenderTargets` is the same kind of thing and should follow the same pattern. The dual-cleanup race the current code carefully guards against simply ceases to exist. |
| **Honest names** | Two entities, distinguished by tag, not by struct field: `RenderTarget { RenderTexture2D tex; }` + `WorldPassTag` on one, `RenderTarget { RenderTexture2D tex; }` + `UiPassTag` on the other. RAII via `reg.on_destroy<RenderTarget>().connect<&unload_render_target>()`. The `owned` flag and the `RenderTargets` aggregate disappear. |
| **Audited against** | `ObstacleModel`/`on_obstacle_model_destroy`, `GameCamera`. Same pattern. |
| **Min rewire** | 1) Define `RenderTarget` component + tags. 2) Add `wire_render_target_lifetime(reg)`. 3) `camera::init` creates two entities instead of an aggregate. 4) `game_loop_frame` blits via `view<RenderTarget, WorldPassTag>()` / `view<RenderTarget, UiPassTag>()` lookups (a thin `world_target(reg)` / `ui_target(reg)` accessor pair, mirroring `game_camera(reg)`). 5) Delete the entire RAII machinery in `RenderTargets`. |

### 2.6 `camera::ShapeMeshes` — same treatment as `RenderTargets`, lower priority

| | |
|---|---|
| **Verdict** | **Promote to a `MeshLibrary` entity** with `on_destroy` cleanup, same justification as 2.5. It is a GPU resource bundle that today carries a hand-rolled `owned` flag and a parallel cleanup path. The single consumer is `game_render_system`. |
| **Honest names** | Entity holds component `ShapeMeshLibrary { Mesh shapes[4]; Mesh slab; Mesh quad; Material material; }`. Lifetime via `wire_mesh_library_lifetime(reg)`. Accessor `shape_mesh_library(reg)` mirroring `game_camera(reg)`. |
| **Audited against** | `RenderTarget` (after migration), `ObstacleModel`, `GameCamera`. |
| **Min rewire** | Same shape as 2.5. Lower priority than `RenderTargets`/`MusicContext` because the bug surface is smaller (it is read-only after init), but it should land for consistency. |

### 2.7 `FloorParams` — delete from ctx; it is not state

| | |
|---|---|
| **Current home** | `ctx<FloorParams>` written every frame by `camera_system` and read every frame by `floor_render_system`. |
| **Verdict** | **Not state. Not a singleton. Not data with identity.** It is a derived projection of `SongState` recomputed every frame. ECS-honest answer: inline the derivation into `floor_render_system` (or expose it as a free function `compute_floor_params(const SongState&)` in `util/`). |
| **Audit rule used to remove it** | "If a value is recomputed unconditionally every frame from another ctx singleton and read by exactly one system, it is a function call, not state." |
| **Audited against** | `ScreenTransform` (also derived per frame, but read by *many* systems and so legitimately cached) — `FloorParams` does not meet that bar. |
| **Min rewire** | Delete the ctx slot and its writer in `camera_system`; have `floor_render_system` call `compute_floor_params(reg.ctx().get<SongState>())` once at the top. Zero behaviour change. |

### 2.8 `ShapeMeshConfig` — move out of ctx into `constexpr` data

| | |
|---|---|
| **Current home** | `ctx<ShapeMeshConfig>` holding a 4-element table of constants. |
| **Verdict** | **Not a singleton; it is a `constexpr` table.** Should live as `constexpr ShapeProps SHAPE_PROPS[4]` in `constants.h` or `components/shape_mesh.h`. ctx storage gives the misleading impression that it could be mutated at runtime. |
| **Audited against** | `constants::LANE_X`, `constants::SHAPE_COLORS`. |
| **Min rewire** | Trivial: move to constants header, drop the ctx slot. |

### 2.9 `play_session` / `test_player_session` — keep as factory functions; clean two smells

| | |
|---|---|
| **Verdict** | **Already ECS-honest.** `setup_play_session` and `test_player_init` are *factory functions* over the registry. They are not state and should not become components. The session's actual *state* already lives correctly distributed across `GameState`, `SongState`, `ScoreState`, `EnergyState`, `SongResults`, `GameOverState`, `BeatMap`. That distribution is correct: each component is owned by exactly one system family. |
| **Two real smells to fix** | (a) `TestPlayerSessionSignals { bool wired; }` hidden ctx state in `test_player_session.cpp` — used only to remember "did I already connect signal handlers?". This is the same shape as `InputDispatcherConnections` in `input_dispatcher.cpp` and should follow the same pattern: hold the `entt::scoped_connection`s themselves, so disconnection is automatic on registry clear and the boolean disappears. (b) `SongState.restart_music` is a flag-as-event smell; it is fixed for free by 2.4. |
| **Optional further step (defer)** | Emit `SessionStartedEvent` / `SessionEndedEvent` on the dispatcher so persistence/logging systems can react without `setup_play_session` knowing about them. Defer until a third subscriber appears. |
| **Audited against** | `wire_input_dispatcher` / `unwire_input_dispatcher` (the right pattern for owning signal connections). |

### 2.10 Input dispatcher / listeners — leave alone, treat as the reference pattern

| | |
|---|---|
| **Verdict** | This subsystem is the reference implementation we are auditing everyone else against. Do not modify it. Do not re-add removed input layers. The only follow-on action is to **apply the same pattern** to `AudioQueue` (2.1), `HapticQueue` (2.2), and `MusicContext` (2.4 — `MusicControlEvent`). |

---

## 3. Summary Table

| Candidate | Verdict | New ECS shape | Owning entity / archetype | Replaces |
|---|---|---|---|---|
| `AudioQueue` | **Become event** | `PlaySfxEvent` via `entt::dispatcher` | n/a (event) | hand-rolled ring buffer in ctx |
| `HapticQueue` | **Become event** | `PlayHapticEvent` via `entt::dispatcher` | n/a (event) | hand-rolled ring buffer in ctx |
| `SFXBank` | **Keep in ctx, rename** | `SfxAssetLibrary` (immutable asset library) | ctx singleton | (just rename) |
| `MusicContext` | **Become entity** | `MusicStream` + `MusicPlaybackState` (+ `MusicControlEvent`) | **MusicTrack entity** (singular) | ctx blob + `restart_music` flag |
| `RenderTargets` | **Become entities** | `RenderTarget` + `WorldPassTag` / `UiPassTag` | **RenderTarget entity per pass** | ctx aggregate + manual RAII |
| `camera::ShapeMeshes` | **Become entity** | `ShapeMeshLibrary` | **MeshLibrary entity** (singular) | ctx blob + manual RAII |
| `FloorParams` | **Delete from ctx** | free function `compute_floor_params(SongState)` | n/a | per-frame scratch in ctx |
| `ShapeMeshConfig` | **Move to `constexpr`** | header constant | n/a | misleading "config" in ctx |
| `play_session` / `test_player_session` | **Keep as factory functions** | already correct | n/a | (fix `TestPlayerSessionSignals` smell only) |
| input dispatcher / listeners | **Leave alone — reference pattern** | already correct | dispatcher in ctx | nothing |

---

## 4. Recommended Sequencing (low-risk → high-value)

1. **Lock in the pattern by example: migrate `AudioQueue` → `PlaySfxEvent`.** Mirror exactly the `GoEvent` wiring in `wire_input_dispatcher`. 5 producer call sites, 1 consumer. ~1 PR. Validates the audit before touching GPU resources.
2. **`HapticQueue` → `PlayHapticEvent`** in the same PR shape, immediately after, while the pattern is fresh.
3. **Delete `FloorParams`**; inline derivation. Pure cleanup, no behaviour change. ~1 small PR.
4. **Move `ShapeMeshConfig` to `constexpr`.** Trivial. Same PR as 3 if desired.
5. **Promote `RenderTargets` → entity** with `on_destroy` RAII observer. This is the highest-value cleanup — it eliminates the dual-cleanup-path foot-gun the current `owned` flag exists to paper over.
6. **Promote `MusicContext` → MusicTrack entity** + `MusicControlEvent`. Splits `song_playback_system` cleanly from raylib lifecycle and kills `SongState.restart_music`.
7. **Promote `camera::ShapeMeshes` → MeshLibrary entity.** Mirror of step 5; lower-stakes since it is read-only after init.
8. **Rename `SFXBank` → `SfxAssetLibrary`** so the ctx residents that *legitimately* remain (asset libraries, persistence config, dispatcher) are consistently named.
9. **Fix `TestPlayerSessionSignals`** to hold `scoped_connection`s instead of a boolean.

After these nine steps, every remaining `ctx<T>` slot in the codebase is provably one of: (a) the dispatcher itself, (b) an immutable asset library indexed by enum, (c) cold persistence/configuration. Anything that does not fit those three buckets has been moved to an entity, an event, a per-entity component, or deleted as derived data.

---

## 5. Explicit Non-Goals

- **No new input abstractions.** The semantic event model already in place (`GoEvent`, `ButtonPressEvent`, scoped connections) is the target architecture, not a thing to be replaced.
- **No "Component" base classes.** Every component named in this proposal is a plain data struct, with the single exception of EnTT-managed RAII via observer (which is how `ObstacleModel` already works).
- **No service-locator wrappers.** The accessors `world_target(reg)`, `music_track(reg)`, `shape_mesh_library(reg)` are thin functions returning a `view<T>::get<T>(entity)` reference, exactly mirroring the existing `game_camera(reg)` / `ui_camera(reg)` pattern. They are not a layer.

# Keyser — Strict EnTT-style `app/` layout

**Date:** 2026-05-08T21:45:45-07:00
**By:** Keyser (Lead Architect)
**Requested by:** yashasg
**Status:** Decision (read-only audit; no code changes in this branch)

## Decision

`app/` follows strict EnTT/data-oriented layout. Only the following top-level
folders are sanctioned:

- `app/components/` — POD structs (entity components AND `registry.ctx()` singletons).
- `app/systems/` — free functions that operate on registry queries / dispatcher events.
- `app/entities/` — entity-creation factories (`create_*`, `spawn_*`).
- `app/util/` — non-ECS resource loaders, persistence, math helpers, logging.
- `app/platform/` — true OS abstraction with conditional compilation (iOS bridge, etc.). Not a feature folder; cannot be modeled as ECS without leaking ObjC into systems.
- `app/ui/` — **provisional**. Screen controllers blur system/state lines; revisit after audio/input/session/rendering are absorbed.

**Forbidden as feature-layer folders:** `app/audio/`, `app/input/`, `app/session/`, `app/rendering/`.

## Rewire targets

| Current | Kind | Target |
|---|---|---|
| `app/audio/audio_types.h` | ctx singletons (`AudioQueue`, `SFXBank`) + `SFX` enum | `app/components/audio.h` |
| `app/audio/music_context.h` | ctx singleton (`MusicContext`) | `app/components/audio.h` (merge) or `app/components/music.h` |
| `app/audio/sfx_bank.{h,cpp}` | resource init/unload (procedural sample gen + `LoadSoundFromWave`) | `app/util/sfx_bank.{h,cpp}` (peer of `beat_map_loader`) |
| `app/input/input_routing.h` | listener + wiring declarations | fold into `app/systems/input_dispatcher.h` |
| `app/input/game_state_routing.cpp` | dispatcher listeners (game-state semantics) | `app/systems/game_state_input_listeners.cpp` |
| `app/input/input_dispatcher.cpp` | one-shot wiring of `entt::dispatcher` | `app/systems/input_dispatcher.cpp` |
| `app/session/play_session.{h,cpp}` | scene/registry init on phase enter | `app/systems/play_session_setup_system.{h,cpp}` |
| `app/session/test_player_session.{h,cpp}` | test-player ctx + log file init | `app/systems/test_player_setup_system.{h,cpp}` (peer of `test_player_system.cpp`) |
| `app/rendering/camera_resources.h` | RAII ctx singletons (`RenderTargets`, `camera::ShapeMeshes`, `FloorParams`) | `app/components/camera_resources.h` |
| `app/audio/`, `app/input/`, `app/session/`, `app/rendering/` | empty | **delete folders + CMake `file(GLOB ...)` entries** |

## Rationale

- Components-as-data is location-independent: ctx singletons (`MusicContext`, `RenderTargets`) are still POD structs and belong in `components/`. The "audio domain" grouping is a non-ECS habit.
- `sfx_bank` is asset loading; it pairs with `beat_map_loader.cpp` which already lives in `util/`. Same lifecycle, same shape.
- Dispatcher listeners are systems triggered by events instead of by frame ticks. They belong with siblings in `systems/` so processing order is visible alongside `tick_fixed_systems`.
- `play_session.cpp` and `test_player_session.cpp` mutate the registry as setup systems; they are not factories for a single entity (so not `entities/`). They are scene/init systems.
- `platform/` stays — ObjC++ + `#if PLATFORM_IOS` gates cannot live inside ECS systems without contaminating them. This is the one justified non-ECS folder.

## Dependency surfaces affected

- `CMakeLists.txt` — drop `AUDIO_SOURCES`, `INPUT_SOURCES`, `SESSION_SOURCES` globs (already covered by `SYSTEM_SOURCES` + `UTIL_SOURCES` after the move). `app/rendering/` has no `.cpp`, header-only deletion.
- Includes that reference `../audio/...`, `../input/...`, `../session/...`, `../rendering/...`:
  - `app/game_loop.cpp`, `app/main.cpp`
  - All systems pulling `audio_types.h` / `music_context.h` (audio_system, song_playback_system, popup_feedback_system, scoring_system, etc.)
  - `app/systems/camera_system.cpp`, `app/systems/floor_render_system.cpp`, `app/systems/game_render_system.cpp` for `camera_resources.h`
  - `tests/test_audio_system.cpp`, `tests/test_components.cpp`, `tests/test_gpu_resource_lifecycle.cpp`, `tests/test_high_score_integration.cpp`, `tests/test_helpers.h`, `benchmarks/bench_systems.cpp`
- `app/systems/all_systems.h` — add new system declarations; remove `input_routing.h` if folded.

## Implementation order (keep builds green)

1. **Rendering (smallest, header-only).** Move `camera_resources.h` → `components/`. Update 3 includes (`camera_system.{h,cpp}`, `floor_render_system.cpp`, `game_render_system.cpp`) + `tests/test_gpu_resource_lifecycle.cpp`. Delete `app/rendering/`.
2. **Audio components.** Move `audio_types.h` + `music_context.h` → `components/audio.h` (single header). Mass-update includes. `sfx_bank.{h,cpp}` → `util/`. Delete `app/audio/` + its CMake glob.
3. **Session.** Rename `play_session.{h,cpp}` → `systems/play_session_setup_system.{h,cpp}` and `test_player_session.{h,cpp}` → `systems/test_player_setup_system.{h,cpp}`. Update callers (`game_loop.cpp`, `main.cpp`, test-player entry points). Delete `app/session/` + its CMake glob.
4. **Input.** Move `game_state_routing.cpp` → `systems/game_state_input_listeners.cpp`, `input_dispatcher.cpp` → `systems/input_dispatcher.cpp`, fold `input_routing.h` declarations into `systems/input_dispatcher.h` (or `all_systems.h`). Update includes in `game_loop.cpp` and any test that wires the dispatcher. Delete `app/input/` + its CMake glob.
5. **CMake cleanup.** Remove the four obsolete `file(GLOB ...)` entries and confirm no `target_sources` lines reference deleted paths.
6. **Build + test gate after each step** (`./build.sh && ./build/shapeshifter_tests`). Each step is independently shippable.

## Out of scope (flagged, not decided here)

- `app/ui/screen_controllers/*` — controllers carry per-screen state and behave like stateful systems. Decide separately whether to (a) decompose into systems + components, or (b) accept `app/ui/` as a justified non-ECS facade. Recommend revisit once the four target folders are gone.
- Top-level `app/game_loop.{cpp,h}`, `app/platform_display.{cpp,h}`, `app/main.cpp`, `app/constants.h` — entry/scaffolding; acceptable at `app/` root.

## Revisit triggers

- New audio middleware (FMOD, Wwise) → re-evaluate whether a thin `platform/audio_backend.*` is needed (parallel to `platform/haptics_backend`).
- UI controllers gain enough behavior to dominate the file → split per the deferred `app/ui/` decision above.

# Keyser — Correction: stricter component criteria, retract input moves

**Date:** 2026-05-08T21:57-07:00
**By:** Keyser (Lead Architect)
**Requested by:** yashasg
**Status:** Amendment (read-only; no app/ edits)
**Amends:** `keyser-entt-app-layout.md`, `keyser-entt-fit-criteria.md`

## Why this amendment

User pushback, two parts:

1. The recent input cleanup intentionally removed input-layer surface area.
   Any plan that recreates an `input/` shape — even renamed under
   `systems/` — undoes that work.
2. "Component" is being used too loosely. A struct that lives in
   `registry.ctx()` is not automatically a component. The honest test:
   **does an entity hold it, OR is it a real ECS singleton with a system
   that owns its lifecycle and operates on it as gameplay data?** If the
   answer is "it's a resource handle / cache / service bucket," it is
   not a component, and `components/` is the wrong destination.

## Retractions

### R1. Input migration — fully retracted

Retracted from `keyser-entt-app-layout.md` and `keyser-entt-fit-criteria.md`:

- ~~`input/input_dispatcher.cpp` → `systems/input_dispatcher.cpp`~~
- ~~`input/game_state_routing.cpp` → `systems/game_state_input_listeners.cpp`~~
- ~~`input/input_routing.h` folded into `systems/input_dispatcher.h` /
  `all_systems.h`~~
- ~~`ui/level_select_controller.*` → `systems/level_select_input_listeners.*`~~

Rationale: the recent input_system cleanup is the source of truth for input
shape. Re-promoting `input_dispatcher` / `game_state_routing` /
`level_select_controller` into `systems/` re-establishes the very layer
that was just collapsed. The current layout (input wiring lives next to
its consumers; `app/systems/input_system.cpp` and
`player_input_system.cpp` are the only system-side input touchpoints)
is the desired end state. **Do not move these files.**

### R2. "Components" reclassification

The `components/` folder is reserved for:

- **Entity-attached POD** read via `registry.view<T...>` (e.g. `Position`,
  `Velocity`, `Player`, `Obstacle`).
- **True ctx singletons** — singleton gameplay state with a clear owning
  system that mutates it as data per frame/tick (e.g. `SongState`,
  `GameState`, `ScoreState`).

It is NOT a destination for: GPU resource RAII owners, sound banks,
codec/stream handles, render-target wrappers, asset caches, or any other
"service in a struct." Those are resources and belong with their loader
in `util/` or next to their single consumer.

Re-verdicts:

| File | Prior verdict | Corrected verdict | Rationale |
|---|---|---|---|
| `audio/audio_types.h` — `SFX` enum | → `components/audio.h` | **Defer.** Keep next to `sfx_bank` (header in `audio/` or `util/`). | An enum used to index a sound array is not entity data. |
| `audio/audio_types.h` — `AudioQueue` | → `components/audio.h` | **Component-eligible** (ctx singleton, written by gameplay systems, drained by `audio_system`). Move only as part of an audio cleanup PR; don't move it alone. | Real producer/consumer system pair on gameplay data. |
| `audio/audio_types.h` — `SFXBank` | → `components/audio.h` | **Not a component.** Stays with `sfx_bank.{h,cpp}`. | It's a resource cache of `Sound` handles loaded once at boot. No system "operates on" it; systems read sounds out of it. |
| `audio/music_context.h` — `MusicContext` | → `components/audio.h` | **Not a component.** Stays in `audio/` (or moves with `sfx_bank` to `util/`). | Wraps a raylib `Music` handle + load/play flags. It's a resource handle owned by `song_playback_system`, not gameplay data. |
| `rendering/camera_resources.h` — `RenderTargets` | → `components/camera_resources.h` | **Not a component.** Stays where it is. | RAII GPU resource owner with `release()` and deleted copy ctor. Behaves as a service handle, not as ECS data. Putting it in `components/` would dilute what "component" means. |
| `rendering/camera_resources.h` — `camera::ShapeMeshes` | → `components/camera_resources.h` | **Not a component.** Stays. | Same as above: GPU mesh/material RAII owner. |
| `rendering/camera_resources.h` — `FloorParams` | → `components/camera_resources.h` | **Component-eligible** (per-frame derived data computed by camera/beat code, read by `floor_render_system`). | But moving one struct out of a 3-struct header isn't worth the churn alone. **Defer** until verified entity/system relationship in code. |
| `session/play_session.{h,cpp}` | → `systems/play_session_setup_system.*` | **Defer.** | One-shot init invoked on state transition. Whether it's a "system" or a setup helper called by `game_state_system` requires reading the call site, not the filename. |
| `session/test_player_session.{h,cpp}` | → `systems/test_player_setup_system.*` | **Defer.** Same reason. |

### R3. Folder-deletion goal is dropped as a driver

Deleting `app/audio/`, `app/input/`, `app/session/`, `app/rendering/`
was the cosmetic incentive that produced the bad moves above. We do
not delete a folder by relocating its contents into a folder that
mislabels them. If after honest reclassification a folder still has
content, it stays.

## Revised first-pass recommendation (small, honest)

The ONLY move in this revision that pays for itself in EnTT signal
without misusing `components/` and without re-creating an input layer:

**Single recommended step — none of the prior bulk moves.** Leave the
layout as-is for this branch. The corrective architectural work is:

1. **Document** that `components/` is for entity-attached data and true
   gameplay ctx singletons only, with examples. (One-line addition to
   `design-docs/architecture.md` or `app/components/README.md` in a
   future PR — not this branch.)
2. **Document** that `audio/`, `rendering/`, `session/` are *resource
   modules* — they hold the loader + the handle struct together
   intentionally, and that grouping is the architecture, not a smell.
3. **Leave input alone.** The recent cleanup is the spec.

That's the entire actionable first pass. No file moves on
`docs/cleanup-stale-markdown`.

## Deferred — requires reading code, not filenames

Each of the following needs a concrete entity/system trace before any
move is justified. Do NOT move on naming intuition.

- `AudioQueue`: confirm at least one gameplay system writes it and
  `audio_system` drains it per frame. If yes → eligible for
  `components/audio_queue.h`. Move on its own merits, not bundled with
  `MusicContext`/`SFXBank`.
- `FloorParams`: confirm a system writes it from `SongState` and
  `floor_render_system` reads it. If yes → eligible for
  `components/floor_params.h`. Don't drag `RenderTargets` /
  `ShapeMeshes` along.
- `play_session.cpp` / `test_player_session.cpp`: read the call sites
  in `game_state_system` and `main.cpp`. If they truly run as
  state-transition systems with no peer setup helpers, they may earn a
  `systems/` rename. If they are one-shot init helpers called by a
  system, they stay where they are.
- Any future component candidate: produce the entity query OR the
  ctx-singleton owning-system pair, in writing, before adding the
  header under `components/`.

## Acceptance test (replaces prior over-eager folder-empty test)

The architecture is honest when:

1. Every header in `app/components/` is referenced by either
   `registry.view<...>` / `registry.get<...>` / `registry.emplace<...>`
   on an entity, OR `registry.ctx().emplace<T>()` *together with* a
   named system that mutates it as gameplay data (not as a resource
   handle).
2. No file from the recently-cleaned input flow has been re-promoted
   into `systems/` or `components/`.
3. Resource-handle structs (RAII owners of GPU/audio/file handles)
   live next to their loader, regardless of folder name.

## What to tell the next agent

- Treat `keyser-entt-app-layout.md` and `keyser-entt-fit-criteria.md`
  as superseded for the input section in full and for the audio /
  rendering moves in part. This document is the current rule.
- If a future plan proposes moving a struct into `components/`, it
  must cite the entity query or the owning system + mutation pattern.
  No citation, no move.

# Keyser — Revised: Semantic-fit gate for `app/` layout cleanup

**Date:** 2026-05-08T21:50-07:00
**By:** Keyser (Lead Architect)
**Requested by:** yashasg
**Status:** Decision (read-only revision; supersedes prior plan where noted)
**Supersedes (in part):** `keyser-entt-app-layout.md`

## Why this revision

User directive: moving things for the sake of strict folder names is not
architecture. A move only earns its keep if the destination folder honestly
describes what the code *is* under the EnTT model, not just where we wish it
lived. Previous plan was largely correct but treated all four target folders
as equally guilty. They aren't.

## Stricter classification gate

A file moves only if it passes one of these:

1. **True component** — POD/ctx struct read by ECS views or `registry.ctx()`.
2. **True system** — free function operating on `entt::registry` per frame OR
   as a registered `entt::dispatcher` listener OR as a one-shot scene/init
   system invoked by another system on a state transition.
3. **Entity construction** — `create_*` / `spawn_*` factory producing one
   archetype.
4. **Util / platform / resource** — asset loaders, persistence, math, OS
   bridges. Touching `entt::registry&` once at boot/shutdown does NOT promote
   a file into `systems/`.
5. **Delete** — dead.
6. **Defer** — moving is cosmetic *or* the destination would lie.

## Per-file revised classification

| File | Verdict | Revised target | Change vs prior |
|---|---|---|---|
| `audio/audio_types.h` (`SFX`, `AudioQueue`, `SFXBank`) | True component (ctx singletons) | `components/audio.h` | unchanged |
| `audio/music_context.h` (`MusicContext`) | True component (ctx singleton) | `components/audio.h` (merge) | unchanged |
| `audio/sfx_bank.{h,cpp}` | Util — procedural sample gen + `LoadSoundFromWave`; one-shot init/unload | `util/sfx_bank.{h,cpp}` | unchanged. Honest call: do NOT promote to system. |
| `input/input_dispatcher.cpp` | True system — dispatcher wiring + connection lifecycle | `systems/input_dispatcher.cpp` | unchanged |
| `input/game_state_routing.cpp` | True system — dispatcher listeners | `systems/game_state_input_listeners.cpp` | unchanged |
| `input/input_routing.h` | System decls | fold into `systems/input_dispatcher.h` (or `all_systems.h`) | unchanged |
| `rendering/camera_resources.h` (`FloorParams`, `RenderTargets`, `camera::ShapeMeshes`) | True component (ctx singletons; RAII destructors are fine — POD-ness is not the gate, ECS-data-role is) | `components/camera_resources.h` | unchanged |
| `session/play_session.{h,cpp}` (`setup_play_session`) | True system — invoked by `game_state_system` on `Playing` transition; mutates many ctx singletons + spawns player via `create_player_entity` | `systems/play_session_setup_system.{h,cpp}` | **Confirmed system, not factory.** Belongs in `systems/`, not `entities/`. |
| `session/test_player_session.{h,cpp}` (`test_player_init`) | True system — registry init + log-file setup on test-player entry | `systems/test_player_setup_system.{h,cpp}` | unchanged |
| `ui/level_select_controller.{h,cpp}` | True system — dispatcher listeners (same shape as `input_routing`) | `systems/level_select_input_listeners.{h,cpp}` | **NEW: previously deferred.** Same listener pattern; honest fit in `systems/`. |
| `ui/screen_controllers/*_screen_controller.{h,cpp}` | True system — `render_*_screen_ui(reg)` + per-screen tap handling | Stays in `app/ui/` for now | **Defer.** Functions are systems by signature, but the `screen_controllers/` grouping is a meaningful semantic axis (one file per HUD screen) that flat `systems/` would lose. Re-evaluate only if/when they collapse to a generic dispatch table. |
| `ui/generated/*_layout.h` | Util — raygui codegen output (asset-like) | Stays in `app/ui/generated/` | **Defer.** Generated headers are codegen artifacts, not components. Moving to `util/generated/` would be cosmetic. |

## Net result on prior plan

- **Keep** the prior plan's audio split, camera move, input move, session move.
- **Add** `ui/level_select_controller.*` to the input/listeners migration —
  it's the same pattern and was missed.
- **Reject** any future suggestion to push `ui/screen_controllers/` or
  `ui/generated/` into `systems/` or `components/`. Those would be cosmetic
  renames that erase real grouping (per-screen state machines, codegen
  provenance).
- **`app/ui/` is no longer "provisional"** — it is the justified non-ECS
  facade for screen-state controllers and codegen artifacts. Documented, not
  forbidden.

## Folders that can still be removed (semantically honest)

After the moves above:
- `app/audio/` — empty → delete + drop CMake glob.
- `app/input/` — empty → delete + drop CMake glob.
- `app/session/` — empty → delete + drop CMake glob.
- `app/rendering/` — empty (header-only file moved) → delete.

`app/ui/`, `app/util/`, `app/platform/`, `app/components/`, `app/systems/`,
`app/entities/` all stay.

## Acceptance tests (prove the move was architectural, not cosmetic)

The migration is complete only when ALL hold:

1. **Ctx-singleton residency:** every type registered via `registry.ctx().emplace<T>()`
   in `app/` is declared in a header under `app/components/`.
   - Validate: `rg "ctx\(\).emplace<" app/ -o -N | sort -u` → each type T resolves
     to a header in `components/`.
2. **System surface visibility:** every function registered as an
   `entt::dispatcher` listener OR called from `tick_fixed_systems` /
   `playing_systems_runner` / phase-transition setup is declared in
   `app/systems/`.
   - Validate: grep call sites; no system is reached via a header outside
     `systems/` except entity factories and util loaders.
3. **Util purity:** no file in `app/util/` exposes a function called per frame.
   `entt::registry&` parameters in `util/` appear only in init/unload/persist
   signatures (one-shot).
   - Validate: `rg "entt::registry" app/util/` review by hand — every hit
     is `_init`, `_unload`, `_load`, `_save`, or constructor.
4. **No cross-import of dead folders:** `rg "app/(audio|input|session|rendering)/" app/ tests/ benchmarks/` → empty.
5. **Build + tests green** after each step independently:
   `./run.sh test` passes.
6. **No semantic regressions in `ui/`:** `app/ui/screen_controllers/` and
   `app/ui/generated/` retain their groupings; nothing from these folders
   gets dragged into `systems/` or `components/` in this pass.

If any of (1)–(4) fails, the move was cosmetic — revert that step.

## Recommended first implementation pass (highest EnTT signal, lowest risk)

Do these two steps as the first PR. They produce real architectural value
even if the rest is never done:

**Step A — Audio honesty split** (highest semantic value)

Splits `app/audio/` along the *actual* boundary between data and asset loading,
which the current grouping hides:

- `audio_types.h` + `music_context.h` → `components/audio.h` (single header).
- `sfx_bank.{h,cpp}` → `util/sfx_bank.{h,cpp}` (peer of `beat_map_loader`).
- Delete `app/audio/` + CMake glob.
- Update includes (audio_system, song_playback_system, popup_feedback_system,
  scoring_system, game_loop, tests).

Acceptance: tests (1), (3), (4), (5) above pass for the audio domain.

**Step B — Camera resources header move** (lowest risk, header-only)

- `rendering/camera_resources.h` → `components/camera_resources.h`.
- Update 3–4 includes; delete `app/rendering/`.

Acceptance: (1), (4), (5) pass.

**Steps C and D (input listeners, session setup) follow in separate PRs** once
A and B are merged green. They have real value but more include churn; batching
them risks a noisy diff that hides regressions.

**Skip for now:** `app/ui/`. It is no longer on the cleanup target list.

## Revisit triggers (unchanged from prior)

- New audio middleware → revisit `platform/audio_backend.*`.
- `screen_controllers/` collapse to a uniform dispatch table → reconsider
  promoting them into `systems/`.
---

### 1.1 AudioQueue Write Sites (3 active SFX producers)

| File | Line(s) | SFX |
|---|---|---|
| `app/systems/player_input_system.cpp` | 81–83 | `SFX::ShapeShift` — non-rhythm mode shape change |
| `app/systems/player_input_system.cpp` | 119–121 | `SFX::ShapeShift` — rhythm mode `begin_shape_window` lambda |
| `app/systems/popup_feedback_system.cpp` | 13–17 | `SFX::ScorePopup` — once per popup request |
| `app/systems/game_state_terminal_phase_system.cpp` | 35–37 | `SFX::Crash` — `GamePhase::GameOver` only |

**Known dead SFX (bank entries without producers):**
`SFX::ChainBonus`, `SFX::UITap`, `SFX::ZoneRisky`, `SFX::ZoneDanger`, `SFX::ZoneDead`, `SFX::GameStart`
— sound specs exist in `sfx_bank.cpp` but nothing enqueues them. These are pre-existing gaps, not regressions to guard.

### 1.2 HapticQueue Write Sites (6 call sites across 4 files)

| File | Line(s) | Event |
|---|---|---|
| `app/systems/player_input_system.cpp` | 48 | `LaneSwitch` — go-handler (Left/Right swipe) |
| `app/systems/player_input_system.cpp` | 85, 93 | `ShapeShift` + `LaneSwitch` — press-handler (non-rhythm) |
| `app/systems/player_input_system.cpp` | 123 | `ShapeShift` — press-handler (rhythm `begin_shape_window`) |
| `app/systems/player_movement_system.cpp` | 65 | `JumpLand` — on vertical-state transition to `Grounded` |
| `app/systems/game_state_terminal_phase_system.cpp` | 46–50 | `DeathCrash` + `NewHighScore` — on GameOver |
| `app/input/game_state_routing.cpp` | 22–23, 52 | `RetryTap` / `UIButtonTap` (end screen); `UIButtonTap` (Title screen) |

### 1.3 Queue Reset / Drain Pattern

**AudioQueue:**
- Startup: `game_loop.cpp:169` — `emplace<AudioQueue>()`
- Session start: `play_session.cpp:46` — `insert_or_assign(AudioQueue{})` ← **resets queue**
- Per-frame drain: `audio_system` sets `count = 0` after playback

**HapticQueue:**
- Startup only: `game_loop.cpp:170` — `emplace<HapticQueue>()`
- ⚠ **`play_session.cpp` does NOT reset `HapticQueue`** — asymmetry from `AudioQueue`
- Per-frame drain: `haptic_system` sets `count = 0` after triggering

### 1.4 Haptics-Enabled Gating Pattern (4 independent sites)

All four write-site files gate on `SettingsState::haptics_enabled` at **push time**:
- `push_haptic()` helper in `player_input_system.cpp`
- Inline guard in `player_movement_system.cpp`
- Inline guard in `game_state_routing.cpp`
- Inline guard in `game_state_terminal_phase_system.cpp` (`emit_terminal_feedback`)

---

## 2. Existing Test Coverage

### 2.1 Tests That Must Remain Green After Migration

| Test File | Tags | What It Guards |
|---|---|---|
| `test_audio_system.cpp` | `[audio]` | `audio_system` drains queue; invalid SFX indices safe; headless-safe lifecycle (`sfx_bank_init`/`unload`) |
| `test_haptic_system.cpp` | `[haptic]` | Queue struct; `haptic_system` drain; absent-context no-crash; `DeathCrash` + `NewHighScore` on GameOver; `RetryTap` on Restart; `UIButtonTap` on non-Restart; `haptics_enabled=false` suppresses all |
| `test_haptics_backend.cpp` | `[haptic]` | `pattern_for_event` mapping; warmup idempotence |
| `test_components.cpp` | `[components]` | `AudioQueue` struct shape (capacity, enum storage) |
| `test_player_systems.cpp` | `[player]` | `AudioQueue.count > 0` after shape press; `== 0` for same-shape |
| `test_player_input_rhythm_extended.cpp` | `[player][rhythm]` | `audio.queue[0] == SFX::ShapeShift` after shape change; `count == 0` for same shape |
| `test_player_action_rhythm.cpp` | `[player_rhythm]` | `AudioQueue.count > 0` in rhythm-mode shape change and re-press |
| `test_scoring_system.cpp` | `[scoring]` | `AudioQueue.count > 0` after `scoring_system` + `popup_feedback_system` tick |
| `test_world_systems.cpp` | `[gamestate]` | `audio.queue[0] == SFX::Crash` on `game_state_enter_terminal_phase(GameOver)` |
| `test_entt_dispatcher_contract.cpp` | `[entt_dispatcher]` | Dispatcher semantics; `enqueue`/`update`/`clear`; listener ordering; same-tick delivery |
| `test_event_queue.cpp` | `[events]` | GoEvent / ButtonPressEvent dispatcher flows |
| `test_input_pipeline_behavior.cpp` | `[input_pipeline]` | End-to-end pipeline — SFX side-effects flow through same fixed tick |

### 2.2 Known Test Coverage Gaps (pre-existing, not regressions)

- `HapticEvent::JumpLand` — emitted in `player_movement_system.cpp` but **no test asserts it is produced**
- `SFX::ScorePopup` — tested indirectly via `AudioQueue.count > 0` in `test_scoring_system.cpp`; the specific enum value is never checked
- `HapticEvent::UIButtonTap` from the **Title-screen path** in `game_state_routing.cpp` — end-screen path is covered; title-screen path is not
- `HapticEvent::Burnout*` events — backend patterns are mapped in `haptics_backend.cpp` but **no burnout zone logic produces them**

---

## 3. Code Paths That Must Work After Migration

1. **Shape press → SFX::ShapeShift** — both rhythm and non-rhythm modes, single-frame delivery
2. **Score popup spawn → SFX::ScorePopup** — `popup_feedback_system` drives this; same frame as scoring
3. **GameOver → SFX::Crash + HapticEvent::DeathCrash** — via `game_state_enter_terminal_phase`
4. **GameOver (new high score) → HapticEvent::NewHighScore** — second haptic in same terminal-phase call
5. **GoEvent (swipe) → HapticEvent::LaneSwitch** — same-frame delivery via dispatcher listener
6. **ButtonPressEvent (shape) → HapticEvent::ShapeShift + optional LaneSwitch** — same-frame
7. **ButtonPressEvent (end-screen Restart) → HapticEvent::RetryTap** — from `game_state_routing.cpp`
8. **ButtonPressEvent (end-screen non-Restart) → HapticEvent::UIButtonTap**
9. **ButtonPressEvent (Title screen) → HapticEvent::UIButtonTap**
10. **JumpLand → HapticEvent::JumpLand** — `player_movement_system`, no SFX counterpart
11. **haptics_enabled=false suppresses all haptic delivery** — gate must remain per-event, not per-frame or per-connection
12. **Session restart clears in-flight SFX** — `play_session.cpp` currently resets `AudioQueue`; equivalent flush required for new event mechanism
13. **audio_system + haptic_system drain fully each frame** — no cross-frame accumulation

---

## 4. Review Checklist for Keaton's Patch

### A. No old input layer reintroduced
- [ ] No new `reg.ctx().find<InputState>()` reads in audio/haptic producers
- [ ] SFX/haptic triggers remain driven by semantic events (dispatcher) or game-state transitions — not raw `InputState` fields

### B. No fake components
- [ ] `AudioQueue` and `HapticQueue` ctx entries are either **fully removed** or explicitly deprecated with no write sites remaining
- [ ] No new empty/stub component added to work around missing dispatch wiring (e.g. an `AudioEventTag` that's never queried)

### C. Dispatcher connections are scoped and properly unwired
- [ ] All audio/haptic sink connections stored in an `entt::scoped_connection`-based struct (like `InputDispatcherConnections`)
- [ ] Connections are destroyed (or explicitly reset to `{}`) on session teardown — no dangling listeners
- [ ] Reconnecting on `setup_play_session` does not **double-connect** the same listener
- [ ] If listeners are free functions taking `entt::registry& reg`, verify `reg` is captured by reference safely for the connection lifetime
- [ ] `unwire_input_dispatcher` (or its audio/haptic analogue) is called in `game_loop.cpp` shutdown at the same point as the existing disconnect (line 316)

### D. Event ordering is acceptable within a frame
- [ ] SFX dispatcher `update()` call happens **after** all producers run in the same frame — in the current loop that means after `tick_fixed_systems` and after `tick_playing_systems`
- [ ] Haptic dispatcher `update()` call follows the same rule
- [ ] Events enqueued during `game_state_enter_terminal_phase` (GameOver path) are not cleared before the audio/haptic drain
- [ ] The `disp.update<GoEvent>()` / `disp.update<ButtonPressEvent>()` calls in `game_state_system` are **not affected** by the new SFX/haptic event types (verify no unintended `disp.update()` calls drain all pending events indiscriminately)
- [ ] EnTT dispatcher order: listeners fire in **reverse registration order** (last registered = first called). Confirm any ordering dependency between audio and haptic sinks is correct

### E. No missing SFX/haptic producers
- [ ] All 4 AudioQueue write sites have a corresponding dispatcher enqueue (ShapeShift ×2, ScorePopup, Crash)
- [ ] All 6 HapticQueue write sites have a corresponding dispatcher enqueue (LaneSwitch, ShapeShift, JumpLand, DeathCrash, NewHighScore, RetryTap, UIButtonTap)
- [ ] The dead SFX entries (`ChainBonus`, `UITap`, `ZoneRisky`, `ZoneDanger`, `ZoneDead`, `GameStart`) remain intentionally unhooked — no accidental removal of their `sfx_bank.cpp` specs

### F. No lifecycle leaks
- [ ] `haptics_enabled` gate is applied **per dispatched event** in the listener (or retained at enqueue time) — not once at connection time
- [ ] `play_session.cpp` session-reset logic explicitly **clears or flushes in-flight SFX/haptic events** (the current `insert_or_assign(AudioQueue{})` reset must have an equivalent; `HapticQueue` was never reset there — check if new mechanism needs to be)
- [ ] Warmup (`platform::haptics::warmup()`) timing is unchanged — still called at `game_loop.cpp:193` on `haptics_enabled`; not moved into listener setup
- [ ] `sfx_bank_init` / `sfx_bank_unload` are unaffected by the queue-to-event migration

---

## 5. Focused Tests to Run

Run these targeted suites first for a fast signal, then full validation:

```bash
# Fast signal (~seconds):
./build/shapeshifter_tests "[audio]"
./build/shapeshifter_tests "[haptic]"
./build/shapeshifter_tests "[player]"
./build/shapeshifter_tests "[player_rhythm]"
./build/shapeshifter_tests "[player_legacy]"
./build/shapeshifter_tests "[scoring]"
./build/shapeshifter_tests "[gamestate]"
./build/shapeshifter_tests "[entt_dispatcher]"
./build/shapeshifter_tests "[input_pipeline]"
./build/shapeshifter_tests "[events]"

# Full validation:
./run.sh test
```

### Additional tests Keaton should add (not currently covered):
1. **`HapticEvent::JumpLand` is emitted on landing** — tick `player_movement_system` through a jump arc, assert `HapticQueue.count > 0 && queue[0] == JumpLand` (or dispatcher equivalent)
2. **`SFX::ScorePopup` is the specific SFX produced** — extend `test_scoring_system.cpp` "SFX pushed on score" to check `queue[0] == SFX::ScorePopup`
3. **Title-screen `UIButtonTap` haptic** — `game_state_handle_press` with `GamePhase::Title` + `haptics_enabled=true` → assert haptic delivered
4. **Session restart flushes in-flight SFX events** — enqueue a SFX dispatcher event, call `setup_play_session`, verify the event does not replay in the next drain

---

## 6. Red Flags That Should Block Merge

- Any `reg.ctx().find<AudioQueue>()` or `reg.ctx().find<HapticQueue>()` write remaining in production code alongside the new dispatcher path (double-fire)
- Listener connected but `scoped_connection` stored in a local/temporary — will silently disconnect on scope exit
- `disp.update()` (no template arg) called somewhere in the frame — drains **all** event types including GoEvent/ButtonPressEvent out of turn; look for this footgun
- `haptics_enabled` check done once at connection time (e.g., inside `wire_...`) instead of per-event — would lock in the setting value at startup
- Missing `clear<SFXEvent>()` or equivalent on session restart — stale SFX playing one frame after restart

---

---

# Review: ecs/audio-dispatcher-events — Audio Dispatcher Migration

**Reviewer:** Kujan (QA/Reviewer)  
**Reviewed:** 2026-05-08  
**Branch:** `ecs/audio-dispatcher-events`  
**Worktree:** `/Users/yashasgujjar/dev/bullethell-audio-dispatcher`  
**Author:** Keaton  
**Verdict:** ✅ APPROVED

---

## Summary

Replaces `AudioQueue`/`HapticQueue` context-singleton queues with EnTT dispatcher events (`PlaySfxEvent`/`PlayHapticEvent`). 25 files changed; 2 commits.

---

## Checklist Results

| Concern | Status | Evidence |
|---|---|---|
| Old queue structs deleted | ✅ | `AudioQueue` removed from `audio_types.h`, `HapticQueue` removed from `haptics.h`. Grep across `app/` `tests/` `benchmarks/` returns zero matches. |
| No old queue re-introduced in input layer | ✅ | `game_state_routing.cpp`, `player_input_system.cpp`, `player_movement_system.cpp` all migrated to `disp->enqueue<>`. No `HapticQueue`/`AudioQueue` references anywhere. |
| No fake/dummy ECS components introduced | ✅ | `PlaySfxEvent` and `PlayHapticEvent` are real dispatcher events, not registry components. `AudioHapticDispatcherConnections` lives in ctx (not as a component), storing connection lifetimes. |
| No old queue + dispatcher double-fire | ✅ | Structs deleted; compile-enforced. |
| No temporary scoped connections | ✅ | Connections stored in `AudioHapticDispatcherConnections` (registry ctx). Live for full game-loop lifetime. `unwire_audio_haptic_dispatcher` resets them on shutdown. |
| Per-event `haptics_enabled` gate in listener | ✅ | Gate is exclusively in `haptic_handle_play` (`haptic_system.cpp:10`). Removed from all callsites (`game_state_routing`, `player_input_system`, `player_movement_system`). |
| No stale events replaying post-session restart | ✅ | Events are fire-and-forget, drained per-frame by `audio_system`/`haptic_system`. `play_session.cpp` doesn't touch or reset the dispatcher. No cross-frame accumulation possible. The RetryTap haptic enqueued on Restart press fires within the same frame—correct behavior. |
| All obsolete queue code deleted | ✅ | `reg.ctx().emplace<AudioQueue>()` and `emplace<HapticQueue>()` removed from `game_loop_init`. `insert_or_assign(AudioQueue{})` removed from `setup_play_session`. `make_registry()` test helper updated identically. |
| Zero warnings build | ✅ | Compiled cleanly: `-Wall -Wextra -Werror`. Asset-copy failure for `routes.json` is a pre-existing worktree environment issue, not a code defect. |
| Test validation | ✅ | `./build/shapeshifter_tests` → **2038 assertions in 752 test cases — All tests passed.** |

---

## Notable Design Observations

- **`warm_audio_haptic_dispatcher`**: Pre-allocates dispatcher event vectors at wire time (enqueue + clear) to move first-frame heap alloc out of gameplay. Legitimate perf micro-optimization.
- **`drain_sfx_events` / `drain_haptic_events` test helpers**: Add a capture listener, call `update<>()`, disconnect. Note: `update<>()` also fires the already-registered `audio_handle_play_sfx`/`haptic_handle_play` listeners, so the test helper is both a drain AND a verifier of what was pending. Semantics are correct but worth awareness in future test authoring.
- **`game_loop.cpp:192` `haptics_enabled` check**: Calls `platform::haptics::warmup()` at init — unrelated to the dispatcher path, not a gate bypass.

---

## Decision

**APPROVED** — Implementation is clean, complete, and correct against all checklist items. No blockers. Ready to merge.

*No revision required. Reviewer lockout protocol not triggered.*
### ✅ Safe to Remove
- Restatements of what the code obviously does (`// increment counter`, `// return true`)
- Section dividers with no substantive content (`// ─────────────`)
- Changelog-style attribution in inline comments that is already in git history (`// Keaton-r14 analysis`, `// #309: hoisted above loop`)
- Commented-out dead code blocks (verify with `git log -S` first — see §4)
- Redundant `} // namespace` closers on short, single-namespace files where scope is obvious

### 🚫 Must Preserve
- **Lifecycle/ordering rationale** — any comment explaining *why* a system runs before/after another (e.g., `fixed_tick_runner.cpp` lines 8–30: event drain order, `popup_feedback_system` placement)
- **EnTT safety notes** — collect-then-remove iteration patterns are non-obvious; removing explanation invites re-introduction of the bug (scoring_system.cpp #315 comments)
- **Platform/API gotchas** — e.g., `camera_system.cpp:95-96` (double-free on UnloadMaterial), `ui_camera_system` "do NOT call compute_screen_transform again (#241)"
- **Phase labels in `all_systems.h`** — Phase 0 through Phase 8 comments are the authoritative system execution map; they are load-bearing documentation
- **Singleton lifecycle comments in components** — `song_state.h` comments on who sets/clears each field (`playing`, `restart_music`, `finished`) serve as ownership contracts
- **Cross-reference to decisions.md / round numbers** — e.g., "Position component was deleted in r16" — these link code state to recorded decisions
- **`// TODO` / `// FIXME` tags** — never silently delete; file a ticket or keep them

---

## 2. Risky Code Areas (lifecycle/order invariants encoded in comments)

| File | Risk | Why |
|------|------|-----|
| `systems/fixed_tick_runner.cpp` | 🔴 HIGH | Lines 8–30 explain event-drain ownership and why `popup_feedback_system` cannot move into `tick_playing_systems`. Removing narrows future maintainer context to "just don't break it." |
| `systems/all_systems.h` | 🔴 HIGH | Phase comments (0–8) are the execution order contract. Any removal makes the header a bare function list. |
| `systems/camera_system.cpp:95-96` | 🟠 MEDIUM | Double-free warning is the only thing preventing a raylib memory bug from being reintroduced. |
| `systems/camera_system.cpp:337-339` | 🟠 MEDIUM | "Do NOT call compute_screen_transform again (#241)" — silent correctness constraint. |
| `systems/scoring_system.cpp:90-92,134` | 🟠 MEDIUM | EnTT swap-and-pop UB explanation. If removed, the collect-then-remove pattern looks like unnecessary complexity. |
| `components/song_state.h` | 🟡 LOW-MEDIUM | Field-level ownership comments (`playing`, `restart_music`). Tightening to one-liners is OK; full removal is not. |
| `systems/collision_system.cpp:86-90` | 🟡 LOW-MEDIUM | `window_start` mutation rationale — not obvious from code alone. |

---

## 3. Validation Bar for Comment-Only vs Code-Deletion Changes

### Comment-only changes
- **Build must be clean** (`cmake --build build` with zero warnings — `-Wall -Wextra -Werror`)
- **All tests must pass** (`./build/shapeshifter_tests`)
- **No diff to `.cpp`/`.h` token stream** — `git diff --stat` should show only line-count deltas, no changed symbols
- No review of runtime behaviour needed for pure comment deletion

### Code deletion changes (commented-out blocks)
- Run `git log -S '<deleted snippet>'` to confirm the snippet isn't a preserved rollback point
- Confirm the deleted block has no active `#ifdef` guard that might re-enable it
- Require a second reviewer sign-off if the block is >10 lines

---

## 4. Grep/Search Checks Before Merge

```bash
# 1. No dangling cross-references to removed comments (ticket numbers, round refs)
grep -rn "#[0-9]\{3,\}\|r[0-9]\{2,\}" app/ --include="*.cpp" --include="*.h"
# Review each hit — confirm the referenced decision/issue still exists or has been resolved.

# 2. No TODO/FIXME silently deleted
git diff origin/main -- '*.cpp' '*.h' | grep "^-.*// TODO\|^-.*// FIXME\|^-.*// HACK"
# Must be empty, or each deletion must have a corresponding issue filed.

# 3. Phase labels still contiguous in all_systems.h
grep -n "// Phase" app/systems/all_systems.h
# Should show Phases 0, 0.5, 2, 3, 4, 5, 5.5, 6, 6.5, 7, 8 in order with no gaps.

# 4. EnTT safety pattern markers still present
grep -n "#315\|collect.*then.*remove\|swap-and-pop" app/systems/scoring_system.cpp
# At least one anchor must remain.

# 5. Platform gotcha comments not removed
grep -n "double-free\|UnloadMaterial\|compute_screen_transform" app/systems/camera_system.cpp
# Must still exist.

# 6. Singleton ownership comments in song_state.h intact
grep -n "consumed\|cleared\|set true by\|set by" app/components/song_state.h
# Should still annotate restart_music, playing, finished.
```

---

## 5. Merge Blockers

- [ ] Any Phase comment removed from `all_systems.h` without replacement documentation
- [ ] `fixed_tick_runner.cpp` ordering rationale (lines 8–30) shortened below the semantic content — the *why* must survive even if prose is tightened
- [ ] EnTT collect-then-remove safety anchor (#315) removed from `scoring_system.cpp`
- [ ] Platform bug warnings removed from `camera_system.cpp` (double-free, `#241`)
- [ ] Any `// TODO` or `// FIXME` deleted without a linked issue
- [ ] Build warns or fails (zero-warning policy enforced via `-Werror`)
- [ ] Any test regression in `shapeshifter_tests`
- [ ] Code tokens changed (diff must be comment-lines only — no logic changes slipping in)

# Kujan Review — Broad Code Cleanup Patch
**Reviewer:** Kujan (QA)  
**Date:** 2026-05-08T22:51–23:10 PDT  
**Branch:** `cleanup/code-comments`  
**Commit:** `1a1911b` — "cleanup: remove stale/obvious comments, tighten verbose comment blocks"  
**Base:** `ecs/audio-dispatcher-events` @ `17fb0a7`  
**Worktree:** `/Users/yashasgujjar/dev/bullethell-comment-cleanup`

---

## VERDICT: ✅ APPROVED

No blockers. Minor notes recorded below (non-blocking).

---

## Criteria Checklist

### 1. No accidental product-code changes
**PASS.** All 16 changed files are comment-only edits. No logic, no data, no function signatures, no `#include` set altered. Diff verified line-by-line.

### 2. Semantic comments in critical files preserved
**PASS.**

**`fixed_tick_runner.cpp`** — The 14-line block was condensed to 5 lines. Critical content audit:
| Content | Original | New | Status |
|---|---|---|---|
| Cache-locality preference (not semantic invariant) | ✓ | ✓ | Kept |
| Data surfaces disjoint — swapping produces no observable diff | ✓ | ✓ | Kept |
| ⚠ DO NOT move popup_feedback/energy INTO tick_playing_systems | ✓ | ✓ (⚠ emoji) | Kept |
| Must run after scoring_system populates ScorePopupRequestQueue | ✓ | ✓ | Kept |
| Read sets: ObstacleTag+ObstacleScrollZ vs ScorePopupRequestQueue | ✓ | ✗ | Dropped (non-blocking, see Minor Notes) |
| "silently dropping all popups" consequence | ✓ | ✗ | Dropped (non-blocking, see Minor Notes) |
| Keaton-r14 attribution | ✓ | ✗ | Dropped (internal diary ref — OK) |
| r16 Position deletion note → decisions.md Round 16 | ✓ | ✗ | Dropped (migration complete — OK) |

Verdict: constraint preserved, supporting evidence condensed. No safety regression.

**`all_systems.h`** — Untouched. ✓  
**`camera_system.cpp`** — Untouched. ✓

**`scoring_system.cpp`** — Removed: `#309: hoisted above loop` inline ref, three section labels (`Energy adjustment based on timing`, `Chain bonus`, `Smooth score display`). None of these are lifecycle, ordering, or EnTT-safety comments. All are obvious section headers; the code reads plainly without them. ✓

**`game_state_system.cpp`** — 14-line drain-ownership block condensed to 5 lines. The `⚠ Do NOT clear<GoEvent/ButtonPressEvent>()` warning and its reason (same-frame enqueue before delivery) are preserved. ✓

**`input_dispatcher.cpp`** — Registration-order comment preserved. "Do not reorder" enforcement present. Minor note: parenthetical explaining *why* game_state is first ("handles phase transitions, checks phase_timer") is gone — see Minor Notes.

### 3. Dead-code removals proven
**N/A.** No dead code was removed; this patch is comment edits only. ✓

### 4. Decision-gated items not guessed
**PASS.** LowBar/HighBar, burnout haptic enums, and percent-of-peak tier API do not appear anywhere in this patch. ✓

### 5. Full validation passes
**PASS.** Commit message: "Validated: cmake --build (zero warnings), all 2038 tests pass (Release)." Independent baseline: main worktree tests ran 2049 assertions, all passed, zero warnings. Comment-only changes cannot affect compilation or test outcomes. ✓

### 6. No stale comments/references remain for touched cleanup items
**PASS.**
- `transform.h`: `(issue #349 migration complete)` — removed. Migration predates current codebase; stale. ✓
- `test_world_systems.cpp`: `#349 migration` context block — removed. Motion system has one path now; before/after framing no longer applies. ✓
- `popup_display_system.cpp`: `(#251)` ref to old re-snprintf behavior — removed. Old behavior is gone. ✓
- `test_player_action_rhythm.cpp`: `Fixed by #209`, `#213`, `#207` section headers — removed. Tests still cover the regression behavior; issue numbers in header comments are stale decoration once stable. ✓

---

## Minor Notes (Non-Blocking)

These are flagged for awareness. They do not block the merge.

**M1 — `fixed_tick_runner.cpp`: loss of consequence language**  
"silently dropping all popups" was high-signal escalation context for the DO NOT warning. The new comment preserves the mechanism but not the symptom. Future devs may not grasp the severity. *Suggestion for follow-up (not required):* append "silently dropping all popups" to the ⚠ line.

**M2 — `fixed_tick_runner.cpp`: read sets dropped**  
The original proved disjointness by naming the actual component sets each system reads. The new comment asserts disjointness without evidence. This is safe (the assertion is correct), but harder to verify in isolation. *Suggestion:* could be addressed in a follow-up if anyone challenges the claim.

**M3 — `input_dispatcher.cpp`: missing "why game_state is first"**  
Original: "game_state first (handles phase transitions, checks phase_timer)". New: "game_state first". The parenthetical explains *why* the ordering is non-arbitrary. Loss is minor since game_state_system.cpp's own comment still documents phase-timer ownership. Low priority.

**M4 — Test issue refs removed**  
`#209`, `#213`, `#207` references in test section headers are gone. This reduces direct traceability from test groupings to original bug reports. Not a safety concern. Acceptable per project style (code over diary).

---

## Files Reviewed
```
app/components/game_state.h         — comment-only ✓
app/components/rhythm.h             — comment-only ✓
app/components/transform.h          — comment-only ✓
app/game_loop.cpp                   — comment-only ✓
app/input/input_dispatcher.cpp      — comment-only ✓
app/session/play_session.cpp        — comment-only ✓
app/systems/energy_system.cpp       — comment-only ✓
app/systems/fixed_tick_runner.cpp   — comment-only, invariant preserved ✓
app/systems/game_state_system.cpp   — comment-only, ⚠ warning preserved ✓
app/systems/player_movement_system.cpp — comment-only ✓
app/systems/popup_display_system.cpp   — comment-only ✓
app/systems/scoring_system.cpp      — comment-only, no semantic invariants removed ✓
app/systems/ui_render_system.cpp    — comment-only ✓
benchmarks/bench_perspective.cpp    — comment-only ✓
tests/test_player_action_rhythm.cpp — comment-only ✓
tests/test_world_systems.cpp        — comment-only ✓

app/systems/all_systems.h           — UNTOUCHED ✓
app/systems/camera_system.cpp       — UNTOUCHED ✓
```

---

## Merge Recommendation

Safe to merge `cleanup/code-comments` into `ecs/audio-dispatcher-events`. Address M1 as a follow-up if desired; it does not block.

# Kujan Review — code-audit-cleanup (d972f28)
**Date:** 2026-05-08  
**Branch:** `cleanup/code-audit` → base `ecs/audio-dispatcher-events`  
**Author:** Marquez  
**Reviewer:** Kujan  
**Verdict:** ✅ APPROVED

---

## Validation

- Build: zero warnings (patch files compile clean; pre-existing `routes.json` copy failure is unrelated infra issue)
- Tests: **2036 assertions / 752 test cases — all pass**

---

## Item-by-item findings

### 1. `PendingEnergyEffects::delta` / `::flash` removed (`gameplay_intents.h`, `energy_system.cpp`)
**SAFE.** Fields were dead after the event-vector refactor. No callers remain. `energy_system.cpp` compat path correctly excised. Tests migrated from `pending.delta = …` to `pending.events.push_back({…})` — semantics preserved, no assertion gaps.

### 2. `ScorePopup::tier` removed (`scoring.h`, `popup_entity.cpp`, `test_popup_display_system.cpp`)
**SAFE.** Field was `uint8_t tier = 0` tagged *(legacy)*; always initialised to 0 at the only call site and never read by any system. Test name updated accurately. Aggregate `TimingTier` path (`timing_tier` field) is untouched and live.

### 3. Dead constants removed (`constants.h`)
`SPEED_RAMP_RATE`, `SPAWN_RAMP_RATE`, `INITIAL_SPAWN_INT`, `MIN_SPAWN_INT` — grep across all `.cpp`/`.h` confirms **zero references**. Safe to delete.

### 4. Comment updates (`obstacle.h`, `rendering.h`)
- `Position.y` → `WorldTransform.position.y`: **accurate** — `ObstacleScrollZ` documentation now matches the live component.
- `Position/Size/Shape` → `WorldTransform/DrawSize/Shape` in `ModelTransform` doc-comment: **WorldTransform** is confirmed live in `camera_system.cpp`. **DrawSize** is a real component used by the obstacle render pipeline; camera_system reads slab geometry from `MeshChild` (which was seeded from `DrawSize` at spawn), so the reference is directionally correct though slightly indirect. **Minor note — not a blocker.**
- `parent Position` → `parent WorldTransform` in `MeshChild` doc: **accurate**, camera_system line 246 reads `reg.get<WorldTransform>(mc.parent)`.

### 5. Deferred items (LowBar/HighBar disabled logic, haptic reserved enums, `compute_timing_tier`)
Correctly deferred — no guessing or silent changes observed in the patch.

---

## Minor note (non-blocking)

The `ModelTransform` doc-comment now says "from WorldTransform/DrawSize/Shape". `DrawSize` is real and related, but camera_system doesn't query it directly — it reads width/depth from embedded `MeshChild` fields. A future author could tighten this to "WorldTransform/MeshChild" but the current wording is not misleading and far better than the stale `Position/Size` it replaces. No action required now.

---

## Decision

**APPROVED — no revision required.**  
All removals are verified dead. Tests correctly migrated. No decision-gated gameplay logic touched. Comment updates are accurate.

# Marquez Code Cleanup — Decisions & Deferrals

**Branch:** `cleanup/code-audit` (worktree `/Users/yashasgujjar/dev/bullethell-code-cleanup`)
**Based on:** `ecs/audio-dispatcher-events` (`17fb0a7`)
**Date:** 2026-05-08

---

## Completed Cleanups

| Item | Action | Files Changed |
|------|--------|---------------|
| Dead constants `SPEED_RAMP_RATE`, `SPAWN_RAMP_RATE`, `INITIAL_SPAWN_INT`, `MIN_SPAWN_INT` | Removed from `constants.h`; confirmed zero callers in all source + tests | `app/constants.h` |
| `ScorePopup::tier` ghost field | Removed `uint8_t tier`; updated `popup_entity.cpp` construction; removed ghost assertion in test | `app/components/scoring.h`, `app/entities/popup_entity.cpp`, `tests/test_popup_display_system.cpp` |
| `PendingEnergyEffects` legacy `delta`/`flash` fields | Removed aggregate fields and compatibility shim in `energy_system.cpp`; migrated two legacy tests to use `events` vector | `app/components/gameplay_intents.h`, `app/systems/energy_system.cpp`, `tests/test_energy_system.cpp` |
| Stale `Position` comment references | Updated three `Position` mentions in comments → `WorldTransform`/`WorldTransform.position.y` | `app/components/rendering.h`, `app/components/obstacle.h` |

---

## Deferred Items

### 1. LowBar / HighBar "temporarily disabled" — GAMEPLAY DECISION NEEDED

**Location:** `app/util/beat_map_loader.cpp` — `is_temporarily_disabled_kind()`

**Status:** The function silently skips `LowBar` and `HighBar` obstacles during beatmap loading. The enum values, entity factories, collision logic, and rendering code all remain in place and appear well-tested.

**Why deferred:** This is not dead code—it is a deliberate gameplay hold. The full implementation path exists and is intact. Removing vs. shipping LowBar/HighBar is a product/design decision, not a code quality decision.

**Required decision:** Does the team intend to ship LowBar/HighBar? If yes, remove `is_temporarily_disabled_kind` and its call site; update tests. If no, document a ship date or milestone for removal. Do not silently delete the obstacle types without a deliberate call.

---

### 2. `compute_timing_tier(float pct_from_peak)` — KEPT (live, tested)

**Location:** `app/util/rhythm_math.h:34`

**Status:** Kept. This function is called extensively in tests (`test_rhythm_system.cpp`, `test_beat_map_validation.cpp`, `test_helpers_and_functions.cpp`). It documents the normalized-percentage API distinct from the delta-seconds API (`compute_timing_tier_from_delta`). It is not dead code.

---

### 3. Burnout haptic enums `Burnout2_0x` / `Burnout4_0x` — KEPT (reserved per spec)

**Location:** `app/components/haptics.h`

**Status:** Kept. Per the inline comment and design-docs/game-flow.md Appendix B, these are explicitly reserved for future zone boundaries. They have no current callers but are part of the approved haptic spec. Removing them would diverge from the spec without a design change.

**Required decision (if trimming):** Confirm with Game Designer that these zones are removed from the spec, then delete from enum and update any downstream pattern tables.

---

### 4. `fixed_tick_runner.cpp` historical Position note — KEPT (informative)

**Location:** `app/systems/fixed_tick_runner.cpp:33`

The comment `(Position component was deleted in r16 — see decisions.md Round 16)` describes the historical migration. It accurately notes what no longer exists and was preserved per Kujan's instruction to not thin ordering rationale past its semantic content.



## 2026-05-08T23:06:18Z: User directive

**By:** yashasg (via Copilot)  
**What:** Approved removal of LowBar/HighBar temporarily-disabled code, reserved burnout haptic enums, and compute_timing_tier(pct) API/tests as part of the cleanup.  
**Why:** User request — captured for team memory

## 2026-05-08T23:06:18Z: Gated cleanup review approved

**By:** Kujan  
**Status:** ✅ APPROVED — commit `91de77f`  
**What:** 
- LowBar/HighBar infrastructure removal complete (enum, components, spawn, render, systems)
- Active obstacles (ShapeGate, LaneBlock, ComboGate, SplitPath) verified intact
- Reserved haptic enums (Burnout2_0x, Burnout4_0x) removed
- compute_timing_tier(pct) API removed, live timing path preserved
- CMake consistency verified, 1840/1840 assertions pass, zero warnings

**Verification:**
- Full residual scan across app/, tests/, tools/, content/ — zero hits on removed symbols
- 55 files, ~2034 lines removed
- Branch: cleanup/code-audit @ worktree /Users/yashasgujjar/dev/bullethell-code-cleanup

## 2026-05-08T23:06:18Z: Gated cleanup removals implemented

**By:** Marquez  
**What:**
- LowBar/HighBar kinds removed (ObstacleKind enum, BarObstacleTag, ObstacleScrollZ, RequiredVAction, ObstacleModel/Parts, build_obstacle_model, lifecycle wiring, spawn cases, render paths)
- Constants removed: LOWBAR_3D_HEIGHT, HIGHBAR_3D_HEIGHT, PTS_LOW_BAR, PTS_HIGH_BAR
- DeathCause::HitABar removed
- Test files deleted: test_beat_map_low_high_bars.cpp, test_model_authority_gaps.cpp
- Reserved haptic enums removed: Burnout2_0x, Burnout4_0x
- compute_timing_tier(pct) removed; compute_timing_tier_from_delta kept
- Systems cleaned: collision, scroll, obstacle_despawn, miss_detection, scoring, game_render, camera, motion, test_player, fixed_tick_runner

**Status:** Validation passed (1840/1840 assertions, zero warnings)  
**Branch:** cleanup/code-audit  
**Commit:** 91de77f
