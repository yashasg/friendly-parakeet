# Decision: Canonical Obstacle + Player Archetype Factories (#344)

**Date:** 2026-05-XX  
**Author:** Keaton  
**Scope:** `app/archetypes/`

## Decisions Made

### 1. `create_obstacle_base` owns the pre-bundle
`ObstacleTag + Velocity(0, speed) + DrawLayer(Layer::Game)` is now emplaced exclusively by `create_obstacle_base(reg, speed)` in `app/archetypes/obstacle_archetypes.*`.  Both `beat_scheduler_system` and `obstacle_spawn_system` call it; test helpers in `tests/test_helpers.h` call it too.  Direct emplacing of this triple outside that factory is a violation.

### 2. `create_player_entity` is the single source of truth for the production player
`app/archetypes/player_archetype.*` owns the canonical player layout: `PlayerTag + Position(LANE_X[1], PLAYER_Y) + PlayerShape(Hexagon) + ShapeWindow(Idle, Hexagon) + Lane + VerticalState + Color{80,180,255} + DrawSize(PLAYER_SIZE) + DrawLayer(Game)`.  `play_session.cpp` and `make_rhythm_player` route through it.

### 3. `make_player` in test_helpers intentionally kept at Circle default
`make_player` starts as Circle because non-rhythm tests rely on pressing Circle as a no-op (e.g., "no shape change when same shape pressed"). Changing it would break 15+ tests without adding production fidelity.  Tests requiring the canonical rhythm player use `make_rhythm_player` (which calls `create_player_entity`).
