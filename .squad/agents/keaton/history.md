# Keaton — History

## Core Context

- **Owner:** yashasg

## Summary: Work Through 2026-05-03

Archived initial motion_system refactors, Velocity migration, Position elimination (15+ files, atomic atomic delivery via git history). All core systems migrated to WorldTransform+MotionVelocity. See git commit history for detailed walk-through (70f6436 and predecessors).

Key patterns learned: phantom bench fixtures, structural view optimization dangers, pre-dispatch dispatch wins. All 18 modules now 🟢. Loop reached diminishing returns 2026-05-03.

---

### 2026-05-04T04:55:12Z — Decision Registry: Issue #350 recommendation merged

- **Scribe action:** Merged inbox decision file to `.squad/decisions.md` (`# Decision: Keep magic_enum...`)
- **Orchestration log written:** `.squad/orchestration-log/2026-05-04T04:55:12Z-keaton.md`
- **Status in registry:** Recommended (GitHub issue #350 comment posted, gate cleared by Verbal)

### 2026-05-08T07:08:55.899Z — Raylib branch read-only cleanup audit

**Task:** Audit current raylib branch for cleanup candidates: raylib wrappers, raylib type duplicates, unnecessary abstraction layers, project duplicate implementations.

**Audit Results**

**Definite cleanup candidates (high priority):**
1. `app/util/shape_vertices.h` — custom V2 duplicate; replace with raylib Vector2
2. `app/input/raylib_gesture_input.h` — thin gesture adapter layer; consolidate into caller or remove
3. `app/ui/text_renderer.h/cpp` — text measurement/render wrapper; likely duplicates raylib functionality

**Retention decisions (keep as-is):**
- `app/systems/camera_resources.h` — RAII safety pattern required; no raylib equivalent
- Domain ECS components — essential to architecture; not raylib duplicates

**Cleanup impact:** 3 files (V2, gesture, text); estimated 200–300 lines of dead weight.

**Next steps:** Schedule cleanup task with engineering team for prioritization and batch execution.

### 2026-05-08T17:06:16Z — Raylib branch cleanup audit (second pass, directives clarified)

**Task:** Second read-only audit of raylib-branch cleanup candidates after clarified directives from team feedback.

**Audit Results (Confirmed)**

**Definite cleanup candidates:**
1. `app/util/text_renderer.h/cpp` — DELETE
2. `app/input/raylib_gesture_input.h` — DELETE

**Hold for architecture review:**
- `app/util/shape_vertices.h` — custom V2/geometry — HOLD pending architecture team review; geometry tables support floor annulus rendering and raylib does not provide direct annulus helper.

**Clarification on gesture_routing.cpp:**
- `app/systems/gesture_routing.cpp` — KEEP (domain logic, not raylib-specific cleanup scope)

**Preserved components:**
- ECS components & resource RAII — KEEP (core architecture)

**Status:** Ready for implementation phase on text_renderer and raylib_gesture_input. Architecture team to review shape_vertices before any deletion decision.

### 2026-05-08T17:25:41Z — Raylib wrapper deletion (sync implementation)

**Task:** Delete approved raylib wrappers `text_renderer` and `raylib_gesture_input`; update direct raylib call sites; rebuild and validate.

**Scope Completed**
- Deleted: `app/ui/text_renderer.h/cpp`, `app/input/raylib_gesture_input.h`, `tests/test_raylib_gesture_input.cpp`
- Updated call sites: `app/game_loop.cpp`, `app/systems/ui_render_system.cpp`, `app/systems/input_system.cpp`, `CMakeLists.txt`
- Build verification: blocked by pre-existing undefined constant `constants::PTS_LANE_PUSH` in `app/entities/obstacle_entity.cpp` (out of scope for cleanup task)

**Status:** Implementation complete. Awaiting constants fix (routing to Coordinator for assignment).

## Learnings

### 2026-05-08T10:47:42.149-07:00 — Removed push-lane obstacle support

- Push-lane obstacles are retired; do not reintroduce `constants::PTS_LANE_PUSH` to satisfy stale tests.
- Active removal touched `app/components/obstacle.h`, `app/entities/obstacle_entity.cpp`, `app/systems/collision_system.cpp`, `app/systems/playing_systems_runner.cpp`, `app/components/gameplay_intents.h`, and the related obstacle/scoring/collision tests.
- `NonScorableTag` remains as a generic scoring escape hatch and is covered by `tests/test_scoring_system.cpp`; it is no longer tied to push-lane obstacles.
- User confirmed floor shapes are 2D, but floor-shape/`shape_vertices` cleanup was intentionally deferred.
- Validation path for this task: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh`, then `./build/shapeshifter_tests`.

## Session 2026-05-08: Push-lane removal approved & logged

**Timestamp:** 2026-05-08T17:57:30Z

Keaton's push-lane obstacle removal work was reviewed and approved by Kujan. All decisions and orchestration events have been logged to team artifacts.

### Work summary
- Push-lane enum value, spawn branch, collision path, execution order removed
- Obsolete constant `PTS_LANE_PUSH` references deleted
- Push-lane-specific tests removed; generic `NonScorableTag` coverage retained
- Validation: build + 2148 assertions / 774 tests — PASSED
- Floor-shape cleanup deferred as separate task

### Team decisions logged
- `keaton-remove-push-lanes.md` → decisions.md
- `kujan-push-lane-cleanup-approved.md` → decisions.md
- Orchestration logs written
- Session closed by Scribe

### Next
- Pointer input cleanup work available
- Coordinate with Kujan if additional cleanup scope expands
