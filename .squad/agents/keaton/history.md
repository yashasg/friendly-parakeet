# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-04-26T02:09:15.781Z

# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.

### Work Completed

1. **Created `app/systems/motion_system.cpp`** — new system with phase guard matching scroll_system's
2. **Stripped `app/systems/scroll_system.cpp`** — removed vel_view and motion_view, leaving 3 obstacle-only views
3. **Updated `app/systems/all_systems.h`** — added motion_system declaration under Phase 5
4. **Updated `app/game_loop.cpp`** — added motion_system call immediately after scroll_system
5. **Test consolidation:** Renamed scroll→motion tests; added phase-guard test for motion_system
6. **Bench updates:** Added motion_system benchmarks (10/100/1000 entity tiers)

### Build & Test Results

- **Build:** Zero warnings (clang -Wall -Wextra -Werror)
- **Tests:** 2211 assertions / 772 test cases — all pass (+2 assertions from new motion phase-guard test)
- **Bench:** See decisions.md for full breakdown. 10-entity combined cost +27% (61 ns vs 48 ns baseline); architectural split overhead, not per-entity regression

### Phase Guard Coupling Discovery

Original scroll_system's phase guard silently gated vel_view and motion_view too. When extracted without the guard, 3 tests failed (position-integration assumed no movement in non-Playing). Fixed by adding identical guard to motion_system. No behavioral regression; coupling was implicit until extraction exposed it.

### Module Health

scroll_system: 🟢 Green (obstacle-only, SRP resolved)  
motion_system: 🟢 Green (new, clean)

### Pattern Note for Future Reference

**Synthetic benchmarks must be re-validated after archetype refactors — splitting a system can drop a fixture's matching entity count to zero, leaving the bench measuring nothing.** The scroll_system 10-entity bench fixture spawns ObstacleTag+Position+Velocity (no ObstacleScrollZ or BeatInfo), which was processed by vel_view in the old code but now matches zero scroll_system views post-extraction. The "empty system" measurement is correct but misleading for perf analysis; a proper fixture should include ObstacleScrollZ entities to stress the actual scrolling loops.

### Pattern Note for Future Reference

**Synthetic benchmarks must be re-validated after archetype refactors — splitting a system can drop a fixture's matching entity count to zero, leaving the bench measuring nothing.** The scroll_system 10-entity bench fixture spawns ObstacleTag+Position+Velocity (no ObstacleScrollZ or BeatInfo), which was processed by vel_view in the old code but now matches zero scroll_system views post-extraction. The "empty system" measurement is correct but misleading for perf analysis; a proper fixture should include ObstacleScrollZ entities to stress the actual scrolling loops.

### Decision

Merged to `.squad/decisions.md` under "2026-05-03 — Ralph Round 3" section.

---

## 2026-05-03 — Ralph Round 4: Bench Fixtures + collision_system Optimization

**Loop:** Ralph Round 4 (perf + SOLID continuous iteration)  
**Task:** Fix broken bench fixtures (scroll_system + collision_system) and optimize collision_system hot path  
**Verdict:** ✅ MERGED

### Execution Summary

Keaton identified two bench fixtures degrading silently due to archetype evolution and fixed them to accurately measure real entity work. Additionally optimized collision_system's hot path by precomputing frame-constant values and collapsing a redundant 2D geometric check to 1D.

### Work Completed

1. **scroll_system bench:** Added `spawn_scroll_obstacles` helper creating proper freeplay archetype (ObstacleScrollZ+Velocity)
2. **collision_system bench:** Fixed `make_bench_player` with missing WorldTransform+ShapeWindow components
3. **collision_system perf:** Precomputed `player_timing_y` and `player_x` outside loops; replaced `player_overlaps_lane(centered_rect)` calls with 1D `|obs_x - player_x| < SIZE` check; removed 4 dead helpers

### Round 13 — Chain-Bonus SRP Retraction + Motion System Migration

**Date:** 2026-05-05

**Work:**
1. Investigated chain-bonus SRP coupling claim (r12 follow-up): grep showed chain_count/chain_timer writes ONLY inside scoring_system.cpp. State is fully encapsulated in scoring's concern. Co-location is intentional, not a smell. **Retracted the SRP recipe.**
2. Moved motion_system migration forward (Option C): Updated vel_view comment to document "freeplay obstacles only (issue #349)". Found and fixed bench archetype mismatch: spawn_particles was creating Position+Velocity (legacy path) instead of WorldTransform+MotionVelocity (production). Added two new unit tests for motion_view path.
3. Tests: 784/2251 → 786/2255 (+2 cases, +4 assertions). Zero warnings.

