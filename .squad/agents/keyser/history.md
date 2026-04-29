# Keyser — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

## Session: 2026-04-28T22:35:09Z — Architecture Spine and Boundary Decisions Consolidated

**Context:** Scribe merged all inbox decisions (including keyser-raygui-migration-spine.md, keyser-ui-layout-data-boundary.md) into decisions.md and updated cross-agent history.

**Your work:** Keyser authored Architecture Spine for UI Migration (problem statement, goals, non-goals, hard boundary rules U1–U7), and finalized rguilayout Export-Data Boundary decision. Rejected prior recommendations: no `RaguiAnchors<ScreenTag>` / layout POD duplication, no UI widget entities for static menus/HUD, Phase 5 deletes layout caches instead of rebuilding. Approach rings removed from scope per user directive.

**Status:** Spine document and boundary decisions now consolidated in decisions.md. Ready for Phase 3 handoff with clear ECS/render boundaries established.

**Related:** Merged decisions in `.squad/decisions.md` (Architecture Spine section and UI Layout Data Boundary section)

### 2026-04-28 — rguilayout Integration Spine (#0, user-requested)

**Status:** SPINE DEFINED — Integration path established; ready for tooling handoff.

**Context:** Phase 2 complete: all 8 screens have `.rgl` sources and generated `.c/.h` files. But generated files are standalone programs (`main()` + `RAYGUI_IMPLEMENTATION`) that cannot compile into the game. User asked: "setup an integration path."

**Problem:** Cannot integrate generated code as-is — ODR violations, symbol collisions, and presence of `main()` in each `.c` file.

**Decision: Custom Template + Adapter Layer**

Integration path: `.rgl` → (custom template) → `app/ui/rgl_generated/*.h` (header-only layout functions) → `app/ui/rgl_adapters/*.cpp` (C++ adapters) → `ui_render_system.cpp`

**Key constraints satisfied:**
- ✅ No ECS layout mirrors (adapters call generated functions; no layout copied into components)
- ✅ Generated files own layout data (never duplicated into project structs)
- ✅ Incremental migration (screen-by-screen with parallel runtime paths)
- ✅ Build-safe (header-only avoids ODR; `RAYGUI_IMPLEMENTATION` stays singular)
- ✅ Preserves JSON path (unmigrated screens keep existing system)

**File structure:**
- `app/ui/rgl_generated/*.h` — header-only layout functions (8 screens)
- `app/ui/rgl_adapters/*.cpp` — hand-written C++ adapters (added incrementally)
- **Deletes:** `app/ui/*.c`, `app/ui/*.h` (current standalone generated files)
- **No CMake changes:** Adapters auto-globbed by existing `file(GLOB UI_SOURCES app/ui/*.cpp)`

**Custom template requirements:**
1. No `main()` — just layout drawing functions
2. No `RAYGUI_IMPLEMENTATION` — raygui already implemented elsewhere in game
3. Header-only — pure inline or extern "C" callable
4. Named control accessors — expose button states, rectangles for dynamic content

**Migration strategy:**
1. Screen-by-screen with feature flags
2. Migrated screens call rgl adapter; unmigrated use JSON
3. Validate behavior parity per screen before removing JSON dependency

**Open issues:**
1. raygui implementation location — grep found no current `RAYGUI_IMPLEMENTATION` in app/ (only in standalone generated files). Must add exactly one implementation site.
2. Custom template authoring — tooling task, not architecture.
3. Dynamic text binding (scores, song names) — template must export slots/rectangles.

**Next steps:**
1. Create custom template: `tools/rguilayout/templates/shapeshifter_header.rgl`
2. Proof of concept: re-export title screen, write adapter, wire into ui_render_system
3. Iterate for remaining screens

**Decision written:** `.squad/decisions/inbox/keyser-rguilayout-integration-spine.md`

**Impact:** Establishes the runtime seam that respects all prior architecture constraints (no layout mirrors, generated files own data, incremental migration, build-safe). Unblocks Phase 3 build integration.

## 2026-04-29: Assigned — c7700f8 UI Adapter Abstraction Pattern Design

