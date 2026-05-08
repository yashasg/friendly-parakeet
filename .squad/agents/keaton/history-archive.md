# Keaton — History Archive

## Earlier Sessions (Archived 2026-05-08)

### 2026-05-03 — Core Refactoring & Migration

Archived initial motion_system refactors, Velocity migration, Position elimination (15+ files, atomic delivery via git history). All core systems migrated to WorldTransform+MotionVelocity. Key patterns: phantom bench fixtures, structural view optimization, pre-dispatch wins. All 18 modules now 🟢.

### 2026-05-04T04:55:12Z — Decision Registry: Issue #350

Merged inbox decision file to `.squad/decisions.md` (`# Decision: Keep magic_enum...`). Status: Recommended (GitHub issue #350 comment posted, gate cleared by Verbal).

### 2026-05-08T07:08:55.899Z — Raylib branch audit (first pass)

Definite candidates: `shape_vertices.h`, `raylib_gesture_input.h`, `text_renderer.h/cpp`. Retention: camera_resources RAII, ECS components. Impact: 200–300 lines dead weight.

### 2026-05-08T17:06:16Z — Raylib branch audit (second pass, clarified)

DELETE: `text_renderer.h/cpp`, `raylib_gesture_input.h`. HOLD: `shape_vertices.h` (pending architecture review). KEEP: gesture_routing, ECS, components. Ready for implementation.

### 2026-05-08T17:25:41Z — Raylib wrapper deletion (sync)

Deleted: `text_renderer`, `raylib_gesture_input`, tests. Updated call sites. Build blocked by pre-existing `constants::PTS_LANE_PUSH`. Implementation complete, awaiting constants fix.

### 2026-05-08T10:47:42.149-07:00 — Removed push-lane obstacle support

Push-lane obstacles retired. Removed `constants::PTS_LANE_PUSH`. Active removal: `obstacle.h`, `obstacle_entity.cpp`, `collision_system.cpp`, `playing_systems_runner.cpp`, `gameplay_intents.h`. `NonScorableTag` retained. Build verified: 2148 assertions / 774 tests PASSED.

### 2026-05-08T17:57:30Z — Push-lane removal approved & logged

Work reviewed and approved by Kujan. All decisions and orchestration logged. Next: pointer input cleanup or coordinate with Kujan on expanded scope.

### 2026-05-08T11:03:33-07:00 — Post-cleanup audit: full app/ directory

Analyzed 134 files. Result: STABLE. No additional cleanup candidates (text_renderer, raylib_gesture_input already removed). shape_vertices.h KEEP for now (geometry tables are production-critical for floor annulus; V2 duplicate is low-priority). Ready for feature development.

### 2026-05-08T18:08:07Z — TEAM AUDIT SUMMARY

Keaton + Keyser. Confirmed: CIRCLE table critical for floor rings. Dead code: HEXAGON, SQUARE, TRIANGLE (tests only). No new cleanup candidates. Shape geometry audit complete.

### 2026-05-08T11:14:33.527-07:00 — Circle floor ring: 2D verification

Verified XZ-plane implementation (lines 97–133 in game_render_system.cpp). Y hardcoded to 0.0f. Raylib 2D APIs not drop-in replacements. CIRCLE required, test-only shapes removable.

### 2026-05-08T11:17:28.150-07:00 — shape_vertices cleanup recommendation

CIRCLE production-critical. HEXAGON/SQUARE/TRIANGLE referenced only by tests/benchmarks, not app runtime. Raylib DrawRing is 2D screen-space, not applicable to BeginMode3D XZ plane. Key files: shape_vertices.h, game_render_system.cpp, test_perspective.cpp, bench_perspective.cpp, CMakeLists.txt.

### 2026-05-08T18:20:43.000Z — Keaton inspection completed

Two decisions recorded: (1) Circle Floor Ring 2D implementation confirmed; (2) shape_vertices cleanup sequencing recommended. Ready for team review.
