# Hockney — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Platform Engineer
- **Joined:** 2026-04-26T02:07:46.546Z

## 2026-04-29 — Settings Gear Click Fix + Standalone UI Cleanup + Vendored raygui Removal

### Settings Gear Click Reliability
Fixed title screen settings button (bottom-right gear, `#142#`) unresponsiveness under letterbox scaling.

**Root cause:** raygui hit-testing used unadjusted window coordinates; UI renders in virtual 720×1280 space.

**Solution:** Applied `SetMouseOffset(-ScreenTransform.offset)` + `SetMouseScale(1 / ScreenTransform.scale)` around screen-controller rendering in `ui_render_system`, restored defaults immediately after. Canonized pattern for all future raygui controls.

**Files modified:** `app/systems/ui_render_system.cpp`, `app/ui/screen_controllers/title_screen_controller.cpp`, `app/ui/generated/title_layout.h`.

**Validation:** 867 test cases, 2603 assertions, zero warnings. Settings navigation regression test added (`test_game_state_extended.cpp`).

**Status:** ✅ APPROVED (Kujan)

### Standalone UI Export Cleanup
Deleted 17 committed standalone rguilayout exports from `app/ui/generated/standalone/`. These were dead surface; runtime UI now uses embeddable headers + screen controllers.

**Policy formalized:** Scratch exports only under `build/rguilayout-scratch/` (auto-ignored). Docs + tooling updated to enforce. Active paths (`content/ui/screens/*.rgl`, `app/ui/generated/*_layout.h`, `app/ui/screen_controllers/`) untouched.

**Validation:** Zero build/runtime reference breakage. All active files preserved.

**Status:** ✅ APPROVED (Kujan)

### Vendored raygui Removed; vcpkg Integration Complete
Completed user directive to stop committing vendored raygui. Removed `app/ui/vendor/raygui.h` and integrated vcpkg-provided raygui throughout build system.

**Changes:**
- Added `raygui` to `vcpkg.json` dependencies
- Updated CMake to resolve raygui via `find_path()` and apply as SYSTEM include on `shapeshifter_lib`
- Switched all UI controllers and `app/ui/raygui_impl.cpp` to `#include <raygui.h>` (system include)
- Retained `app/ui/raygui_impl.cpp` as minimal project-owned TU to own single RAYGUI_IMPLEMENTATION definition

**Validation:** 867 test cases, 2603 assertions, zero warnings.

**Status:** ✅ APPROVED (Kujan)

### app/ui Root-Level Files Retention Audit
Conducted comprehensive audit of `app/ui/*.cpp/.h` root-level files. Confirmed all are active and necessary for the build:
- `raygui_impl.cpp` — sole RAYGUI_IMPLEMENTATION TU
- `text_renderer.*` — used by game_loop and ui_render_system
- `ui_loader.*` — powers screen JSON loading and layout cache builders
- `ui_source_resolver.*` — used by UI tests and game-state validation
- `level_select_controller.*` — wired into input_dispatcher and level-select tests
- `ui_button_spawner.h` — used by game_state_system, game_loop, routing, and hitbox tests

**Decision:** Do not delete any current root-level files; migration to screen_controllers is incremental and live infrastructure is still required.

**Status:** ✅ DOCUMENTED

**Decisions logged:** `2026-04-29T07-42-57Z-` (orchestration log)

---

## 2026-04-30 — Repo Pollution Cleanup: Scratch Build Dirs + Tracked CMake Artifacts

**Task:** Remove stale scratch build directories and de-track generated CMake output that leaked into version control.

**Actions taken:**
- Deleted local scratch dirs: `build/`, `build-baer-336342/`, `build-hockney-audit/`, `build-keaton-273333/`, `build-keyser-ui/`, `build-unity-verify/`, `build-verify/` (all already covered by `.gitignore` patterns `build/` and `build-*/`).
- `git rm -r --cached build_test_check/` — un-tracked 7 generated CMake files (CMakeCache.txt, CMakeConfigureLog.yaml, CompilerIdCXX artifacts, etc.) and deleted from working tree.
- `git rm --cached build_cmake.txt` — un-tracked this file; it was clearly a captured `cmake -B build` vcpkg install log output, not intentional documentation. Deleted from working tree.
- Added `/build_cmake.txt` and `/build_test_check/` to `.gitignore` so they cannot re-enter the index.
- Preserved `build-unity-verify-vcpkg/` intact (the canonical vcpkg-backed build prefix; already ignored).

**Rule to enforce going forward:** Never `git add` anything under a `build*` directory, or any `*_cmake.txt` / `*_output.txt` diagnostic files. These patterns are covered by `.gitignore` but agents must not force-add them.

