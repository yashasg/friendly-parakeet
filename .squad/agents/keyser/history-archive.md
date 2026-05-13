# Keyser — History (Summarized)

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

## Executive Summary

Conducted 17 audit rounds focusing on SOLID compliance, module health, system wiring verification, and architectural refactoring guidance. Key achievements: Identified and escalated production bugs (lane_push_response_system wiring, order regressions), audited and reconciled Position migration (#349), established forensic procedures for test count validation, and maintained module health snapshot across complex multi-round iterations.

**Current Module Status:** 11 🟢 (green) / 1 🟡 (pending) — motion_system awaiting complete Position bridge deletion (follow-up to R17)

## Key Learnings

1. **Audit findings labeled 'redundant' must trace ALL invocation paths** — Event-dispatcher callbacks invoke functions outside static runners; guards protecting dispatcher callbacks aren't redundant just because the runner gates other paths.

2. **Parallel agent edits are swept up by `git add -A`** — Scribe must use explicit `git add` paths; commit-message attribution can diverge from actual diff location.

3. **Test assertions claiming pre-fix failure are vibes claims** — Demand fail-then-fix verification (stash production, run tests, unstash) before trusting a test comment's claim about pre-fix behavior.

4. **Synthetic benchmarks must be re-validated after archetype refactors** — Splitting a system can drop fixture matching to zero, leaving the bench measuring nothing.

5. **When implementation hasn't landed, audit only stable commits** — Don't speculate on in-flight work; pre-document baselines for post-implementation comparison.

6. **Self-wiring tests mask production bugs** — Tests calling systems directly short-circuit integration verification. Demand at least one test that calls the production tick path.

7. **Methodology inconsistency generates false alarms** — Canonical commands + verbatim output enforce discipline. Filter-flag asymmetry (r10: no-filter 798 total; r11: ~[bench] 783 non-bench) can look like regression.

8. **When flagging commit absence, verify gitignore status first** — Inbox files are gitignored by design; a missing file in git doesn't indicate process failure.

## Recent Work

### R17: End-to-End Position Deletion Audit

**Date:** 2026-05-03  
**Scope:** r16 Position deletion (7ae9659) + r17 WIP latent fix

**Findings:**
- F1: decisions.md incorrectly credited r16; fix was r17. Corrected via surgical edit.
- F2: r16 inbox absence initially flagged 🔴 — verified as gitignored by design (not a process miss).
- Position deletion successful: all critical sites (collision, scoring, camera, scroll) migrated correctly.
- Test count drop (786→784, 2256→2234): all deletions correctly attributed.
- Motion system latent fixed in r17 via Option A (exclude<ObstacleScrollZ>).

**Verdict:** All modules 🟢. Loop at natural diminishing returns.

## Historical Round Snapshots

**R2–R7:** Ralph performance + SOLID audit loop; identified SRP issues in scroll_system, BarObstacleTag refactor, LanePush critical production bug (system declared but not wired).

**R8–R9:** Phase-Guard Design B scope; 11 systems consolidated into central runner; Keaton-r9 refactor landed in `tick_playing_systems`.

**R10–R12:** Order regression discovery (popup_feedback + energy_system moved pre-despawn, breaking design intent); test count forensic (methodology drift r10→r11, no actual deletion); collision_system SRP + scoring_system kind-independence audit.

**R13–R15:** Chain-bonus retraction (independent grep confirmed), SRP audit, fixed_tick_runner demotion reversal (commutativity proof), motion_system latent regression (Velocity → MotionVelocity migration debt).

**R16–R17:** Position bridge audit, Position deletion completion, motion_system latent fix landed.

## Status

**Module health snapshot (post-R17):** 11 🟢 / 1 🟡 (motion_system; pending bridge audit for r18)

**Next:** Audit motion_system Position bridge for completeness; recommend surfacing R17 audit to user for decision on architecture direction or further cleanup.

---

## Learnings (Continued)

### R18: Shape Geometry Audit — 2D Floor Shapes Clarification

**Date:** 2026-05-08  
**Scope:** Read-only audit: necessity of shape_vertices.h for 2D floor rendering

**Questions and Answers:**

**Q1: Is shape_vertices.h still necessary as-is?**  
Partial yes. The CIRCLE array and V2 struct are genuinely required for floor annulus (ring) rendering in 3D world space. The HEXAGON, SQUARE, and TRIANGLE arrays are dead code in production.

**Q2: Which arrays/structs are currently used?**
- **In production:** `shape_verts::CIRCLE` (24-segment circle), `CIRCLE_SEGMENTS` constant, `V2` struct
  - Usage: `app/systems/game_render_system.cpp::draw_floor_rings()` (lines 110–121) — samples circle perimeter to emit annulus triangles via `rlVertex3f()`
- **Dead in production:** `HEXAGON`, `HEX_SEGMENTS`, `SQUARE`, `TRIANGLE`
  - Used only in tests (`tests/test_perspective.cpp`) and benchmarks (`benchmarks/bench_perspective.cpp`)

**Q3: Can floor ring rendering be replaced by raylib 2D APIs?**  
No. Rendering occurs inside `BeginMode3D()/EndMode3D()` (3D camera context). Raylib's 2D functions (`DrawRing()`, `DrawCircleLines()`) operate in 2D screen space and cannot be directly used in a 3D context. The current rlgl low-level approach (`rlVertex3f()`) is the correct choice for emitting 3D world-space geometry.

**Q4: Cleanup recommendations (future pass, no implementation):**
- **Remove:** `HEXAGON` array (6 vertices), `HEX_SEGMENTS` constant, 4 related test cases in `test_perspective.cpp`
- **Remove:** `SQUARE` array (4 vertices), 3 related test cases in `test_perspective.cpp`
- **Remove:** `TRIANGLE` array (3 vertices), 2 related test cases in `test_perspective.cpp`
- **Remove:** Hexagon benchmark in `benchmarks/bench_perspective.cpp` (lines 46–57)
- **Keep:** `CIRCLE` array, `CIRCLE_SEGMENTS`, `V2` struct — these are production-critical for floor ring rendering
- **Consider:** Rename/reorganize header to clarify its now-singular purpose (circle-vertex sampler for 3D rendering), though current name is still acceptable

**Verdict:** The "floor shapes are 2D" clarification confirms that floor entities have no vertical extent (y=0 plane), but their rendering happens in 3D world space. shape_vertices.h remains justified for the CIRCLE table; dead shapes should be removed in a dedicated cleanup pass.

### 2026-05-08T18:08:07Z — TEAM AUDIT FINALIZATION (Sync Session)

**Participants:** Keaton, Keyser (Architecture Review)  
**Session Type:** Coordination + decision logging  
**Session Outcome:** Logged to decisions.md, orchestration logs written

#### Audit Confirmation
1. **shape_vertices.h CIRCLE table (KEEP):** Actively used by app/systems/game_render_system.cpp::draw_floor_rings() to emit world-space annulus triangles via rlgl API (`rlVertex3f()`). Raylib 2D APIs cannot operate inside 3D camera context (BeginMode3D/EndMode3D).
2. **Dead code identified:** HEXAGON, SQUARE, TRIANGLE arrays + HEX_SEGMENTS constant used only in tests and benchmarks. Removable in future cleanup without breaking game logic.
3. **Architecture Status:** Stable. Zero new cleanup candidates identified beyond previous raylib-wrapper removal (text_renderer, raylib_gesture_input).

#### Decision Archived
**Shape Geometry Audit Decision**  
✅ DECISION: Keep shape_vertices.h; schedule dead shape arrays/tests for removal in next cleanup cycle.  
**Owner:** Architecture Review (Keaton, Keyser)  
**Status:** Approved

#### Session Artifacts
- `.squad/orchestration-log/2026-05-08T18-08-07Z-keaton.md` — Keaton work summary
- `.squad/orchestration-log/2026-05-08T18-08-07Z-keyser.md` — Keyser work summary
- `.squad/log/2026-05-08T18-08-07Z-shape-geometry-audit.md` — Session log
- `decisions.md` updated with team decision

#### Next Steps
- Codebase ready for feature development
- Floor rendering refactor (if planned): schedule V2→Vector2 migration as low-priority follow-up
- Dead shape cleanup: schedule in next cleanup iteration when floor rendering changes are planned

### 2026-05-08T13:08:46.440-07:00: Raylib cleanup audit learning

- High-value cleanup is concentrated in low-level rlgl callsites that re-implement primitives raylib already exposes (notably floor lane/beat line emission and HUD hex icon triangulation).
- Preserve ECS-owned runtime state and GPU-lifecycle wrappers where they enforce ownership/testability; replacing those with "simpler" direct helpers risks regressions and hidden resource bugs.

### 2026-05-08T13:23:49.542-07:00: EnTT architecture audit learning

- Highest-leverage EnTT cleanup is eliminating manual singleton bookkeeping (signal-maintained counters, singleton-entity lookups, lazy ctx scratch init) before touching performance-tuned view topology.
- Keep explicit fixed-tick ordering and collect-then-remove safety patterns; EnTT organizer/groups are valuable only when profiling proves a bottleneck and ordering invariants are externalized.

### 2026-05-08T13:57:50.741-07:00: Systems-wide raylib rewire audit synthesis

- Across 31 system files, only 4 items are truly safe first-pass rewires: gesture poll collapse (`GetGestureDetected`), two `Clamp` substitutions (collision precision + popup alpha), and floor ring `DrawTriangle3D` swap (with perf gate on that last one).
- The `lane_overlaps` → `CheckCollisionRecs` swap is medium-risk because raylib uses a closed interval while the current lambda is open; boundary tests must be written and confirmed before proceeding.
- The overwhelming majority of ECS systems have no useful raylib equivalent — game-domain logic (song time, beat scheduling, scoring, particle/despawn/game-state) is not a raylib concern and must stay ECS-owned.
- Design-gated blockers (textured floor ring, letterbox coordinate ownership, audio/haptic queue inlining) each require a design decision first; never conflate "raylib has the API" with "it's safe to substitute."

### 2026-05-08T21:49:55Z: EnTT Lifecycle/API Audit Decision

**Date:** 2026-05-08T21:49:55Z  
**Scope:** Read-only EnTT architecture audit (lifecycle APIs, plumbing reduction opportunities)

**Decision:** Use EnTT APIs to remove orchestration/plumbing (signal wiring state, lazy ctx scratch setup, queue handoff boilerplate), but do NOT move gameplay rules into component/entity callbacks.

**Rationale:**
- Project architecture depends on strict fixed-step ordering and unidirectional writes
- Callback-heavy gameplay would make order, replayability, and tests harder to reason about
- EnTT lifecycle hooks are best for ownership/lifetime side-effects, not core game decisions

**Safe EnTT moves:**
- Replace ad-hoc `wired` bool state with owned EnTT connection objects in context (~5-10 LOC cleanup)
- Eager-init ctx scratch/queue holders in session/init and use `ctx().get<T>()` in hot loops (~7-10 LOC cleanup)
- Consider dispatcher unification for intent queues if fixed-tick drain boundaries remain explicit (~70-150 LOC, design-gated)

**Do NOT move:**
- Collision/scoring/energy/game-state transitions into callbacks
- Fixed tick ordering into implicit signal chains
- External resource ownership (raylib model/audio/window) into generic ECS callbacks

**Expected impact:**
- Plumbing reduction: tens to low-hundreds LOC
- Gameplay logic reduction inside systems: low and intentionally capped (~45-80 LOC safe, ~35-90 LOC medium/design-gated)
- Determinism/testability risk: low only if gameplay remains in explicit phase-run systems

**Status:** Audit complete. Approved for safe-move implementation. Medium/design-gated moves await Coordinator signal.

### R19: Architecture Boundary Audit — app/tests/CMake/docs

**Date:** 2026-05-10  
**Scope:** Read-only audit of `app/`, `tests/`, `CMakeLists.txt`, and migration/design docs against ECS/DOD, dependency-boundary, direct SDL2/glm, no-wrapper, and zero-warning expectations.

**Findings filed:**
- #406: `shapeshifter_lib` exposes `systems/all_systems.h` declarations for runtime-only systems that it deliberately does not link.
- #407: Common ECS/component headers embed raylib platform types, making headless data and tests depend on backend handles instead of glm/direct boundary data.
- #405: Repo docs and open migration guidance still point to raylib/wrapper abstractions, conflicting with the current direct SDL2/glm/no-wrapper direction.

**Learning:** Treat public target headers as part of the architecture boundary: declarations in a PUBLIC include directory must match symbols provided by that target. Migration docs are also architecture inputs; stale wrapper-oriented docs can cause future code to violate current no-wrapper decisions even before product code changes.

### 2026-05-10T02:40:52-07:00: Architecture fix execution (issues #405, #406)

- Split system API surfaces: `app/systems/all_systems.h` now exposes only headless ECS/gameplay declarations, and runtime-bound functions moved to `app/systems/runtime_systems.h`.
- Added a boundary regression test (`tests/test_system_header_boundaries.cpp`) that asserts runtime-only declarations stay out of the headless header.
- Marked raylib migration docs as historical and set `docs/ongoing_migration.md` as the authoritative no-wrapper direct SDL2/glm direction.
- Issue #407 implementation was blocked for this run because active parallel edits already touched files that would be part of the required component-type migration blast radius; avoided cross-agent overwrite risk.

### R20: Round-2 architecture audit learning (post-PR #408)

**Date:** 2026-05-10  
**Scope:** Read-only audit of `app/`, public headers, `CMakeLists.txt`, and migration docs.

- Closing a broad boundary issue can still leave a thinner residual abstraction seam: replacing backend handles with project-local proxy structs (`Vec2f`/`Mat4f`/`ColorRGBA8`) plus conversion helpers still behaves like a compatibility layer when docs explicitly ban wrappers.
- Component headers should carry mutable state shape, not transition helpers and asset catalogs. Keeping behavior/content tables out of `app/components/*.h` reduces include-surface coupling and keeps ECS boundaries easier to evolve.
- Filed follow-up issues: #411 and #412.

### R21: Round-3 architecture boundary residuals (post-PR #427)

**Date:** 2026-05-10  
**Scope:** Read-only architecture audit focused on residual non-ECS/global-state seams in runtime/UI plumbing.

- Filed #432: screen controllers retain mutable file-scope singleton state (`RGuiScreenController` instances) outside `entt::registry`.
- Filed #433: WASM loop ownership in `platform_display.cpp` is held in file-scope global `g_state` (registry pointer + accumulator) instead of explicit registry-owned state.
- Duplicate check completed against open issues before filing; no matching open issues covered these exact boundary seams.

### 2026-05-11T22:13:01.838-07:00: High-level system design synthesis

- The runtime architecture is cleanly expressed as a layered pipeline around one registry: **variable-rate shell** (input poll, camera, world/UI render, audio/haptics drain) wrapped around a **fixed-step deterministic core** (`tick_fixed_systems` + `tick_playing_systems`).
- The strongest boundary for diagrams/reviews is **semantic intent flow**: input/UI emit dispatcher events, `game_state_system` is the authoritative event drain + phase gate, then Playing-only systems execute in explicit order, then feedback/cleanup systems finalize frame state.
- Treat beatmap/music loading as a **session bootstrap subsystem** (`setup_play_session` + `beat_map_loader`) feeding immutable gameplay content (`BeatMap`, `SongState`) into the fixed-step loop, not as part of hot-path per-frame logic.