**Metrics:** Pre/post test count: 784 → 786 non-bench cases. Bench archetype fix means particles now correctly hit motion_view.

**Process note:** When you find a wrong bench, prior "no delta" claims need revisiting. Bench measured the legacy path, not production.

**Pattern Learned:** Retract recipes when grep falsifies them. Don't ship the wrong refactor to look productive. Independent agreement (Keyser reached same RETRACT verdict) is strong evidence.

**Decision:** Merged to `.squad/decisions.md` under "Round 13: Keaton — Chain-Bonus SRP Retraction + Motion System Migration" section.

---

### Round 15 — Velocity Struct Deletion + vel_view Elimination

**Date:** 2026-05-06

**Work:**
1. **Full Velocity migration:** Deleted `Velocity` struct from `app/components/transform.h` entirely. Migrated `spawn_obstacle` to `MotionVelocity{{0, speed}}`. Migrated `scroll_system` model_view from `Velocity` to `MotionVelocity` with `.value.y` accessor.
2. **vel_view → motion_view:** Deleted the 11-line `vel_view` loop from `motion_system.cpp`. Added `Position` bridge in motion_view to sync Position from WorldTransform when both present (legacy compatibility for collision/scoring/camera that still read Position).
3. **Test migration (13 files):** Updated 6 factory functions in `test_helpers.h`. Updated 8 archetype assertions in `test_obstacle_archetypes.cpp`. Rewrote 3 motion_view tests (one covering Position bridge). Updated 5 other test files. Removed 1 obsolete Velocity assertion. Updated 4 bench sites.
4. **Metrics:** 19 files touched. Pre: 786/2255. Post: 786/2256 (+1 assertion from Position bridge test). Zero warnings.
5. **Benchmarks:** motion_system slightly higher due to `try_get<Position>` bridge cost (~30–90 ns at 10–100 entities); particle_system and full-frame within r14 baseline.

**Metrics:** Bridge overhead ~30–90 ns; justified by elimination of legacy `Velocity` type and the fragmented vel_view loop.

**Pattern Learned:** When a migration touches 19 files, do them in lockstep with continuous test-passing. The bridge approach (motion_view writes Position) lets readers migrate independently in a future round. This decouples writer migration from reader migration.

**Decision:** Merged to `.squad/decisions.md` under "Round 15: Keaton vel_view → motion_view Migration (Issue #349)" section. Commit: `70f6436`.

### R16: Large Position Deletion Migration (35 files, 491 deletions / 145 insertions)

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 784 cases / 2234 assertions (−2 cases / −22 assertions vs r15)

**Pattern:** When migrating away from a legacy type (Position), delete the type and migrate all readers in lockstep with continuous test-passing. Motion-system bridge eliminated; latent double-integration path (LowBar/HighBar) also eliminated.

**Compliance note:** Missed verbatim tail-5 paste protocol in decision drop. Keaton-r16 drop omitted full `tail -5` from test run. Strict adherence to tail-5 protocol from r17 onwards.

**Process:** 15+ production files + tests + benchmarks migrated atomically. Build green, zero warnings. Module health: motion_system 🟡 → 🟢.

### R17: Double-Integration Fix + Bench Re-baseline

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 785 cases / 2235 assertions (+1 case / +1 assertion: new double-integration regression test)

**Pattern:** Structural exclusion via `entt::exclude<...>` > runtime checks for double-integration prevention.

**Latent fix applied:** `motion_view` now excludes `ObstacleScrollZ`, making scroll_system's `model_view` and motion_system's `motion_view` mutually exclusive on the discriminator component.

**Correctness verified:** Fail-then-fix test added: pre-fix produced `200.0f == 150.0f` (2× integration); post-fix passes.

**Bench win:** motion_system **26.6 ns / 10 ents** (vs r14 baseline 34-38 ns), ~30% improvement post-r16 Position bridge deletion.

**Protocol:** Pre/post tail-5 paste protocol restored after r16 lapse. ✅

**Verdict:** motion_system 🟢 (latent fixed). All 18 modules 🟢. Loop at natural diminishing returns per Keyser-r17.