## 2026-04-30 — Unity Build Diagnosis: Individual .cpp compiles in build.sh are EXPECTED

**Question:** Did the screen-controller/CMake cleanup break unity builds? Why does `build.sh` compile individual .cpp files?

**Root cause:** Unity builds are intentionally OFF for native (non-Emscripten) platforms by design.

**CMakeLists.txt logic (lines 28-33):**
- `EMSCRIPTEN` → `SHAPESHIFTER_UNITY_BUILD` defaults `ON`
- Non-Emscripten (macOS, Linux, Windows) → defaults `OFF`

**`build.sh` behavior:** Does not pass `-DSHAPESHIFTER_UNITY_BUILD=ON`. Targets the plain `build/` directory. Cache confirms: `SHAPESHIFTER_UNITY_BUILD:BOOL=OFF`.

**Verdict:** No bug. Screen-controller changes did not break unity. Individual `.cpp` compile lines are expected on native. The design intent is: incremental rebuilds matter more locally; unity is used to cut WASM compile time.

**How to get unity builds natively:** Pass `-DSHAPESHIFTER_UNITY_BUILD=ON` to cmake, or use the pre-configured `build-unity-verify-vcpkg/` directory (cache has `SHAPESHIFTER_UNITY_BUILD:BOOL=ON`).

**FAQ for future confusion:** If someone says "build.sh compiles individual .cpp files" — that is correct and intentional on macOS/native. Only `build-unity-verify-vcpkg` tests unity behavior.

## 2026-04-30 — Unity Builds Now Default ON for All Platforms

**Task:** Enable unity builds by default across all platforms (native + Emscripten).

**Change:** Modified `CMakeLists.txt` lines 24-29 to replace platform-conditional option() with unconditional default ON:
- **Before:** `if(EMSCRIPTEN) option(...ON) else() option(...OFF) endif()`
- **After:** `option(SHAPESHIFTER_UNITY_BUILD "..." ON)` — single declaration, all platforms

**Comment updated:** Removed claim that "native builds keep unity OFF by default". New text emphasizes full rebuilds benefit all platforms and users can override with `-DSHAPESHIFTER_UNITY_BUILD=OFF` for incremental-rebuild-focused workflows.

**Learnings:**
- Unity builds now reduce full rebuild time on all platforms, not just WASM
- Native devs can still disable with `-DSHAPESHIFTER_UNITY_BUILD=OFF` when incremental rebuilds are more important
- `build.sh` does NOT pass unity flag, so existing plain `build/` directories will adopt new ON default on next configure

## Learnings

