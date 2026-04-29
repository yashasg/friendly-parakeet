### #190 — Portable C rGuiLayout Integration Decision (2026-05-19)

**Owner:** Keyser (Lead Architect)  
**Status:** APPROVED — Ready for implementation  
**Scope:** ECS architecture, UI integration boundary, terminology

**Decision:** Portable generated C is the primary integration contract. rGuiLayout exports header-only UI layout definitions; game screen controllers include these and translate raygui input into game events. Generated code is the authoritative contract; screen controllers are game-specific glue.

**Terminology:**
- Primary: **"Portable generated C as the integration contract"**
- Implementation: **"Screen controllers"** (game-specific code that consumes the contract)
- Legacy naming: `app/ui/adapters/` to be migrated to `app/ui/screen_controllers/`

**Rationale:**
1. **Upstream ownership:** rGuiLayout (the tool) produces portable libraries. The game adopts the contract, not the reverse.
2. **Clarity for future developers:** "Portable C" signals do-not-edit; "screen controller" signals game-specific integration.
3. **Reusability:** Generated layouts could serve other games; screen controllers are always project-specific.
4. **Architectural clarity:** Separates the portable contract (generated) from the game-specific implementation (screen controllers).

**Design Outcome:** Spec at `design-docs/rguilayout-portable-c-integration.md` (17KB, 11 sections).

**Key sections:**
- Flow: `.rgl` → generated `.h` → screen controller → `ui_render_system`
- File layout: `app/ui/generated/<screen>_layout.h` (portable), `app/ui/screen_controllers/<screen>_screen_controller.cpp` (game integration)
- Why not compile generated C directly (RAYGUI_IMPLEMENTATION ownership)
- API shape contract: `Init()` / `State` / `Render()` (no update/cleanup unless custom resources)
- Event-consumer design: UI effects applied around layout render, not within
- Strict non-goals: no ECS widget mirrors, no copying layout data, no hand-editing generated files, no standalone compilation
- Migration path: rename `adapters/` → `screen_controllers/`, rename functions `<screen>_adapter_render()` → `render_<screen>_screen_ui()`
- CMake integration guidance updated to use screen controller naming

**Non-Goals:**
- Does NOT change the current implementations (they already follow portable C pattern correctly).
- Does NOT require immediate CMake wiring or build integration.
- Does NOT change game-phase routing, dispatcher, or event semantics.
- Does NOT require immediate migration of existing `adapters/` folder (migration can be staged).

**Impact:**
- **Scope:** Specification and team alignment.
- **Affected modules:** None immediately (spec-driven).
- **Implementation timeline:** Current code is correct; migration of `adapters/` → `screen_controllers/` is future work.
- **Risk:** Low — clarifies existing pattern, no new behavior introduced.

**Follow-Up Actions:**
- Team review of revised `rguilayout-portable-c-integration.md`.
- When new screens land or existing screens are regenerated, use `screen_controllers/` naming.
- Plan migration of existing `adapters/` → `screen_controllers/` (can be staged per-screen).
- Integrate into contributor onboarding docs: "Portable C is the contract; screen controllers are the glue."

**Decision Ownership:**
- **Keyser:** Approved—stands as the ECS/architecture decision.
- **Team:** Implement screen controller naming on new/regenerated screens; migrate legacy adapters on a rolling basis.

---

### keyser-system-boundary-cleanup (2026-05-18)
**By:** Keyser  
**Status:** RECOMMENDATION — for Keaton/reviewer to implement  
**Directive source:** `.squad/decisions/inbox/copilot-directive-20260428T091354Z-system-boundaries.md`

---

## 1. beat_map_loader → app/util/

**Finding:** `beat_map_loader.h/.cpp` has zero ECS surface — no `entt::registry`, no `float dt`, no system signature. It is a JSON→BeatMap parser + validator. Its profile is identical to `high_score_persistence` and `settings_persistence` already living in `app/util/`.

**Recommendation:** Move files verbatim to `app/util/beat_map_loader.h/.cpp`. No rename of functions or types required. The name "loader" is fine — `high_score_persistence` sets the naming precedent for this layer.

**Why not app/io or app/loaders?** Those directories don't exist. Creating them adds a new GLOB entry to CMakeLists.txt. `app/util/` already covers this pattern and requires zero CMake changes.

**CMake impact:**
- `SYSTEM_SOURCES = file(GLOB app/systems/*.cpp)` loses the file automatically on move.
- `UTIL_SOURCES = file(GLOB app/util/*.cpp)` gains it automatically.
- `shapeshifter_lib` link graph is unchanged. No new FILTER or GLOB needed.
- **Action required after move:** `cmake -B build -S .` (re-configure to refresh globs).

**Include sites that must be updated (4 total):**
| File | Old include | New include |
|---|---|---|
| `app/systems/play_session.cpp` | `"beat_map_loader.h"` | `"../util/beat_map_loader.h"` (or `"util/beat_map_loader.h"` via the `app/` include root) |
| `tests/test_shipped_beatmap_shape_gap.cpp` | `"systems/beat_map_loader.h"` | `"util/beat_map_loader.h"` |
| `tests/test_beat_map_parser_unknown_fields.cpp` | `"systems/beat_map_loader.h"` | `"util/beat_map_loader.h"` |
| `tests/test_beat_map_low_high_bars.cpp` | `"systems/beat_map_loader.h"` | `"util/beat_map_loader.h"` |

---

## 2. cleanup_system → obstacle_despawn_system

**Finding:** `cleanup_system` *is* an ECS system — it has a valid `(entt::registry&, float dt)` signature and runs at Phase 6 in `game_loop.cpp`. The problem is:

1. **Name is wrong.** "Cleanup" is ambiguous. The function's sole job is to despawn `ObstacleTag` entities that have scrolled past the player. `obstacle_despawn_system` is precise and matches the established naming convention (`miss_detection_system`, `scoring_system`, `scroll_system`).

2. **Despawn threshold for the Z-path is not camera-derived.**
   - Current: `oz.z > constants::DESTROY_Y` where `DESTROY_Y = 1400.0f` — a 2D screen-Y constant.
   - The `GameCamera` entity lives in the registry with `cam.position.z = 1900.0f` (set in `spawn_game_camera`).
   - For `ObstacleScrollZ.z` (model-authority obstacles: LowBar, HighBar), "past the camera" means `oz.z >= camera.cam.position.z`. The current 1400 threshold culls them 500 units before the camera — it works accidentally but is wrong conceptually and will break if the camera Z ever changes.
   - Fix: query `reg.view<GameCamera>()`, read `cam.cam.position.z`, use that as the despawn Z.
   
