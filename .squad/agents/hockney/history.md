# Hockney — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Platform Engineer
- **Joined:** 2026-04-26T02:07:46.546Z

## 2026-04-29 — Settings Gear Click Fix + Standalone UI Cleanup Wave

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

**Decisions logged:** `2026-04-29T07-30-55Z-hockney.md`

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

- Title screen source-of-truth paths: `content/ui/screens/title.rgl` and `app/ui/generated/title_layout.h`; controller glue stays in `app/ui/screen_controllers/title_screen_controller.cpp`.
- Keep title rendering on the generated `title_controller.render()` path; apply style overrides around it (`GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER)`) instead of manual `DrawText`/`GuiButton` overrides.
- Settings affordance decision: move to bottom-right at `632,1170,64,64` with raygui gear label `#142#`; keep behavior wired to `GamePhase::Settings`.
- Validation command for this change: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` (pass).
- Root cause (settings gear no-op): raygui buttons were hit-tested in raw window coordinates while UI renders in fixed virtual space (720x1280) with letterbox scaling/offset; the small bottom-right gear missed clicks under scaled windows.
- Files changed: `app/systems/ui_render_system.cpp` (scoped `SetMouseOffset/SetMouseScale` virtual-space mapping around screen-controller render), `tests/test_world_systems.cpp` (settings transition regression for game-state path).
- Validation command: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'`.

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