**Date:** 2026-04-29T03:13:21Z  
**Task:** Design template/trait-based abstraction pattern for rguilayout UI adapters  
**Context:** Keaton review of c7700f8 revealed 377-line boilerplate duplication across 8 adapter files  
**Priority:** HIGH  
**Status:** ASSIGNED

### Assignment Scope

Keyser (Lead Architect) is assigned to design an abstraction pattern that eliminates boilerplate duplication across 8 UI screen adapter files (game_over, song_complete, paused, settings, tutorial, gameplay, level_select, title).

### Problem Statement

c7700f8 introduces identical init/render/state patterns repeated per-screen:
```cpp
namespace {
    {Screen}LayoutState {screen}_layout_state;
    bool {screen}_initialized = false;
}
void {screen}_adapter_init() { /* identical */ }
void {screen}_adapter_render(entt::registry& reg) { /* identical */ }
```

This violates user directive: "maximize code reuse, no slop."

### Design Requirements

1. **Abstract shared lifecycle**
   - Anonymous namespace state initialization
   - Guard-checked init function
   - Render entrypoint with lazy init

2. **Support per-adapter customization**
   - Dispatch logic varies by screen (button handlers, timers, etc.)
   - Pattern must allow screen-specific behavior without duplication

3. **Preserve generated header interface**
   - Must use existing `{Screen}Layout_Init()` and `{Screen}Layout_Render(&state)` functions
   - No changes to `.rgl` export workflow

4. **Compile-time enforcement**
   - Use C++17 templates or CRTP for type safety
   - Wrong function signature = compiler error

### Recommended Approaches

**Option A: Non-type Template Parameters (Simplest)**
- Best for stateless init/render functions
- 377 lines → ~50 template + 8 using-declarations
- Zero runtime overhead

**Option B: CRTP + Traits (More Flexible)**
- Better for per-adapter state/dispatch customization
- Trait contract: State typedef, init(), render_layout(), dispatch() methods
- Clear pattern for future adapter implementations

**Option C: Hybrid**
- Template base for init/render lifecycle
- CRTP for dispatch customization

### Deliverables Expected

1. **Pattern design document** (to `.squad/decisions/inbox/keyser-ui-adapter-pattern.md`)
   - Chosen approach and rationale
   - Template signatures / trait contract
   - Example specializations (2–3 adapters)

2. **Proof-of-concept refactor**
   - Refactored 2–3 adapters (game_over, title, gameplay) using chosen pattern
   - Demonstrates elimination of duplication
   - Validates generated header interface compatibility

3. **Implementation handoff spec**
   - Clear steps to refactor remaining 5 adapters
   - Assign implementation to non-Fenster agent

### Blocking Considerations

- Fenster is locked out (per review protocol); cannot execute refactor
- Hockney has already approved platform concerns
- McManus (Gameplay) can proceed independently on dispatch logic

### Timeline

- **Design:** 1–2 hours
- **Proof-of-concept:** 1–2 hours
- **Implementation (assigned agent):** 2–4 hours
- **Re-review (Keaton):** 1 hour
- **Total cycle:** ~5–9 hours

### Related Decisions

- User directive: `.squad/decisions.md` ("User Directive: Maximize Code Reuse, No Slop")
- UI adapter abstraction directive: `.squad/decisions.md` ("Directive: UI Adapter Boilerplate Abstraction Pattern")
- Keaton review: `.squad/orchestration-log/2026-04-29T03:13:21Z-keaton.md`

### Orchestration Log

See `.squad/orchestration-log/2026-04-29T03:13:21Z-keyser.md`


### 2026-04-29 — RGUILayout Adapter Template Refactor (Keaton Review Revision)

**Status:** COMMITTED (958a7d9) — Architectural refactor of c7700f8 after Keaton rejection.

**Scope:** Replaced 377 lines of duplicated adapter boilerplate across 8 screen adapters with template-based pattern and shared helpers.

**Changes implemented:**