3. **The Position.y path (2D obstacles) is acceptable as-is.** `pos.y > DESTROY_Y(1400)` means 120px past the bottom of the 1280px logical screen. This path is not 3D-projection-dependent — a small screen-margin constant is the right approach. Rename `DESTROY_Y` to `OBSTACLE_DESPAWN_SCREEN_Y` or add a comment clarifying it is screen-space, not camera-space.

**Recommendation:**
- Rename `cleanup_system` → `obstacle_despawn_system` (function, filename, declaration in `all_systems.h`).
- Keep file in `app/systems/`. No folder change.
- For the `ObstacleScrollZ` despawn path: replace `constants::DESTROY_Y` with the camera's actual Z. Query `reg.view<GameCamera>()` once per frame tick; if no camera entity exists yet, fall back to `DESTROY_Y` to keep headless tests safe.
- For the `Position.y` despawn path: keep `DESTROY_Y` or add a named alias for clarity.

**Location after rename:**
- `app/systems/cleanup_system.cpp` → `app/systems/obstacle_despawn_system.cpp`
- SYSTEM_SOURCES glob picks it up automatically — no CMake change.

---

## 3. Test and CMake risks

| Risk | Severity | Mitigation |
|---|---|---|
| `test_world_systems.cpp` calls `cleanup_system(...)` by name in 4 tests | P1 — build break on rename | Update all 4 calls to `obstacle_despawn_system` atomically |
| `test_game_state_extended.cpp` references `cleanup_system` in comments | P3 — cosmetic | Update comment text |
| `test_death_model_unified.cpp` references `cleanup_system` in a comment | P3 — cosmetic | Update comment text |
| `test_player_system.cpp` has a comment referencing `cleanup_system` | P3 — cosmetic | Update comment text |
| CMake glob re-configure after file moves | P2 — stale build | Always run `cmake -B build -S .` after moving files |
| `test_model_authority_gaps.cpp` references `cleanup_system` in its context comment | P3 — cosmetic | Update comment text |

---

## 4. Acceptance criteria

**beat_map_loader move:**
- [ ] `app/systems/beat_map_loader.h` and `.cpp` deleted from `app/systems/`
- [ ] Same files present at `app/util/beat_map_loader.h` and `.cpp` (content unchanged)
- [ ] All 4 include sites updated to `"util/beat_map_loader.h"` (or correct relative path)
- [ ] CMake re-configured; `cmake --build build` passes with zero warnings
- [ ] All beat map tests pass: `test_shipped_beatmap_shape_gap`, `test_beat_map_parser_unknown_fields`, `test_beat_map_low_high_bars`
- [ ] `app/systems/all_systems.h` has NO declaration for any beat_map_loader function (it never had one — confirm it stays clean)

**obstacle_despawn_system rename + threshold fix:**
- [ ] `app/systems/cleanup_system.cpp` renamed to `app/systems/obstacle_despawn_system.cpp`
- [ ] Function renamed to `obstacle_despawn_system` in source, header, and `all_systems.h`
- [ ] `game_loop.cpp` Phase 6 call updated from `cleanup_system` to `obstacle_despawn_system`
- [ ] The `ObstacleScrollZ` despawn path queries `GameCamera.cam.position.z` from registry; falls back to `DESTROY_Y` when no camera entity is present
- [ ] All 4 `test_world_systems.cpp` tests updated; tests still pass
- [ ] All 4 cosmetic comments in other files updated
- [ ] `cmake --build build` passes with zero warnings on all paths (Clang + MSVC)
- [ ] Existing test suite passes end-to-end: `./build/shapeshifter_tests`


# Transform Migration Contract
**Author:** Keyser  
**Date:** 2026-04-28  
**Executors:** Keaton (implementation), McManus (integration + tests)  
**Status:** READY FOR EXECUTION — pending answers to blocking questions in §4

---

## 1. What We're Replacing and Why

Current state: `transform.h` holds `Position {float x, y}` and `Velocity {float dx, dy}`. All movement systems read/write `Position` directly. `game_camera_system` derives the render matrix from Position + DrawSize + constants each frame and writes it into `ModelTransform`.

Target state: `Transform { Matrix mat }` replaces `Position` as the source of truth for world-space location of any entity that moves or occupies a position in the game world. `Velocity` stays. `ModelTransform` stays (camera_system still produces it from Transform + size/color/mesh data). The existing local matrix helpers inside `camera_system.cpp` get promoted to a shared helper header.

This matches every user directive from 2026-04-28:
- *"If we use Transform, we do not need a separate Vector2 for position"*
- *"Pretty much every entity that moves should have a Transform component"*
- *"Matrix-backed Transform updates should use raylib/EnTT-friendly helper APIs"*

---

## 2. Coordinate Mapping

The game uses a 2D logical coordinate space:
- `x` = left/right (maps to 3D X axis)
- `y` = scroll depth / beat timing axis (maps to 3D Z axis — positive going "into the screen")
- Vertical (`VerticalState.y_offset`) = 3D Y (height) — NOT stored in Transform; derived from VerticalState at render time by `game_camera_system`

raylib `Matrix` is column-major. Translation sits at `{m12, m13, m14}`.

**Mapping:**
```
game x  →  Matrix.m12  (3D X)
game y  →  Matrix.m14  (3D Z)
vertical → computed by camera_system from VerticalState, not stored in Transform
```

---

## 3. Migration Contract

### kujan-boundary-cleanup-review (2026-05-18)
**By:** Kujan  
**Scope:** Keaton's system-boundary cleanup — beat_map_loader move + cleanup_system → obstacle_despawn_system rename  
**Directive refs:** `copilot-directive-20260428T091354Z-system-boundaries.md`, `keyser-system-boundary-cleanup.md`  
**Verdict:** ❌ REJECTED — 2 blocking defects (BL-1, BL-2). Keaton locked out. Revision owner: Keyser.

---

## What Was Done Correctly

| Check | Result |
|---|---|
| `app/util/beat_map_loader.h/.cpp` present with content intact | ✅ |
| `app/systems/beat_map_loader.h/.cpp` deleted | ✅ |
| All 4 Keyser-listed include sites updated to `"util/beat_map_loader.h"` | ✅ |
| `all_systems.h` — `cleanup_system` declaration replaced with `obstacle_despawn_system` | ✅ |
| `game_loop.cpp` Phase 6 call updated to `obstacle_despawn_system` | ✅ |
| `test_world_systems.cpp` — all call sites renamed | ✅ |
| `app/components/obstacle.h` comment updated | ✅ |
| No new misplaced headers introduced | ✅ |

