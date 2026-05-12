
## 2026-05-08: Input Dead Code Cleanup (Scribe Log)

Team session: dead code elimination in input routing.
- Keaton: Deleted game_state_end_screen_routing.cpp, inlined routing helper
- Baer: Audited test/benchmark code
- Fenster: Removed duplicate GoEvent test, unused GamePhase param
- Kujan: Reviewed and approved all changes

Status: COMPLETE. All validation/build tests passed.

# Keaton — History

## Core Context

- **Owner:** yashasg

## Current Phase: Raylib Primitives & Shape Vertices Cleanup

### 2026-05-08T11:21:16.041-07:00 — Raylib primitives vs shape_vertices cleanup

- Verified available raylib APIs in this repo's header (`vcpkg_installed/arm64-ios/include/raylib.h`): 2D helpers (`DrawCircle*`, `DrawRing*`, `DrawRectangle*`, `DrawTriangle`) and 3D helpers (`DrawCircle3D`, `DrawTriangle3D`, `DrawTriangleStrip3D`).
- `draw_floor_rings()` renders annulus geometry in world XZ with `rlVertex3f(x, 0, z)` inside `BeginMode3D`; 2D `DrawRing`/`DrawCircle` use `Vector2` screen-space semantics and are not direct replacements.
- `shape_vertices.h` can still be deleted if circle generation is localized in `game_render_system.cpp` (constexpr circle table or `cos/sin` per segment) and test/benchmark references are updated.

## Learnings

- 2026-05-11T22:42:23.114-07:00: Runtime frame orchestration is split between variable-rate pre/post stages in `app/game_loop.cpp` (input/test-player, camera, render, audio/haptics) and deterministic fixed-step gameplay in `app/systems/fixed_tick_runner.cpp`; `game_state_system` is the sole semantic input-event drain (`disp.update<GoEvent/ButtonPressEvent>()`).

- 2026-05-11T01:19:23.327-07:00: Desktop shape key routing is produced in `app/systems/input_system.cpp`; keep keyboard slot-to-shape bindings centralized (now `app/input/keyboard_shape_mapping.h`) and regression-check lane alignment via `tests/test_input_pipeline_behavior.cpp` + `lane_for_shape`.

- 2026-05-08T14:50:05.765-07:00: Replacing manual entity-tracking arrays with an existential EnTT tag (`TestPlayerPlannedTag`) safely removes stale-handle cleanup code and keeps planning state attached to obstacle lifetime without callback coupling.

- 2026-05-08T13:57:50.741-07:00: Full `app/systems/` raylib audit found safe direct-API trims only around gesture collapse and optional XZ ring triangle emission; most lifecycle queues, floor texture work, display sizing, and owned-model drawing remain design-gated by ECS ordering, web canvas behavior, or GPU ownership contracts.

- 2026-05-08T13:57:50.741-07:00: Input-system raylib cleanup has two safe trims (gesture detection via `GetGestureDetected`, and dropping unused transient `InputState` fields after UI pointer release is preserved) while letterbox coordinate conversion remains design-gated because current UI hit-testing relies on explicit virtual-space release coordinates.