1. **Created `adapter_base.h` with `RGuiAdapter` template** — Uses C++17 auto template parameters (`template<typename State, auto InitFunc, auto RenderFunc>`) for compile-time dispatch to generated layout functions. Eliminates manual state variable + bool flag + init guard pattern repeated across 8 files. Each adapter now declares a type alias (`using TitleAdapter = RGuiAdapter<...>`) and single static instance.

2. **Created `end_screen_dispatch.h` template helper** — Consolidates byte-for-byte identical dispatch logic from `game_over_adapter.cpp` and `song_complete_adapter.cpp` (Restart/LevelSelect/MainMenu button → `EndScreenChoice` enum). Generic over any layout state with the three button-pressed fields.

3. **Fixed EnTT storage access pattern** — Removed cargo-cult `std::as_const(reg).storage<T>()` wrapping in `ui_render_system.cpp` lines 363–364, 391–392. Replaced with direct `reg.try_get<T>(entity)` per-entity queries matching codebase conventions (collision_system, scoring_system, test_player_system all use `try_get`). EnTT v3.16.0 `storage()` returns a reference, not a pointer; the `std::as_const` wrapper does not prevent pool creation and is unnecessary.

4. **Added `offset_rect()` helper** — Small raylib-friendly utility for anchor-relative Rectangle construction (`offset_rect(anchor, x, y, w, h)`) used in `settings_adapter.cpp` to replace 3 instances of manual `(Rectangle){anchor.x + x, anchor.y + y, w, h}` arithmetic.

5. **Renamed adapter instances for unity build safety** — Each adapter's static instance now has a unique name (`game_over_adapter`, `title_adapter`, etc.) instead of generic `adapter`. Avoids symbol collision when multiple adapters are merged into a single translation unit by CMake unity builds.

**Build/test validation:**
- Zero-warning build with Clang on macOS (arm64-osx triplet, unity build enabled).
- All 901 test cases pass (2635 assertions).
- Verified no `app/ui/generated/standalone/*.c` files are compiled (only `app/ui/raygui_impl.cpp` is the real `RAYGUI_IMPLEMENTATION` site).

**Patterns established:**

- **RGUILayout adapter pattern:** Any new screen adapter should use `RGuiAdapter<StateType, &InitFunc, &RenderFunc>` type alias + named static instance, not manual init/render boilerplate.
- **EnTT optional component access:** Use `reg.try_get<T>(entity)` per-entity checks, not `storage<T>()` + contains + get. The latter is EnTT v2/v3-early API surface; v3.16.0 `try_get` is idiomatic.
- **Unity build compatibility:** Static variables in anonymous namespaces must have unique names when file-scoped uniqueness is insufficient (adapters are likely to be batched together).

**Key lesson:** Template-based DRY elimination in C++17 is straightforward when the generated output is uniform (all adapters follow same init/render contract). The cost is one template definition; the benefit scales linearly with the number of conforming types. This pattern can extend to other generated-code adapter layers (audio event callbacks, beatmap parsers, etc.) if similar boilerplate emerges.

**Decision written:** None required — this is a revision of c7700f8 per Keaton's architectural requirements, not a new team decision.

---

## 2026-04-29 — UI Adapter Template Refactor (Commit 958a7d9) — APPROVED

**Context:** Revision of rejected c7700f8 (code duplication in 8 UI adapters).

**Challenge:** Keaton's rejection identified 377 lines of duplicated boilerplate init/render patterns across 8 adapters, plus byte-for-byte identical end-screen dispatch logic, manual rectangle construction, and incorrect EnTT API usage.

**Solution:** 
- Created `adapter_base.h` with `RGuiAdapter<LayoutState, InitFunc, RenderFunc>` C++17 template
- Created `end_screen_dispatch.h` with shared button dispatch helper
- Refactored all 8 adapters to use template (reduced ~45 LOC → ~25-35 LOC per adapter)
- Fixed ui_render_system.cpp: removed std::as_const cargo-cult pattern, replaced with idiomatic `reg.try_get<T>(entity)`
- Added `offset_rect()` helper to eliminate manual Rectangle arithmetic