---

## Blocking Defect BL-1 — `benchmarks/bench_systems.cpp`: 4 stale `cleanup_system` calls

**Files:** `benchmarks/bench_systems.cpp`, lines 192, 196, 215, 235

`cleanup_system` is called directly in three benchmark test cases:
- `TEST_CASE("Bench: cleanup_system", ...)` — test name and both `meter.measure(...)` and the case title reference the old symbol
- `TEST_CASE("Bench: full frame (typical)", ...)` — line 215
- `TEST_CASE("Bench: full frame (stress)", ...)` — line 235

`cleanup_system` no longer exists. The build fails with "use of undeclared identifier 'cleanup_system'" on all three test cases. The rename was applied to `tests/test_world_systems.cpp` but not to `benchmarks/bench_systems.cpp`.

**Required fix:**
- Line 192: rename test case string to `"Bench: obstacle_despawn_system"`
- Lines 196, 215, 235: rename all `cleanup_system(reg, DT)` calls to `obstacle_despawn_system(reg, DT)`

---

## Blocking Defect BL-2 — `obstacle_despawn_system.cpp`: Z-path does not query camera

**File:** `app/systems/obstacle_despawn_system.cpp`, line 20

```cpp
// model_view path (ObstacleScrollZ):
if (oz.z > constants::DESTROY_Y)   // BL-2: WRONG — must use camera.cam.position.z
```

The directive and Keyser's acceptance criterion (§2, criterion 5) are explicit:

> The `ObstacleScrollZ` despawn path queries `GameCamera.cam.position.z` from registry; falls back to `DESTROY_Y` when no camera entity is present.

`DESTROY_Y = 1400.0f`. `GameCamera.cam.position.z = 1900.0f` (set in `spawn_game_camera`). Obstacles are destroyed 500 Z-units before the camera — accidentally correct today, silently broken the moment camera Z changes. The function comment itself says "camera's far-Z boundary" but uses a hard constant — a semantic lie.

The `Position.y` path on line 29 (`pos.y > constants::DESTROY_Y`) is correct and should remain unchanged.

**Required fix:**
```cpp
void obstacle_despawn_system(entt::registry& reg, float /*dt*/) {
    static std::vector<entt::entity> to_destroy;
    to_destroy.clear();

    // Resolve camera Z: use actual camera position; fall back to DESTROY_Y in headless tests.
    float cam_z = constants::DESTROY_Y;
    auto cam_view = reg.view<GameCamera>();
    if (!cam_view.empty()) {
        auto [cam_entity, gc] = *cam_view.each().begin();
        cam_z = gc.cam.position.z;
    }

    auto model_view = reg.view<ObstacleTag, ObstacleScrollZ>();
    for (auto [entity, oz] : model_view.each()) {
        if (oz.z >= cam_z)
            to_destroy.push_back(entity);
    }
    // ... rest unchanged
```

Add `#include "../components/camera.h"` to the include list.

---

## Lockout and Revision Assignment

- **Keaton:** locked out of this revision per squad lockout policy.
- **Revision owner:** Keyser.
- Both blockers are small, well-scoped fixes. No architectural re-work needed.


# Kujan Review Decision: ECS Cleanup Batch #273/#333/#286/#336/#342/#340

**Filed:** 2026-04-28  
**Reviewer:** Kujan  
**Branch:** user/yashasg/ecs_refactor

---

## Verdict: ✅ ALL SIX ISSUES APPROVED — CLOSURE-READY

---

## Per-Issue Findings

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

### #190 — Approved Template Adapter Pattern for UI Boilerplate (2026-04-29)

**Owner:** Keaton (C++ Performance Engineer)  
**Status:** APPROVED

The `RGuiAdapter<State, InitFunc, RenderFunc>` pattern demonstrated in commit 958a7d9 is now the **approved standard** for wrapping external library state in this codebase.

**Rationale:**
1. Proven effectiveness: Eliminated 377 lines of duplicated adapter boilerplate (33% reduction)
2. Zero overhead: Compile-time template instantiation, no runtime cost
3. Type safety: Incorrect signatures caught at compile-time
4. Maintainability: New adapters require ~20 lines vs ~45 lines manual implementation

**Application Scope:**
Use this pattern when:
- 3+ classes share identical lifecycle (init, render/update, cleanup)
- State managed by external C library with function pointers
- Need zero-overhead abstraction (performance-sensitive paths)

**Reference Implementation:**
- Location: `app/ui/adapters/adapter_base.h`
- Example usage: `app/ui/adapters/game_over_adapter.cpp`
- Skill documentation: `.squad/skills/cpp-template-adapter/SKILL.md`

**Future Application:**
Consider this pattern for:
- Audio context wrappers
- Physics engine state managers
- Platform-specific window/input abstractions

---

### #191 — Validation: Keyser Commit 958a7d9 — UI Adapter Template Refactor (2026-04-29)

**Owner:** Hockney (Platform Engineer)  
**Status:** ✅ APPROVED

Validated Keyser's UI adapter template refactor (commit 958a7d9) for build/platform safety. All validation criteria met: zero-warning unity build, passing test suite, RAYGUI_IMPLEMENTATION single-site invariant preserved, standalone generated files confirmed uncompiled, template headers safe for unity builds.

**Validation Results:**

1. **Build Safety (Zero Warnings):** ✅ PASS
   - Unity build (`build-unity-verify-vcpkg`) completed cleanly
   - `clang -Wall -Wextra -Werror` flags: no errors

2. **Test Integrity:** ✅ PASS
   - Full test suite: All tests passed (2635 assertions in 901 test cases)

3. **RAYGUI_IMPLEMENTATION Single-Site Invariant:** ✅ PASS
   - Exactly ONE compiled site: `app/ui/raygui_impl.cpp` (line 20)
   - All embeddable layout headers: comment disclaimers ("NO RAYGUI_IMPLEMENTATION")
   - All standalone files: contain `#define RAYGUI_IMPLEMENTATION` BUT are C source (.c) in excluded directory

4. **Unity Build Safety:** ✅ PASS
   - Template headers properly structured (implicitly inline)
   - Adapter instances have unique names per file: gameplay_adapter, title_adapter, paused_adapter, settings_adapter, level_select_adapter, game_over_adapter, song_complete_adapter, tutorial_adapter
   - Symbol check confirms no ODR conflicts

5. **C++17/C++20 Compatibility:** ✅ PASS
   - Project requires C++20 (CMakeLists.txt line 20)
   - Adapter template uses C++17 `auto` template parameters (fully compatible, superset)