- 2026-05-08T13:44:53.252-07:00: Utility dead-surface cleanup is safest when ownership is explicit: obstacle-drain checks should query `reg.view<ObstacleTag>().empty()`, enum-name formatting should use `magic_enum::enum_name` at callsites, and test-player action/planned helpers should stay local to `test_player_system.cpp`.
- 2026-05-08T13:32:04.383-07:00: `app/util/` still has high-confidence dead-surface cleanup wins (`obstacle_counter`, `fs_utils`, `enum_names`) and narrow helpers (`safe_localtime`, `test_player_helpers`) that should move into owning systems/session modules after targeted test rewiring.
- 2026-05-08T13:03:11.140-07:00: Replacing `session_logger` with raylib `SetTraceLogCallback` is not a safe drop-in because the callback is process-global and would capture unrelated engine/app logs unless we add explicit filtering/forwarding behavior.
- 2026-05-08T11:47:09.246-07:00: If floor rings move to a texture-on-quad path (draw 2D in XY into a RenderTexture, then place/rotate that quad into floor XZ), `shape_vertices.h` becomes removable from runtime; the migration risk shifts to render-target lifetime/UV orientation and shader texture-sampling support.
- 2026-05-08T11:53:19.588-07:00: Replacing floor-ring lookup-table sampling with local trig generation in `game_render_system.cpp` removes shared shape vertex data and keeps ring rendering directly in the floor path.
- 2026-05-08T11:59:39.840-07:00: Keeping floor draws in a dedicated `floor_render_system.cpp` preserves behavior while reducing coupling/noise in `game_render_system.cpp`, and lets floor-only includes stay localized.

## 2026-05-08T18:53:19Z — Shape Vertices Removal COMPLETE

**Status:** ✓ DONE. Approved by Kujan.

Floor rings now generate locally in `app/systems/game_render_system.cpp` using cos/sin per segment. Deleted `app/util/shape_vertices.h` and updated all references in tests/benchmarks. Build clean, tests pass.

**Scope closed:** shape_vertices.h removal task complete.

## Previous Phases Summary

See `.squad/agents/keaton/history-archive.md` for earlier work:
- Motion system refactors (May 3)
- Push-lane obstacle removal (May 8)
- Raylib wrapper cleanup (text_renderer, raylib_gesture_input)
- Post-cleanup audit: shape_vertices.h analysis
- Team audit: architecture validation
- Circle floor ring 2D verification (May 8)
- Shape vertices removal (May 8 → 18:53:19Z, COMPLETE)
- 2026-05-08T13:08:46.440-07:00: Highest-confidence raylib cleanup wins are replacing HUD hexagon manual triangulation with `DrawPoly` and file-text ingestion in beatmap loader with `LoadFileText`; floor/world-XZ geometry and model ownership paths should remain design-gated.
- 2026-05-08T13:15:08.642-07:00: `DrawLine3D` is a safe replacement for floor lane/grid/beat `RL_LINES` emission in world-space XZ, while annulus ring geometry should remain on the existing `RL_TRIANGLES` path until design-gated ring architecture changes land.

- 2026-05-08T13:23:49.542-07:00: EnTT audit pass: safest near-term wins are removing the `ObstacleCounter` signal counter in favor of `reg.view<ObstacleTag>().empty()` checks, replacing test-player planned arrays with a `TestPlayerPlannedTag`, and tightening popup fade iteration to `view<ScorePopup, PopupDisplay, Color>`; dispatcher migration for scoring/FX queues remains design-gated due to ordering semantics.

---

## 2026-05-08 Session: Raylib API Replacements

**Task:** Implement safe raylib API replacements for HUD hex fill, beatmap/constants file loading, and floor lines.

**Deliverables:**
- HUD hexagon: replaced manual trig + 6 `DrawTriangle` with `DrawPoly(..., 6, radius, -90.0f, ...)`.
- Beatmap/constants: replaced `std::ifstream` with `LoadFileText`/`UnloadFileText`.
- Floor lines: replaced `RL_LINES` with `DrawLine3D`.
- Floor rings: remains on `RL_TRIANGLES` (design-gated).

**Validation:** 2063 assertions, 758 test cases, zero warnings. APPROVED by Kujan.

**No follow-up work:** Scope complete.

- 2026-05-08T14:14:30.000-07:00: Edge-inclusive collision cleanup now uses raylib `CheckCollisionRecs` with centered hitbox rectangles; audio playback no longer needs an injectable backend because `audio_system` drains queues while directly guarding `PlaySound` behind `IsAudioDeviceReady`, bank loaded state, and `IsSoundValid`.

- 2026-05-08T14:25:19.068-07:00: Cleanup regression tests should prove boundary behavior with compact table/loop cases and rely on existing component/static guards instead of reintroducing broad helper/backend surfaces.