**Metrics:**
- 377 lines of duplication eliminated (~33% reduction)
- All 8 adapters now use reusable template
- Zero warnings, all tests pass, no behavior changes

**Approvals:**
- ✅ Keaton (code reviewer): Exemplary modern C++ implementation, all rejection criteria resolved
- ✅ Hockney (platform engineer): Zero-warning unity build, all tests pass, RAYGUI_IMPLEMENTATION invariant preserved, no ODR violations

**Outcome:** Ready for merge to trunk. PR #351 (origin/ui_layout_refactor) will be pushed after logging.

**Skills Created:** `.squad/skills/cpp-template-adapter/`, `.squad/skills/unity-build-template-safety/`

## Learnings

- Gameplay HUD restoration should read live `ShapeButtonTag + ShapeButtonData + UIPosition` entities first, then fall back to layout-derived defaults; this keeps visible buttons aligned with actual input-hit targets.
- Keep gameplay HUD dynamic behavior (shape buttons, approach rings, segmented energy feedback) inside `gameplay_hud_screen_controller.cpp` while leaving generated rguilayout header focused on static controls like pause.
- Compact, multi-stop segmented energy bars with critical pulse + flash overlays better preserve readability on 720x1280 than oversized single-fill bars.
- Root UI cleanup (post-rguilayout activation): classified `ui_loader` + `text_renderer` + `ui_button_spawner` as live runtime dependencies, `ui_source_resolver` as live test-only dependency, and `tests/test_ui_spawn_malformed.cpp` as dead legacy spawn-surface documentation.
- Applied surgical cleanup: removed `tests/test_ui_spawn_malformed.cpp`, excluded `app/ui/ui_source_resolver.cpp` from `shapeshifter_lib` runtime build while compiling it directly into `shapeshifter_tests`, and updated root UI comments to reflect the active screen-controller path.
- Key files changed: `CMakeLists.txt`, `app/ui/ui_loader.cpp`, `app/ui/ui_source_resolver.h`, deleted `tests/test_ui_spawn_malformed.cpp`.
- Validation: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass: 867 test cases).
- Second-pass dead-surface cleanup: deleted test-only `app/ui/ui_source_resolver.*`, removed legacy `app/components/ui_element.h`, and retired resolver-only tests (`tests/test_ui_source_resolver.cpp` plus resolver assertions in game-state/high-score/redfoot tests) because runtime now renders via screen controllers without JSON dynamic-source resolution.
- Kept live runtime dependencies untouched (`ui_loader`, `text_renderer`, `ui_button_spawner`, level-select + screen controllers, navigation/render systems) and refreshed stale loader/render comments to reflect the active rguilayout controller path.
- Validation and dead-surface proof: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` passed (848 test cases); no app/tests/CMake references remain to `ui_source_resolver`, legacy `UIElement*` components, `spawn_ui_elements`, `app/ui/vendor`, or generated standalone exports.

### 2026-04-29T08:05:08Z — Root UI Surface Cleanup & Test-Only Component Removal (Session)

**Tasks:**
1. `keyser-root-ui-cleanup` — First-pass audit of root `app/ui/` files; classified live dependencies vs. test-only vs. dead
2. `keyser-test-only-ui-leftovers` — Second-pass cleanup; deleted runtime-dead `ui_source_resolver.*`, `ui_element.h`, legacy tests; kept all live dependencies

**Outcome:** ✅ Completed
- Identified live runtime: `ui_loader`, `text_renderer`, `ui_button_spawner`, `level_select_controller`, all 8 screen controllers
- Moved `ui_source_resolver.cpp` to test sources (test-only)
- Deleted `ui_element.h` and `test_ui_spawn_malformed.cpp` (no runtime use)
- Updated stale comments and empty vendor dir notes
- Validated native + unity builds pass (867 tests, zero warnings)

**Orchestration logs:** 
- `.squad/orchestration-log/2026-04-29T08:05:08Z-keyser-first-pass.md`
- `.squad/orchestration-log/2026-04-29T08:05:08Z-keyser-second-pass.md`