6. **Standalone Generated Output Exclusion:** ✅ PASS
   - Verified standalone files never compiled
   - `find build -name "*.c.o" | grep standalone` → 0 results

**Architectural Impact:**
- Before: ~240 lines of duplicated init/state-management/render boilerplate across 8 adapters
- After: ~150 fewer lines total
  - `adapter_base.h`: 44 lines (generic template)
  - `end_screen_dispatch.h`: 20 lines (shared button dispatcher)
  - 8 adapters: ~20 lines each (type alias + instance + glue functions)

**Verdict:** ✅ APPROVED — No revision required. Recommend merge to trunk.

**Follow-up:** Created skill: `.squad/skills/unity-build-template-safety/SKILL.md` (reusable pattern for future template-based refactors in unity build projects).


### #189 — raygui Title Screen Runtime Integration (2026-04-28)

**Owners:** Fenster (implementation), Hockney (validation)  
**Status:** APPROVED

Title screen now renders via rguilayout in-game. Completed Phase 3 runtime integration with zero warnings, passing test suite (2631 assertions, 901 cases), and no regressions to other screens.

**Implementation (Fenster):**
- Vendored raygui v5.0 to `app/ui/vendor/raygui.h` (6056 lines)
- Created single RAYGUI_IMPLEMENTATION site at `app/ui/raygui_impl.cpp` with warning suppression
- Fixed generated header linkage issues: nested `extern "C"` blocks → single block with `static inline`, nested comments, aggregate initialization
- Wired title adapter into CMakeLists.txt and ui_render_system.cpp dispatcher
- Title screen renders: "SHAPESHIFTER" label, "TAP TO START" label, EXIT button, SET button

**Validation (Hockney):**
- ✅ Build passes zero warnings: `cmake --build build --target shapeshifter`
- ✅ Test suite passes: `./build/shapeshifter_tests` (2631 assertions, 901 cases)
- ✅ Single RAYGUI_IMPLEMENTATION site verified (grep confirmed)
- ✅ No ODR violations: header-only generated layout, no external symbols
- ✅ No regressions: other 7 screens preserve JSON-driven paths
- ✅ Cleanup complete: temp validation files removed

**Architecture:**
```
content/ui/screens/title.rgl
  ↓ (generated with custom template)
app/ui/generated/title_layout.h (header-only, no main, no RAYGUI_IMPLEMENTATION)
  ↓ (called by)
app/ui/adapters/title_adapter.cpp (includes raygui.h, renders layout)
  ↓ (called by)
app/systems/ui_render_system.cpp (ActiveScreen::Title case, line 421-424)
```

**Known Limitations (Intentional Deferrals):**
- Button actions not yet wired (visuals complete, actions require event system refactor)
- Only Title screen migrated (1/8); remaining 7 screens still JSON-driven
- Incremental strategy allows visual validation before action layer

**Files Changed:**
| File | Status |
|------|--------|
| `app/ui/vendor/raygui.h` | ✨ New (vendored) |
| `app/ui/raygui_impl.cpp` | ✨ New (RAYGUI_IMPLEMENTATION site) |
| `app/ui/adapters/title_adapter.*` | ✨ New (adapter) |
| `app/ui/generated/title_layout.h` | ✅ Fixed (linkage, comments, init) |
| `app/systems/ui_render_system.cpp` | ✅ Updated (dispatch) |
| `CMakeLists.txt` | ✅ Updated (UI_ADAPTER_SOURCES glob) |

**Constraints Satisfied:**
- ✅ No ECS layout mirrors (state in adapter namespace, not registry)
- ✅ Generated files source of truth
- ✅ Incremental adoption (title screen opt-in)
- ✅ Build-safe (header-only, single implementation site)
- ✅ Zero warnings
- ✅ All tests pass

**Next Steps (Phase 4):**
1. Wire button actions to game events (exit, settings)
2. Generate embeddable headers + adapters for remaining 7 screens
3. Add compile-time feature flag for transition period
4. Deprecate JSON UI system

**References:**
- Fenster's implementation: `.squad/decisions/inbox/fenster-raygui-title-runtime.md` (merged)
- Hockney's validation: `.squad/decisions/inbox/hockney-final-rguilayout-title-runtime.md` (merged)
- Earlier validation: `.squad/decisions/inbox/hockney-validate-rguilayout-title-runtime.md` (superseded)
- Orchestration: `.squad/orchestration-log/2026-04-29T02-45-27Z-*.md`

---

### #188 — rguilayout Integration Path: Custom Template + Adapters (2026-04-28)

**Owners:** Keyser (architecture), Fenster (implementation), Hockney (validation)  
**Status:** APPROVED

rguilayout Phase 2 complete. Established integration architecture using custom embeddable template and thin C++ adapter layer. Generated standalone code cannot compile directly into game (contains `main()` and `RAYGUI_IMPLEMENTATION`), so architecture provides safe, reversible path:

**Architecture: Custom Template + Adapters**
```
content/ui/screens/*.rgl
  ↓ (export with custom template)
app/ui/generated/*.h (header-only, no main, no RAYGUI_IMPLEMENTATION)
  ↓ (called by)
app/ui/adapters/*.cpp (thin C++ wrappers)
  ↓ (called by)
app/systems/ui_render_system.cpp
```

**Key Decision (Keyser):**
- Generate header-only layout functions, not standalone programs
- Adapters call generated functions, translate to game actions
- Incremental screen-by-screen migration
- Preserve JSON runtime during transition

**Implementation (Fenster):**
- Created custom template: `tools/rguilayout/templates/embeddable_layout.h`
- Generated proof-of-concept: `app/ui/generated/title_layout.h` (title screen)
- Proof adapter: `app/ui/adapters/title_adapter.cpp`
- Directory structure: `generated/` for headers, `generated/standalone/` for archived standalone outputs
- Established safety rules: never copy layout data to ECS, one `RAYGUI_IMPLEMENTATION` site, always use embeddable headers

**Important correction (Hockney validation):** rguilayout CLI `--template` flag performs proper variable substitution. Template is functional, not manual scaffold. All 8 screens can be regenerated via CLI. **This supersedes earlier uncertainty about template capability.**

**Validation (Hockney):**
- ✅ Template system works: validated CLI substitution
- ✅ Embeddable header correct: header-only, no ODR hazards, C/C++ compatible
- ✅ Adapter compile-safe: intentionally unwired, not matched by CMake glob
- ✅ Standalone files safely archived in `generated/standalone/`
- ✅ Build passes zero warnings, existing behavior unchanged
- ✅ Temp files cleaned, duplicates removed

