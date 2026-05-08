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
