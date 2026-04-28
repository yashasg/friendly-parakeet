# Validation Decision: #344 P0/P1 Archetype Factory Consolidation — PASSED

**Date:** 2026-04-28
**Author:** Baer
**Scope:** `app/archetypes/`, CMakeLists.txt, `tests/test_obstacle_archetypes.cpp`, `tests/test_player_archetype.cpp`

## Outcome

Implementation is complete and validated. No source changes required.

## Commands Run and Results

| Command | Result |
|---|---|
| `cmake -B build -S . -Wno-dev` | ✅ Configure OK |
| `cmake --build build --target shapeshifter_tests` | ✅ Build OK, zero warnings |
| `./build/shapeshifter_tests "[archetype]"` | ✅ 94 assertions / 21 tests PASS |
| `./build/shapeshifter_tests "[archetype][player]"` | ✅ 20 assertions / 7 tests PASS |
| `./build/shapeshifter_tests` (full suite) | ✅ 2749 assertions / 828 tests PASS |

## P0/P1 Criteria Met

- `app/archetypes/obstacle_archetypes.*` present; `app/systems/obstacle_archetypes.*` deleted — no split-brain include paths remain.
- `create_obstacle_base` is the single factory for `ObstacleTag + Velocity + DrawLayer`; called by both `beat_scheduler_system` and `obstacle_spawn_system`.
- `apply_obstacle_archetype` is canonical for all 8 obstacle kinds; tests lock the component set per kind.
- `create_player_entity` is canonical for production player layout; `play_session.cpp` routes through it; dedicated test file covers all component invariants.
- CMakeLists.txt updated; `ARCHETYPE_SOURCES` glob compiles archetypes into `shapeshifter_lib`.

## Known Non-Blocking Notes

1. **Legacy obstacle helpers** (`make_shape_gate`, etc. in `test_helpers.h`) still manually emplace `ObstacleTag + Velocity + DrawLayer` rather than calling `create_obstacle_base`. Intentional — these are integration test scaffolding that other system tests depend on. Not a split-brain risk since they are test-only paths.
2. **LanePush color/height** differs between `make_lane_push` (test_helpers.h) and `apply_obstacle_archetype`. No test asserts these values for LanePush; the production path uses the archetype. Not a correctness risk.
3. **No isolated unit test** for `create_obstacle_base` itself. Implicitly covered by integration tests that exercise the scheduler/spawner. Low risk.

## Recommendation

Ready for reviewer sign-off on #344. No further P0/P1 items outstanding.