**Constraints Satisfied:**
- ✅ No ECS layout mirrors
- ✅ Generated files remain source of truth
- ✅ Incremental adoption
- ✅ Build-safe (header-only avoids ODR)
- ✅ Preserves JSON fallback

**Deferred to Phase 3 (intentional):**
1. raygui include path setup (vcpkg or vendor)
2. Single `RAYGUI_IMPLEMENTATION` site designation
3. CMake wiring for adapters
4. ui_render_system runtime integration (feature-gated)
5. Generate embeddable headers + adapters for remaining 7 screens

All deferred work documented in `tools/rguilayout/INTEGRATION.md` with clear blockers and mitigation strategies.

**Build Integration Status:**
| Component | Status | Location |
|-----------|--------|----------|
| `.rgl` sources | ✅ Complete | `content/ui/screens/*.rgl` (8) |
| Template | ✅ Working | `tools/rguilayout/templates/embeddable_layout.h` |
| Generated embeddable | ✅ Proof | `app/ui/generated/title_layout.h` (1/8) |
| Standalone archive | ✅ Archived | `app/ui/generated/standalone/` |
| Adapter proof | ✅ Safe | `app/ui/adapters/title_adapter.*` |
| CMake wiring | ⏸️ Deferred | Phase 3 |
| Runtime integration | ⏸️ Deferred | Phase 3 |
| Remaining screens | ⏸️ Deferred | Phase 3 |

**References:**
- Integration plan: `RGUILAYOUT_INTEGRATION_PLAN.md`
- Integration guide: `tools/rguilayout/INTEGRATION.md`
- Design spec: `design-docs/raygui-rguilayout-ui-spec.md`
- Orchestration: `.squad/orchestration-log/2026-04-29T00-37-14Z-*.md`

---

### ECS Cleanup Wave: Approval Batch (2026-04-28)

**Owners:** Hockney, Fenster, Baer, Kobayashi, McManus, Keyser, Kujan  
**Status:** APPROVED / IMPLEMENTED

### 2026-04-28T04:59:02Z: User directive
**By:** yashasg (via Copilot)
**What:** Pretty much every entity that moves should have a `Transform` component, and every visible entity should have at least one mesh/render component.
**Why:** User request — captured for team memory


### 2026-04-28T04:59:53Z: User directive
**By:** yashasg (via Copilot)
**What:** If we use `Transform`, we do not need a separate `Vector2` for position; the position of the object can be derived from the transform.
**Why:** User request — captured for team memory


### 2026-04-28T05:01:32Z: User directive
**By:** yashasg (via Copilot)
**What:** Camera is also an entity with a `Transform`, a camera component, and a render target.
**Why:** User request — captured for team memory


### 2026-04-28T05:04:29Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/shape_vertices.h` is not needed because raylib has `Draw<shape>` helpers for this purpose. `app/components/settings.h` is not a component or an entity; it holds audio/settings state and should move out of `app/components/` into an audio/music/settings domain header.
**Why:** User request — captured for team memory


### 2026-04-28T05:05:30Z: User directive
**By:** yashasg (via Copilot)
**What:** `rendering.h` is not a component boundary either; each entity should be tagged with the render-pass number that it is part of.
**Why:** User request — captured for team memory


### 2026-04-28T05:06:10Z: User directive
**By:** yashasg (via Copilot)
**What:** `RingZone` is not a component either; remove the broken ring-zone code for now.
**Why:** User request — captured for team memory


### 2026-04-28T05:07:07Z: User directive
**By:** yashasg (via Copilot)
**What:** `WindowPhase` is not a component; it is just an enum and should belong with the Player entity/state.
**Why:** User request — captured for team memory


### 2026-04-28T05:10:05Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/ui_layout_cache.h` is not needed. It stores fixed HUD/UI element positions derived from JSON, but those positions do not change after the JSON creates the respective UI elements; load once and render the same entities repeatedly instead of keeping a separate layout cache struct.
**Why:** User request — captured for team memory


### 2026-04-28T05:16:51Z: User directive
**By:** yashasg (via Copilot)
**What:** The new Transform component should store a Matrix as the authoritative value, not decomposed position/rotation/scale fields.
**Why:** User request — captured for team memory before Transform implementation work begins.


### 2026-04-28T05:18:52Z: User directive
**By:** yashasg (via Copilot)
**What:** Matrix-backed Transform updates should use raylib/EnTT-friendly helper APIs where appropriate, so movement/gameplay systems do not scatter ad hoc transform math.
**Why:** User request — captured to keep Transform mutation consistent during the entity migration.


### 2026-04-28T05:20:40Z: User directive
**By:** yashasg (via Copilot)
**What:** Transform should use raylib's `Matrix` type and raylib-provided matrix/helper functions. Do not interpret "Matrix" as a custom matrix class or introduce a parallel Transform math abstraction unless raylib helpers are insufficient.
**Why:** User clarification — captured to correct the earlier Transform helper directive before implementation starts.

### 2026-04-28T05:20:40Z: User directive
**By:** yashasg (via Copilot)
**What:** Obstacles should be treated as object entities whose meshes travel together at the same direction and speed. Collision should use the known primary mesh, and meshes should be represented as separate typed mesh components on the obstacle entity rather than a logical root with child mesh entities.
**Why:** User clarification — captured to guide the entity/archetype migration and remove unnecessary root/child obstacle complexity.


### 2026-04-28T05:22:05Z: User directive
**By:** yashasg (via Copilot)
**What:** Once `Transform.matrix` owns position, moving entities should use domain-specific motion data instead of a generic `Velocity` component. Obstacle speed, lane/player state, and other domain systems should drive Transform updates directly through raylib Matrix helpers.
**Why:** User decision — captured before the Position/Velocity migration starts.


### 2026-04-28T05:26:11Z: User directive
**By:** yashasg (via Copilot)
**What:** Player and obstacle entities should use raylib's `Model` as the ECS component when appropriate, because `Model` already contains `transform`, meshes, materials, and related render data. For current generated meshes, entities should own their `Model` by value.
**Why:** User clarification — captured to supersede the earlier typed mesh-component direction before the entity/render migration starts.

### 2026-04-28T05:26:11Z: User directive
**By:** yashasg (via Copilot)
**What:** For visible entities that own a raylib `Model`, `Model.transform` is the authoritative transform. Do not keep a separate gameplay `Transform` component that must be copied/synced into `Model.transform` for those entities.
**Why:** User decision — captured to prevent duplicate transform sources during the Matrix/Model migration.


