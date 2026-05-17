#pragma once

#include <entt/entt.hpp>

// Approach ring envelope system (issue #1297).
//
// Reads beat-scheduler state (`SongState`) and the nearest required-shape
// obstacle distance per shape, and writes the per-lane-button
// `ApproachRing` component on every `LaneButtonTag` entity. The render
// pass in `ui_render_system` consumes the component and draws the ring
// without re-running the math.
//
// The system is a no-op outside `GamePhasePlayingTag` and when no lane
// button entities exist (e.g. between `despawn_gameplay_screen()` and
// the next `spawn_gameplay_screen()` invocation).
void approach_ring_envelope_system(entt::registry& reg);