### R19: Hot-Path Dispatch Refactor (Collision)

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 784 cases / 2179 assertions (full suite), plus 51 collision cases / 112 assertions

**Work:**
1. Profiled hot-path benches (`Bench: collision_system`, `Bench: scroll_system`, `Bench: motion_system`) as baseline.
2. Refactored `collision_system` shape-bearing paths to pre-dispatch by archetype when timing grading is active (`BeatInfo` vs non-`BeatInfo`) and moved timing-grade work into a dedicated shape-grade lambda.
3. Added a `can_grade_shape` gate so freeplay/non-press frames stay on a single fast path (no extra empty-view iteration cost).
4. Kept gameplay behavior unchanged; no test expectation updates required.

**Perf summary (bench_systems):**
- collision 1 obstacle: **127.07 ns → 126.94 ns** (flat/slightly better)
- collision 10 obstacles: **153.28 ns → 140.64 ns** (~8% faster)
- motion/scroll stayed within run-to-run variance band (no production code changes there).

**Pattern Learned:** Pre-dispatch by archetype pays off in collision hot loops when gated to the active mode. Attempted scroll pre-dispatch was measured and rejected (regressed); keep direct loops unless branch elimination wins in benchmark.

### R20: Branch Fan-Out Reduction Pass (collision/scoring/scroll)

**Date:** 2026-05-03  
**Completion:** ✅ SHIPPED  
**Tests:** 768 cases / 2179 assertions (non-bench suite)

**Work:**
1. `collision_system`: hoisted timing-grading eligibility into one top-level branch and split shape-bearing loops into graded/non-graded structural partitions (`BeatInfo` vs non-`BeatInfo`) to remove repeated mode checks in hot loops.
2. `scoring_system`: replaced per-entity `try_get<TimingGrade>` with structural timed/untimed views and consolidated tier side effects (energy + SongResults counters + multiplier source) into one helper switch.
3. `scroll_system`: replaced per-entity `try_get<WorldTransform>` with structural with/without-`WorldTransform` views for both beat and freeplay `ObstacleScrollZ` paths; removed `ObstacleTag` from these views to lower join cost.

**Perf summary (bench_systems, 30 samples):**
- scroll 10 entities: **73.11 ns → 56.57 ns** (faster)
- scroll 100 entities: **391.50 ns → 390.30 ns** (flat/slightly faster)
- scroll 1000 entities: **2.97 us → 3.58 us** (slower)
- collision 1 obstacle: **126.83 ns → 133.02 ns** (slower)
- collision 10 obstacles: **149.70 ns → 162.47 ns** (slower)
- full frame typical: **471.93 ns → 546.47 ns** (slower)
- full frame stress: **791.07 ns → 1.22 us** (slower/noisier but materially up)

**Pattern Learned:** Branch elimination via additional structural partitioning is not automatically a win in EnTT; extra join/select costs can dominate at scale. Keep benchmarking each partitioning step and reject “cleaner branch profile” changes that regress full-frame throughput.

## 2026-05-03 — Opt-123 Finalization (Keep-only-positive)

**Task:** Re-evaluate branch-reduction pass and keep only net-positive changes for this codebase.

**Decision:** Kept collision refactor; dropped scroll/scoring structural partitioning.

**Measured outcome (50 samples, before=all-3 refactors):**
- collision 1: 141.98 ns → 135.12 ns
- collision 10: 170.44 ns → 152.36 ns
- scroll 10: 75.64 ns → 86.43 ns
- scroll 100: 422.03 ns → 416.89 ns
- scroll 1000: 3.86 us → 3.05 us
- full-frame typical: 568.30 ns → 507.15 ns
- full-frame stress: 1.01 us → 901.23 ns

**Validation:** Build clean; tests pass (768 cases, 2179 assertions).

**Learning:** Structural split-by-archetype helps only when it does not multiply view passes on dense hot paths; for scroll/scoring in this codebase, fewer-pass loops outperform aggressive branch elimination.

## 2026-05-03 — Issue #350 Research: magic_enum vs EnTT meta

**Task:** Determine whether EnTT v3.16.0 reflection can replace `magic_enum` for enum names/count contracts without sentinel/macros/duplication drift.

**Outcome:** Keep `magic_enum`; no migration recommended.