### 2026-04-28T05:30:22Z: User directive
**By:** yashasg (via Copilot)
**What:** Entity-owned raylib `Model` values should use session/resource-pool lifetime for GPU/resource ownership; Models are unloaded at level/session shutdown rather than per entity destruction.
**Why:** User decision — captured before implementing Model components to avoid accidental double-unload or per-entity GPU churn.

### 2026-04-28T05:30:22Z: User directive
**By:** yashasg (via Copilot)
**What:** Render-pass membership should use empty tag components (for example world/HUD/effects tags) rather than a compact runtime `RenderPass { uint8_t pass; }` component.
**Why:** User decision — captured to guide the rendering component split away from `rendering.h` as a component bucket.


### 2026-04-28T05:32:50Z: User directive
**By:** yashasg (via Copilot)
**What:** Correction to Model lifetime: raylib `Model` values owned by ECS entities should be unloaded when the owning entity is destroyed, not deferred to session/resource-pool shutdown.
**Why:** User revised the earlier lifetime decision — captured so Model component implementation uses entity destruction cleanup and avoids stale resource-pool guidance.


### 2026-04-28T05:34:48Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/audio.h` and `app/components/music.h` should not exist as component-boundary wrappers when raylib already provides `Sound` and `Music` structs that serve the purpose.
**Why:** User request — captured to guide cleanup away from unnecessary audio/music component wrappers and toward raylib-native data.


### 2026-04-28T05:36:50Z: User directive
**By:** yashasg (via Copilot)
**What:** Do not use `LoadModelFromMesh` for obstacle Models. Obstacles are a combination of three meshes, so their raylib `Model` should be built with a populated `meshes` array instead.
**Why:** User correction — captured to prevent one-mesh Model construction from driving the obstacle Model migration.


### 2026-04-28T05:41:40Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/obstacle_counter.h` and `app/components/obstacle_data.h` are not components. Move their contents to the obstacle entity boundary and remove those files.
**Why:** User request — captured to continue component-boundary cleanup and keep only real ECS components in `app/components/`.


### 2026-04-28T05:42:57Z: User directive
**By:** yashasg (via Copilot)
**What:** The current cleanup pass should include the first Model/Transform entity migration slice, not only safe component-boundary rehomes.
**Why:** User scope decision — captured to route implementation beyond header cleanup once review gates allow it.


### 2026-04-28T05:43:35Z: User directive
**By:** yashasg (via Copilot)
**What:** For obstacle entities with a three-mesh raylib `Model`, store explicit obstacle part descriptors on the entity rather than only recomputing and baking slab dimensions/offsets into generated meshes.
**Why:** User design decision — captured to guide the Model/Transform obstacle migration and collision/render data ownership.


### 2026-04-28T07:37:17Z: User directive
**By:** yashasg (via Copilot)
**What:** Do not add new component cleanup surfaces like `app/components/render_tags.h` during this cleanup pass; the pass is about removing/consolidating ECS/component clutter, not adding more component headers.
**Why:** User request — captured for team memory


### 2026-04-28T07:37:17Z: User directive
**By:** yashasg (via Copilot)
**What:** Finish removing/consolidating the existing bad component surfaces before adding any new ECS/component headers or infrastructure. The cleanup pass is about deleting/folding clutter, not introducing more.
**Why:** User request — captured for team memory


### 2026-04-28T07:37:17Z: User directive
**By:** yashasg (via Copilot)
**What:** Components mean something: do not create a class/struct/component just because it is convenient. A component must have a clear ECS/entity-data reason and belong to the entity model.
**Why:** User request — captured for team memory


### 2026-04-28T07:43:03Z: User directive
**By:** yashasg (via Copilot)
**What:** Cleanup must produce concrete deletions and an entities layer: define entities, remove components, clean systems tied to removed components, and delete dead/duplicate archetype surfaces. Audits alone do not satisfy this pass.
**Why:** User request — captured for team memory


### 2026-04-28T07:45:00Z: User directive
**By:** yashasg (via Copilot)
**What:** Finish the current PR. The Model.transform switch scope is narrow: only the two model-using entities need to use Model.transform. Do not expand it into an open-ended rendering migration.
**Why:** User request — captured for team memory


### 2026-04-28T08:19:17Z: User directive
**By:** yashasg (via Copilot)
**What:** The ECS cleanup is not done. Next suspicious component headers include high_score.h, text.h, ui_state.h, and window_phase.h; continue removing/folding random non-component surfaces instead of treating the prior approval as complete.
**Why:** User request — captured for team memory


### 2026-04-28T08:32:38Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/camera.h` is not needed as a component header. Represent cameras as two entities: `game_camera` and `ui_camera`.
**Why:** User request — captured for team memory


### 2026-04-28T08:49:58Z: User directive
**By:** yashasg (via Copilot)
**What:** Always inspect `docs/entt/` before answering EnTT usage/API questions or making EnTT-related recommendations.
**Why:** User request — captured for team memory


### 2026-04-28T09:13:54Z: User directive
**By:** yashasg (via Copilot)
**What:** `beat_map_loader` is not a system; it is utility/JSON wrapper code. `cleanup_system` is not a system boundary; cleanup should happen on obstacles when they pass the camera's Z position.
**Why:** User request — captured for team memory during ECS/system cleanup.


### 2026-04-28T09:16:14Z: User directive
**By:** yashasg (via Copilot)
**What:** Systems should be logic run every frame or multiple times per frame, like collision. Input helper fragmentation (`hit_test_system`, `input_dispatcher`, `input_gesture`, `input_system`) is excessive; use raylib gesture helpers where appropriate. `level_select_system` is not a system; it is UI element click handling.
**Why:** User request — captured for ECS/system boundary cleanup.


### 2026-04-28T09:17:18Z: User directive
**By:** yashasg (via Copilot)
**What:** Use raylib helper functions for input and gestures; do not spin up custom project input/gesture code when raylib already provides the behavior.
**Why:** User request — captured as a hard constraint for input/system cleanup.


### 2026-04-28T09:24:43Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/systems/lifetime_system.cpp` and `app/components/lifetime.h` are pointless for obstacles. Obstacle lifetime ends when it passes the camera's Z position; it should either be tagged for destroy or destroyed in scroll/despawn logic.
**Why:** User request — captured for ECS/system cleanup.


### 2026-04-28T09:27:58Z: User directive
**By:** yashasg (via Copilot)
**What:** Particles and popups have fixed lifetimes and should be destroyed when that fixed lifetime runs out. Do not keep a special generic `Lifetime` component just to store that value; it can be a float owned by the particle/popup data.
**Why:** User request — captured for lifetime component/system removal.


