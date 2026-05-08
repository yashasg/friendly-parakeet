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