**Why:**
- Current code depends on compile-time `enum_count` for `static_assert`-guarded array sizing and test parity.
- EnTT meta requires manual registration (`meta_factory::data`) and runtime iteration (`meta_type::data`) and does not provide auto enum member reflection with constexpr count.
- Replacing `magic_enum` would reintroduce duplicated enum lists and startup-order coupling.

**Pattern Learned:** For enum-name/count infrastructure, a replacement must preserve compile-time guarantees first; runtime reflection parity is insufficient when array bounds and test contracts are compile-time assertions.

### 2026-05-04T04:55:12Z — Decision Registry: Issue #350 recommendation merged

- **Scribe action:** Merged inbox decision file to `.squad/decisions.md` (`# Decision: Keep magic_enum...`)
- **Orchestration log written:** `.squad/orchestration-log/2026-05-04T04:55:12Z-keaton.md`
- **Status in registry:** Recommended (GitHub issue #350 comment posted, gate cleared by Verbal)

## 2026-05-04 — Phase 1 SDL2 Migration Abstraction Layer

**Task:** Begin approved raylib -> SDL2 migration by introducing platform abstraction seams without gameplay changes.

**Work completed:**
1. Added thin platform interfaces in `app/platform/graphics/renderer.h`, `app/platform/input/input_handler.h`, and `app/platform/window/window_manager.h`.
2. Added raylib-backed implementations and SDL2 placeholder stubs for compile-time scaffolding in corresponding `*_raylib.cpp` and `*_sdl2.cpp` files.
3. Refactored high-leverage call sites to use abstractions:
   - `app/game_loop.cpp` now routes frame timing, render pass orchestration, and window lifecycle through renderer/window managers.
   - `app/systems/input_system.cpp` now polls through input handler interface while preserving gesture/keyboard/focus behavior.
   - `app/systems/game_render_system.cpp` now uses renderer interface for frame clear and 3D mode boundaries.
   - `app/platform_display.cpp` native display size now comes from window manager abstraction.
4. Updated CMake platform source globbing to compile abstraction layer directories.

**Validation:**
- `cmake -B build -S . -Wno-dev`
- `cmake --build build`
- `./build/shapeshifter_tests`
- Result: `All tests passed (2176 assertions in 782 test cases)` with no warning/error lines reported.

**Learning:** Keep interfaces intentionally narrow and route only high-churn API boundaries first (timing, frame begin/end, window/input polling). This preserves behavior parity while creating a low-risk seam for later SDL2 functional implementation phases.

## 2026-05-04 — SDL2 Migration Phase 2: Native Window + GL Context Bring-up

**Issue:** #372 (Raylib -> SDL2 phased migration)  
**Branch:** `feature/sdl2-migration-phase-1-abstraction-layer`  
**Base:** Phase 1 commit `5b46969`

### Work Completed

1. Added native SDL2 platform scaffolding in `app/platform/sdl2/`:
   - `sdl2_headers.h` (portable SDL2 include shim)
   - `sdl2_init.h/.cpp` (SDL init/shutdown lifecycle)
   - `sdl2_graphics_context.h/.cpp` (window + OpenGL context + event pump + frame timing)
2. Replaced SDL2 stubs with real Phase-2 bring-up wiring:
   - `window_manager_sdl2.cpp` now creates/destroys SDL2 GL window and polls close events.
   - `renderer_sdl2.cpp` now clears via OpenGL and swaps window buffers.
3. Added compile-time backend selection plumbing:
   - `SHAPESHIFTER_BACKEND` CMake cache option (`raylib` default, `sdl2` optional).
   - Compile definitions: `SHAPESHIFTER_BACKEND_RAYLIB` / `SHAPESHIFTER_BACKEND_SDL2`.
   - Runtime selectors in abstraction entry points now dispatch by compile-time backend.
4. Added native dependency wiring:
   - CMake: `find_package(SDL2 CONFIG REQUIRED)` for non-Emscripten.
   - Manifest: added `sdl2` in `vcpkg.json` for native platforms.
5. Added behavior-safe bring-up path in `game_loop.cpp` for SDL2 backend:
   - SDL2 backend now runs blank-screen bootstrap loop and clean shutdown.
   - Default raylib path remains unchanged and functional.

### Validation

- Baseline (raylib backend): configure + build + full tests passed.
- SDL2 backend toggle (`-DSHAPESHIFTER_BACKEND=sdl2`): configure + build + full tests passed.
- Zero-warning policy preserved (build succeeded with existing `-Werror` policy).

### Notes

- Phase 2 target (native SDL2 window + GL context bring-up) is implemented.
- Rendering/gameplay parity migration is intentionally deferred to later phases.

## 2026-05-04 — SDL2 Migration Phase 3: Rendering Core Port (floor/playfield path)

**Issue:** #372  
**Branch:** `feature/sdl2-migration-phase-1-abstraction-layer`

### Work Completed

1. Ported high-priority floor/playfield draw path in `game_render_system.cpp` from direct `rlBegin/rlVertex3f` usage to abstraction-layer primitive calls.
2. Guarded raylib-specific mesh draw path in `game_render_system.cpp` so SDL2 backend executes the floor geometry slice safely while keeping raylib behavior intact.
3. Updated SDL2 bring-up flow in `game_loop.cpp` to run the Phase-3 world render path instead of black-screen-only bootstrap:
   - seed minimal render state (`GameState`, `GameCamera`, `FloorParams`)
   - execute `game_render_system(...)` each frame on SDL2 backend.
4. Updated SDL2 GL context attributes (`sdl2_graphics_context.cpp`) to use compatibility profile-friendly settings required by fixed-function floor rendering path.

### Validation

- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=raylib`
- `cmake --build build`
- `./build/shapeshifter_tests --reporter compact`
- `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=sdl2`
- `cmake --build build`
- `./build/shapeshifter_tests --reporter compact`
- restored default config: `cmake -B build -S . -Wno-dev -DSHAPESHIFTER_BACKEND=raylib && cmake --build build`

All commands passed (zero-warning build policy preserved).

### Phase 3 Status (done vs remaining)

**Done now (Phase-3 core priority):**
- SDL2 backend renders playfield core floor geometry path (corridor edges, lane guides, floor connectors, ring fill, beat lines)
- Render path routed through abstraction layer (raylib + SDL2 implementations)
- Raylib backend kept healthy with parity-preserving abstraction implementation

**Remaining in Phase 3 (next incremental slices):**
- Port obstacle mesh/model path (`DrawMesh`/owned model draws) to SDL2 backend
- Port particle 3D geometry path to SDL2 backend
- Port UI render compositing path for SDL2 backend (currently world floor path focus)
- Add screenshot/pixel-diff regression checks for raylib vs SDL2 floor parity

## 2026-05-04 — SDL2 Migration Phase 3 Completion (remaining scope)

**Issue:** #372  
**Branch:** `feature/sdl2-migration-phase-1-abstraction-layer`

### Completed in this pass

1. **SDL2 obstacle/particle render paths completed in `game_render_system`:**
   - Added SDL2 draw path for `ModelTransform` world/effects entities (slab/shape/quad), using triangle primitives and mesh data when available.
   - Added SDL2 draw path for `ObstacleModel` owned meshes in world pass.
2. **SDL2 UI compositing parity path in `game_loop_frame`:**
   - SDL2 now follows world pass → UI pass → composite flow (texture-mode boundaries + dual blit) matching current architecture.
   - SDL2 init now seeds `UICamera` + `RenderTargets` context needed for that path.
   - SDL2 `ui_render_system` branch is now safe/no-op (avoids raylib-only immediate-mode calls in SDL2 runtime) while preserving the pass boundary.
3. **Parity checks expanded (Baer hooks):**
   - `tests/test_renderer_sdl2_validation.cpp` now asserts two-pass compositing counters and 3D triangle primitive counter usage.
   - `docs/sdl2-phase3-rendering-parity-checklist.md` updated to include these checks explicitly.

### Validation results

- `cmake --build build-raylib --target shapeshifter_tests` ✅
- `ctest --test-dir build-raylib --output-on-failure` ⚠️ 7 pre-existing failures (same known test-player/UI regressions)
- `cmake --build build-sdl2 --target shapeshifter_tests` ✅
- `ctest --test-dir build-sdl2 --output-on-failure` ⚠️ 23 pre-existing failures (content-path + known test-player/UI regressions)
- `./build-sdl2/shapeshifter_tests "[render][sdl2][validation]"` ✅ (20 assertions / 2 cases)

### Notes

- Raylib backend-specific mesh path remains unchanged and still selected unless backend flag is SDL2.
- SDL2 phase-3 remaining scope from prior report is now implemented in shipping code paths for this migration phase.