### 2026-04-28T09:29:00Z: User directive
**By:** yashasg (via Copilot)
**What:** Continue system cleanup until `app/systems/` contains real frame/tick systems. UI loaders/navigation/source resolution/button spawning are at least utilities, if not dead code; clean them up rather than stopping after audits.
**Why:** User request — captured as overnight cleanup directive.


### 2026-04-28T09:33:22Z: User directive
**By:** yashasg (via Copilot)
**What:** Do not blindly commit changes. Push cleanup work to a PR and validate that builds are successful.
**Why:** User request — captured as delivery gate for ongoing cleanup.


# Decision: Rename cleanup_system → obstacle_despawn_system in benchmarks

**Author:** Hockney (Platform Engineer)  
**Date:** 2025-07-15  
**File touched:** `benchmarks/bench_systems.cpp`

## Context

Baer identified a compile blocker: `benchmarks/bench_systems.cpp` still called `cleanup_system(reg, DT)` at three sites after the system was renamed to `obstacle_despawn_system` during the system-boundary cleanup.

## Decision

Replace all three call sites and the corresponding `TEST_CASE` label string with the new name `obstacle_despawn_system`. No other files were modified.

## Verification

Post-edit grep across `app/`, `tests/`, and `benchmarks/` confirms zero remaining `cleanup_system(` occurrences.


# Decision: HighScoreState/SettingsState helpers live in persistence namespaces

**Date:** 2026-04-27  
**Issue:** #286  
**Author:** Hockney

## Decision
Business logic helpers for `SettingsState` and `HighScoreState` are placed as free functions in `namespace settings` (settings_persistence.h) and `namespace high_score` (high_score_persistence.h) respectively, NOT on the structs as methods.

## Rationale
- Keeps structs as plain data containers per project convention.
- Persistence namespaces already own the helpers for clamping/saving/loading; helper functions are a natural extension.
- Avoids coupling data layout to behaviour, making it easier to evolve ownership independently.

## Affected symbols
- `settings::audio_offset_seconds(const SettingsState&)`
- `settings::ftue_complete(const SettingsState&)`
- `high_score::make_key_str/make_key_hash/get_score/get_score_by_hash/set_score/set_score_by_hash/ensure_entry/get_current_high_score`


# Decision: text.h and ui_state.h removed from app/components/

**Date:** 2026-05-01  
**Author:** Hockney

## What changed
- `app/components/text.h` deleted. Its content (`TextAlign`, `FontSize`, `TextContext`) moved inline into `app/systems/text_renderer.h`.
- `app/components/ui_state.h` deleted. Its content (`ActiveScreen`, `UIState`) moved inline into `app/systems/ui_loader.h`.

## Rationale
Neither was a true ECS entity component. `TextContext` and `UIState` are both stored in `reg.ctx()` (registry context), not attached to entities. They belong with the systems that own them.

## Impact
- All consumers updated: `ui_element.h`, `scoring.h`, `ui_render_system.cpp`, `ui_navigation_system.cpp`, and 5 test files.
- Include path for text types is now `systems/text_renderer.h`; UI state types are now via `systems/ui_loader.h`.
- Zero-warning build. All tests pass (2983 assertions).

## Note for Kujan
No evidence remains to justify keeping these in `app/components/`. This is closed.


# Platform Plan: Moving Non-System Utilities out of app/systems/

**Author:** Hockney  
**Date:** 2026-05  
**Status:** READ-ONLY PLAN — do not implement while Keyser resolves review blockers  
**Scope:** Mechanical safety analysis only. No code changes.

---

## 1. CMake Glob Behavior

The CMakeLists.txt uses four **ungated** `file(GLOB)` calls for source:

```cmake
file(GLOB SYSTEM_SOURCES  app/systems/*.cpp)
file(GLOB UTIL_SOURCES    app/util/*.cpp)
file(GLOB ENTITY_SOURCES  app/entities/*.cpp)
file(GLOB ARCHETYPE_SOURCES app/archetypes/*.cpp)
```

