# Hockney — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Platform Engineer
- **Joined:** 2026-04-26T02:07:46.546Z

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

- 2026-04-29: Runtime, CMake copy rules, and Emscripten preload flags must treat `content/` as the single shipped root; do not maintain a parallel top-level `assets/` tree.
- Fonts now live at `content/fonts/LiberationMono-Regular.ttf` (plus `LICENSE-SIL-OFL.txt`), and `text_init_default()` resolves `content/fonts/...` both exe-relative and repo-relative.
- When consolidating roots, update platform docs and tooling prompts (`.github/agents/rhythm-designer.agent.md`, `design-docs/rhythm-design.md`) so generated beatmaps target `content/beatmaps/`.
- 2026-04-29: `bash run.sh test` can fail to find EnTT when `build/CMakeCache.txt` was first configured before the vcpkg toolchain was active; `CMAKE_TOOLCHAIN_FILE` cannot be retrofitted into an existing CMake build tree.
- `build.sh` now verifies `${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` exists and regenerates only `build/CMakeCache.txt` + `build/CMakeFiles` when the cache lacks vcpkg markers, preserving `build/vcpkg_installed` for CI/local cache reuse.
- Validation: `bash run.sh test '~[bench]'` passed (775 test cases, 2210 assertions); EnTT resolved from `build/vcpkg_installed/arm64-osx/share/entt`.
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

## 2026-04-29 — TestFlight Persistence hardening (#297, #298)

- Added shared persistence policy (`app/util/persistence_policy.*`) that resolves one canonical root + file paths for settings/high scores and returns structured `persistence::Result` status.
- iOS path contract is explicit: `HOME/Library/Application Support/shapeshifter` (no silent CWD fallback). Path/directory failures now surface as `PathUnavailable` / `DirectoryCreateFailed`.
- Upgraded settings/high-score load/save APIs from `bool` to structured result enums (`MissingFile`, `CorruptData`, `FileOpenFailed`, etc.) and propagated results through startup and save call sites.
- Added save observability and retry state (`last_load`, `last_save`, `dirty`) on persistence ctx structs; game-state high-score save now preserves dirty state on failure.
- Validation: `cmake --build build-ralph --target shapeshifter_tests && ./build-ralph/shapeshifter_tests "[settings],[high_score]" --reporter compact` → pass (174 assertions / 37 tests).
- 2026-04-29: Restored pre-split energy semantics by applying ordered pending energy events with clamp-after-each-step in energy_system; added mixed same-tick boundary regression coverage.
- 2026-04-29: `content/ui/screens/gameplay.rgl` is authoritative for gameplay HUD shape slots; `GameplayHudLayout_*ButtonBounds` in `app/ui/generated/gameplay_hud_layout.h` must match DummyRec rectangles exactly (Circle 60/1140/140/100, Square 220/1140/140/100, Triangle 380/1140/140/100).
- When rguilayout omits DummyRec from export, keep geometry helpers in the generated embeddable header and pin them with gameplay HUD pipeline tests (`[input_pipeline][hud]`) so source/generated drift fails CI.

## 2026-04-29 — Gameplay shape buttons migration (final revision R6 → approved)

**Status:** FINAL IMPLEMENTATION PASS
**Reviewer:** Kujan
**Verdict:** APPROVED (all 5 acceptance gates pass)

**Work completed:**
- Resolved geometry source-of-truth drift: aligned generated `gameplay_hud_layout.h` shape slot positions with `content/ui/screens/gameplay.rgl` DummyRec definitions
- Updated shape slot coordinates to match `.rgl` exactly:
  - Circle slot: `(60, 1140, 140, 100)` → center `(130, 1190)`
  - Square slot: `(220, 1140, 140, 100)` → center `(290, 1190)`
  - Triangle slot: `(380, 1140, 140, 100)` → center `(450, 1190)`
- Added source-drift regression test to detect future misalignment
- Full production acceptance path validated: raw raygui bounds expanded to enclose 1.4× forgiveness radius, circular filter applied before semantic dispatch

**Acceptance gates (all pass):**
1. ✅ HUD/raygui-owned shape controls; ECS spawning removed
2. ✅ Stock rectangular visuals hidden; custom circular visuals preserved
3. ✅ Legacy 1.4× circular tap forgiveness production-reachable (+70 accept, +71 reject)
4. ✅ Geometry matches `gameplay.rgl` DummyRec slots; no divergence
5. ✅ Pause behavior and existing side effects intact

**Validation:**
- `cmake -B build -S . -Wno-dev` ✅
- `cmake --build build` ✅
- `./build/shapeshifter_tests "[input_pipeline][hud]"` ✅
- `./build/shapeshifter_tests "[gamestate][play_session]"` ✅
- `./build/shapeshifter_tests "[hit_test]"` ✅
- `./build/shapeshifter` ✅
- `git diff --check` ✅

**Kujan final approval:** Migration ready for production. All reviewer blockers resolved. Gameplay shape buttons now HUD/raygui-owned with preserved circular UX and 1.4× tap forgiveness.

**See:** `.squad/orchestration-log/2026-04-29T22-03-09Z-hockney.md`

## Session: Assets Root Removal (2026-04-30)

Removed top-level `assets/` directory. Moved fonts to `content/fonts/`. Updated runtime paths in `app/ui/text_renderer.cpp`. Updated CMake font copy rules and Emscripten preload flags (removed `assets@/assets`, kept `content@/content`). Updated docs/tooling references from `assets/beatmaps/` to `content/beatmaps/`. Build validation passed. Exposed stale LanePush test contract.

**Manifested:** Decision #172 merged to `.squad/decisions.md`

## 2026-04-30 — PR #357 WASM preview abort (`emscripten_sleep`) hotfix

- **Root cause:** `shapeshifter` WebAssembly link flags did not enable async stack transformation, so runtime calls that resolve to `emscripten_sleep` aborted in browser preview (`Aborted(Please compile your program with async support...)`).
- **Fix:** Added `-sASYNCIFY` to the `if(EMSCRIPTEN)` link options for the `shapeshifter` target in `CMakeLists.txt` (web-only scope).
- **Regression guard:** Added CI step `Verify WASM async support flag` in `.github/workflows/ci-wasm.yml` that fails if `build-web/CMakeFiles/shapeshifter.dir/link.txt` does not contain `-sASYNCIFY`.
- **Validation:** `emcmake cmake -B build-web -S . ... -DVCPKG_TARGET_TRIPLET=wasm32-emscripten && cmake --build build-web -- -j$(sysctl -n hw.ncpu) && (cd build-web && ctest --verbose --output-on-failure)` passed (WASM build + `shapeshifter_tests_wasm` pass). `git diff --check -- CMakeLists.txt .github/workflows/ci-wasm.yml` clean.
- **PR status:** Changes prepared for `squad/level-designer-html-hardening`; push/check monitoring pending this hotfix commit.