### 2026-05-08T21:49:55Z: EnTT Cleanup LOC Audit Decision

**Date:** 2026-05-08T21:49:55Z  
**Scope:** Read-only LOC estimate for EnTT architecture optimization (plumbing reduction, safe candidates, design-gated moves)

**Baseline:** ./run.sh test passed. Current codebase healthy.

**Safe candidates (~45–80 LOC net reduction):**
1. `test_player_system.cpp` + `test_player.h`: Replace manual `planned[]/planned_count` + `reg.valid` cleanup with existential `TestPlayerPlannedTag` on obstacles. Est. -30 to -55 LOC.
2. `popup_display_system.cpp`: Use structural view `<ScorePopup, PopupDisplay, Color>` instead of `view<ScorePopup> + try_get` checks. Est. -8 to -15 LOC.
3. Scratch singleton helpers (particle_system, obstacle_despawn_system, popup_display_system, scoring_system parts): Eager-init in setup, use `ctx().get<T>()` in hot loops. Est. -7 to -10 LOC (mostly helper deletion; setup adds few lines).

**Medium/design-gated (~35–90 LOC, mostly moved not removed):**
1. Signal wiring flags in session/input/obstacle lifecycle → `entt::scoped_connection` owners. Est. -5 to +10 LOC (cleanup/readability win, minimal LOC).
2. `PendingEnergyEffects` / `ScorePopupRequestQueue` → dispatcher events. Est. -10 to +25 LOC (often code moves, not shrinks).
3. Camera singleton entity pattern → context singleton. Est. -20 to -55 LOC across accessors/checks, but touches many tests/assumptions.

**Not worth for LOC (keep in systems):**
- `scoring_system` collect-then-remove passes
- `collision_system` per-kind structural loops
- `particle_system` / `obstacle_despawn_system` collect-then-destroy safety pattern

**Rationale:**
- EnTT helps best where we currently emulate entity state in manual arrays or repeated lookup helpers
- For phase-sensitive gameplay rules, moving logic out of systems would mostly hide behavior and increase coupling without meaningful code reduction

**Verdict:** Safe moves are net-positive. Medium moves are architecturally sound but neutral on LOC. No breaking changes to baseline. Approved for safe implementation; medium moves await design green signal.
- 2026-05-08T15:09:47.770-07:00: For EnTT v3.16 in this codebase, replacing ad-hoc wiring bools with `entt::scoped_connection` owners plus explicit owner-identity guards keeps connect/unwire idempotent while preserving external listener isolation.
- 2026-05-08: Redfoot segfault came from test fixture missing SongState; collision_system now requires SongState in ctx. Add it in focused test setups that call collision_system directly.
- 2026-05-08T15:52:28.945-07:00: Collapsed input dispatch to semantic-only events (input_system emits GoEvent directly; game_state_system is the sole dispatcher drain; player_input_system drain removed).
- 2026-05-10T02:40:52.785-07:00: For issue #407, ECS-facing component headers now use plain data (`Vec2f`, `TintColor`, `Mat4f`) and runtime-only raylib conversion helpers at render/input boundaries; this keeps component contracts backend-neutral without introducing wrapper abstraction layers.

## 2026-05-12T05:42:23Z — EnTT System Design Documentation (spawn session)

**Session:** Parallel Keyser + Keaton analysis synthesized by Coordinator
**Deliverable:** `.squad/log/2026-05-12T05-42-23Z-entt-system-design.md`

**Task:** Produce comprehensive system design of EnTT architecture, entity/component/system boundaries, util patterns.

**Keaton output:** C++ Implementation Validation
- Concrete implementation structure (fixed-rate, variable-rate, ctx/dispatcher)
- Component layout & data-oriented patterns
- Safe vs. design-gated refactoring candidates
- Performance-safe migration roadmap

**Status:** Complete. System design document synthesized and ready for team review.