**None of these have `CONFIGURE_DEPENDS`** (Issue #55 — source globs; asset globs were fixed in #173 but source globs were not). This means:

- Moving a `.cpp` from `app/systems/` to `app/util/` will **not** be detected automatically.
- After the file move, a **forced CMake reconfigure** (`cmake -B build -S .`) is required before the next build.
- Without reconfigure, the old glob has a stale entry and the new glob doesn't know about the file yet — the file silently disappears from the build.
- **Header-only files (`.h`) are never caught by any glob.** Moves of `.h` files have zero CMake impact.

### 2026-04-28: Component boundary rule — obstacle_data + obstacle_counter cleanup

**By:** Keaton
**What:** Deleted `app/components/obstacle_data.h` and `app/components/obstacle_counter.h`.
- `RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction` moved into `app/components/obstacle.h` (entity ECS components belong alongside their sibling entity data)
- `ObstacleCounter` moved to `app/systems/obstacle_counter_system.h` (context singleton, not entity data)

**Rule established:** `app/components/` is exclusively for types emplaced onto entities via `reg.emplace<T>()`. Registry context singletons (`reg.ctx().emplace<T>()`) live in system headers beside their wiring code.

**Validation:** Zero compiler warnings. All 862 test cases pass.


# Code Removal Estimate: Component Consolidation & Entity Refactor

**Author:** Keaton (C++ Performance Engineer)  
**Date:** 2026-04-28  
**Status:** READ-ONLY AUDIT (no edits made)  
**Scope:** File-level C++ audit identifying removals + moves + renames (LOC savings where possible)

---

## Executive Summary

Estimated **~485 LOC can be cleanly removed** (three deletable categories) + **~120 LOC moves** (component boundary fixes that reduce misuse but preserve LOC). High-risk removals (rendering refactor, Transform vs Position/Velocity replacement) require architectural decisions beyond component cleanup.

---

## CATEGORY 1: TRUE DELETIONS (Safe, Verified)

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

### #SQUAD Comment Cleanup (2026-04-27)

**Owners:** Keaton (implementation), Kujan (review)  
**Status:** APPROVED / IMPLEMENTED

### 2026-04-27 — Verbal: test cleanup for burnout removal (#239)

**Decision:** Delete `test_burnout_system.cpp` and `test_burnout_bank_on_action.cpp` entirely (not stub them) since CMakeLists uses `file(GLOB TEST_SOURCES tests/*.cpp)` — deleted files are automatically excluded.

**Decision:** `test_helpers.h :: make_registry()` still emplace `BurnoutState` because `test_haptic_system.cpp`, `test_death_model_unified.cpp`, and `test_components.cpp` still reference it. Removing it now would break those tests before McManus removes the component header. McManus/Hockney own that cleanup.

**Decision:** Added `[no_burnout]` tag to the new no-penalty test so future runs can target it in isolation.

**Out-of-scope flag:** `test_haptic_system.cpp` has 5 `burnout_system()` call sites (lines 75, 87, 99, 112, 125) that will break when McManus removes `burnout_system`. These need a follow-up task for Hockney or McManus.

---

### 2026-04-27T00:04:33-07:00: User directive
**By:** yashasg (via Copilot)
**What:** For the DoD pass, use multiple DoD subagents, create GitHub issues for every surfaced issue, and treat no issue as too small to be fixed.
**Why:** User request — captured for team memory

### 2026-04-27T00:04:34-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Keep looping with DoD passes using subagents until no new issues come up; once clear, add new tests and improve coverage/edge-case testing.
**Why:** User request — captured for team memory

### 2026-04-27T00:04:35-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Do not skip any system or component in the DoD audit.
**Why:** User request — captured for team memory

### 2026-04-27T00:52:58-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Systems should not handle responsibilities outside their purpose; creating new focused systems is acceptable when it improves clean, readable code.
**Why:** User request — captured for team memory

### 2026-04-27T02:36:17-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Prioritize DoD-related TestFlight issues first, then run a cleanup/readability pass.
**Why:** User request — captured for team memory

### 2026-04-27T02:40:40-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Run multiple DoD fixes in parallel; most systems should function independently, so parallel ownership should be used where file/system boundaries do not conflict.
**Why:** User request — captured for team memory

### 2026-04-27T02:40:41-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Once DoD-related issues are resolved, go through the remaining ECS refactor issues and chip away at those next.
**Why:** User request — captured for team memory

### 2026-04-27T02:46:00-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Run up to 32 agents in parallel for independent DoD/ECS fixes; do not hold back on parallel subagent fixes when systems are file/system independent.
**Why:** User request — captured for team memory

### 2026-04-27T12:04:35-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Use EnTT's model for the input implementation; move away from the custom hand-rolled input event queue where EnTT dispatcher/signal patterns apply.
**Why:** User request after comparing current input pipeline against `docs/entt/EnTT_In_Action.md`.

### 2026-04-27T16:25:47-07:00: User directive
**By:** yashasg (via Copilot)
**What:** First create the P0 `ecs_refactor` TestFlight issue for auditing/consolidating dead or duplicate ECS systems/components; validate and review the existing archetype move to `app/archetypes/`; then stop and report status before starting any P2/P3 fixes.
**Why:** User request — captured for team memory

### 2026-04-27T20:52:52Z: User directive
**By:** yashasg (via Copilot)
**What:** Remove the LanePush feature for now; it can be added back later if needed. This should be tracked as a GitHub issue but does not need to be addressed in the ECS refactor. LanePush touches the editor, level builder, and game, so it is a large cross-system change and must be addressed that way.
**Why:** User request — captured for team memory

### 2026-04-27T21:17:52Z: User directive
**By:** yashasg (via Copilot)
**What:** Once fixes are rebased/integrated, clean up old branches and maintain `user/yashasg/ecs_refactor` as the source of truth.
**Why:** User request — captured for team memory

### 2026-04-27T23:16:58Z: User directive
**By:** yashasg (via Copilot)
**What:** Archetypes should live in their own folder at `app/archetypes`.
**Why:** User request — captured for team memory

### Archetype Candidate Audit (2026-04-27)

**Owners:** Keyser (ECS/archetype analysis), Keaton (duplicate audit), Coordinator (routing)  
**Status:** AUDIT COMPLETE — findings ranked for implementation

Read-only audit of `app/systems/` and `tests/test_helpers.h` identified entity construction patterns suitable for extraction into `app/archetypes/` factories or construction helpers.

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

### copilot-directive-2026-04-27T00-04-33-dod-audit

### copilot-directive-2026-04-27T00-04-34-dod-loop-tests

### copilot-directive-2026-04-27T00-04-35-no-skip-dod

### copilot-directive-2026-04-27T00-52-58-system-boundaries

### copilot-directive-2026-04-27T02-36-17-0700

### copilot-directive-2026-04-27T02-40-40-0700

### copilot-directive-2026-04-27T02-40-41-0700

### copilot-directive-2026-04-27T02-46-00-0700

### copilot-directive-2026-04-27T12-04-35-0700-entt-input-model

### 2026-04-27T17:02:41-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Do not create new branches or worktrees for this Ralph/#344 work; work off the current branch.
**Why:** User request — captured for team memory


### 2026-04-27T17:06:37-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Prioritize issues focused on removing dead/duplicate code and consolidating systems/entities so there is less code to traverse and fix later.
**Why:** User request — captured for team memory and Ralph issue selection


### 2026-04-27T17:06:37-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Ralph can run multiple sub-agents to speed up workflow, as long as work remains on the current branch and avoids unsafe overlap.
**Why:** User request — captured for team memory


### 2026-04-27T17:20:10.991-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Focus on fixing all `ecs_refactor` bugs with nothing left behind. Once everything is resolved, move `.squad/`, `design-docs/`, and `docs/` to a different branch and PR.
**Why:** User request — captured for team memory


### 2026-04-27T17:21:00-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Make sure changes are validated by running tests.
**Why:** User request — captured for team memory


### 2026-04-27T17:22:00-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Monitor the PR build status and prioritize fixing PR build failures before returning to resolving `ecs_refactor` issues.
**Why:** User request — captured for team memory


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

### 2026-04-26T22:06:19-07:00: User directive
**By:** yashasg (via Copilot)
**What:** The enum refactor should not use the current X-macro list pattern because it is too cumbersome; use a macro where the enum name is an input.
**Why:** User request — captured for team memory and implementation constraints.

### 2026-04-26T22:20:24-07:00: User directive
**By:** yashasg (via Copilot)
**What:** Remove the burnout system altogether because it gets in the way of staying-on-beat gameplay; changing shape on beat should not be penalized even when it is not close to obstacle arrival.
**Why:** User request — current burnout behavior is breaking functionality and docs must be updated.

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

### User Model Directives (2026-04-26)

**Owners:** yashasg (via Copilot)  
**Status:** CAPTURED

1. **Default model:** claude-sonnet-4.6
2. **Exceptions:** Redfoot, Saul, Rabin, Edie → claude-opus-4.7
3. **Scribe, Ralph:** claude-haiku-4.5
4. **Issue creation:** During diagnostics, create an issue for whatever the team sees; do not skip based on perceived priority

---

### copilot-directive-2026-04-26T22-06-enum-macro

### copilot-directive-2026-04-26T22-20-burnout-removal