- raygui is now supplied by vcpkg manifest dependency `raygui` (header-only); CMake resolves it via `find_path(RAYGUI_INCLUDE_DIR raygui.h REQUIRED)` and injects it as a SYSTEM include on `shapeshifter_lib`.
- Removed committed third-party source `app/ui/vendor/raygui.h`; all active UI screen controllers and `app/ui/raygui_impl.cpp` now include `<raygui.h>`.
- Kept `app/ui/raygui_impl.cpp` as a minimal project-owned integration TU because raygui is header-only and still requires exactly one `#define RAYGUI_IMPLEMENTATION` site.
- Validation command: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass, 867 test cases / 2603 assertions).
- Title screen source-of-truth paths: `content/ui/screens/title.rgl` and `app/ui/generated/title_layout.h`; controller glue stays in `app/ui/screen_controllers/title_screen_controller.cpp`.
- Keep title rendering on the generated `title_controller.render()` path; apply style overrides around it (`GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER)`) instead of manual `DrawText`/`GuiButton` overrides.
- Settings affordance decision: move to bottom-right at `632,1170,64,64` with raygui gear label `#142#`; keep behavior wired to `GamePhase::Settings`.
- Validation command for this change: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass).
- Root cause (settings gear no-op): raygui buttons were hit-tested in raw window coordinates while UI renders in fixed virtual space (720x1280) with letterbox scaling/offset; the small bottom-right gear missed clicks under scaled windows.
- Files changed: `app/systems/ui_render_system.cpp` (scoped `SetMouseOffset/SetMouseScale` virtual-space mapping around screen-controller render), `tests/test_world_systems.cpp` (settings transition regression for game-state path).
- Validation command: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'`.
- Root-level `app/ui/*.cpp|*.h` audit after screen-controller migration: all 10 files are still live; no safe deletions. `raygui_impl.cpp` is the single `RAYGUI_IMPLEMENTATION` TU; `text_renderer.*`, `ui_loader.*`, and `ui_source_resolver.*` are used by runtime systems and tests; `level_select_controller.*` and `ui_button_spawner.h` still drive dispatcher/game-state/menu-hitbox flow.
- Search proof was captured with repo-wide symbol/include scans (`rg`) for each candidate file (`level_select_controller`, `ui_loader`, `ui_source_resolver`, `text_renderer`, `ui_button_spawner`, `raygui_impl`) showing active references in `app/` and `tests/`.
- Validation command for root-level UI audit: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass, 867 test cases / 2603 assertions).
- `app/ui/raygui_impl.cpp` was audited and retained: removing it (temporary rename + rebuild) fails link with undefined raygui symbols (`_GuiButton`, `_GuiLabel`, `_GuiSetStyle`, etc.); compile database contains no `-DRAYGUI_IMPLEMENTATION`, and vcpkg installs only `raygui.h` (no raygui library object to link).
- Global `-DRAYGUI_IMPLEMENTATION` is unsafe: unity build fails with intra-TU redefinitions (`guiIcons`, `guiState`, `GuiPropertyElement`) because `raygui.h` has no include guard and implementation section cannot be enabled in multiple includes.
- Validation command for audit: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` after probe (pass, 867 test cases / 2603 assertions).
- Re-tested compile-definition ownership without `app/ui/raygui_impl.cpp`: excluded `raygui_impl.cpp` from `UI_SOURCES`, set `COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION` on `app/ui/screen_controllers/title_screen_controller.cpp`, and set `SKIP_UNITY_BUILD_INCLUSION TRUE` on that same file.
- Outcome: approach is stable under unity builds; `build-unity-verify-vcpkg` compiles `Unity/unity_*.cxx` plus one standalone `title_screen_controller.cpp.o` carrying `-DRAYGUI_IMPLEMENTATION` exactly once (verified in `build-unity-verify-vcpkg/compile_commands.json`).
- Validation commands: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` and `cmake -B build-unity-verify-vcpkg -S . -Wno-dev && cmake --build build-unity-verify-vcpkg && ./build-unity-verify-vcpkg/shapeshifter_tests '~[bench]'` (both pass, 867 test cases / 2603 assertions).

### 2026-04-29 — Title Screen Layout Revision (approved)

**Session:** Title Screen UI Fix  
**Scope:** Revision under reviewer lockout (Keaton locked out for preserving override)  
**Verdict:** ✅ APPROVED

Removed runtime override entirely from `title_screen_controller.cpp`. Updated `content/ui/screens/title.rgl` with corrected geometry per Redfoot spec: TitleText (40 200 640 96), StartPrompt (40 640 640 56), ExitButton (260 1080 200 56), SettingsButton (632 1170 64 64 with icon #142#). Synced generated header. Controller now calls `title_controller.render()` with style wrapping (center alignment, uniform 28px). Settings button routes to `GamePhase::Settings`.

**Approval criteria met:** All 5 blocking criteria from Redfoot passed. Build: zero warnings. Tests: 2595 assertions pass.

**Non-blocking note:** Per-element font sizing deferred (uniform 28px is architecturally correct; per-element would require new RGuiScreenController infrastructure).
- Removed committed `app/ui/generated/standalone/` artifacts (README + all standalone `.c/.h` exports) so repo UI source-of-truth is `.rgl` + `*_layout.h` + `screen_controllers` only.
- Updated rguilayout tooling/docs to treat standalone exports as scratch-only artifacts under `build/rguilayout-scratch/` and never committed (`tools/rguilayout/generate_embeddable.sh`, `tools/rguilayout/INTEGRATION.md`, `tools/rguilayout/SUMMARY.md`, `design-docs/rguilayout-portable-c-integration.md`, `RGUILAYOUT_INTEGRATION_PLAN.md`).
- Validation command: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass).

### 2026-04-29T08:05:08Z — Raygui Implementation Ownership Consolidation (Session)

**Task:** `hockney-raygui-compile-define` — Replace dedicated `raygui_impl.cpp` with source-level RAYGUI_IMPLEMENTATION ownership on `title_screen_controller.cpp`.

**Outcome:** ✅ Implemented
- Deleted `app/ui/raygui_impl.cpp` from runtime sources
- Set `COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION` and `SKIP_UNITY_BUILD_INCLUSION TRUE` on `title_screen_controller.cpp`
- Validated native + unity builds pass (867 tests, zero warnings)
- Pattern documented for future single-header library implementations

**Orchestration log:** `.squad/orchestration-log/2026-04-29T08:05:08Z-hockney.md`
