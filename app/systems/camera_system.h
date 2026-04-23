#pragma once
#include "../components/camera.h"
#include "../components/player.h"
#include <entt/entt.hpp>
#include <raylib.h>

namespace camera {

// ── Standalone 3D shape draw ─────────────────────────────────────────────────
// Draws a filled shape on the XZ plane at height y_3d.
// Wraps its own RL_TRIANGLES batch; safe to call outside a rlBegin/rlEnd pair.
void draw_shape(Shape shape, float cx, float y_3d, float cz, float size, Color c);

// ── GPU-batched render passes ────────────────────────────────────────────────
// All geometry is emitted on the XZ plane in world space.
// Camera3D perspective projection handles foreshortening automatically.
//
//   Pass 1: RL_LINES      — floor connectors + outlines + corridor edges
//   Pass 2: RL_TRIANGLES  — floor rings (lane 0 circles)
//   Pass 3: RL_QUADS      — obstacle rects + particle rects
//   Pass 4: RL_TRIANGLES  — ghost shapes + player shape

void flush_floor_lines(entt::registry& reg, const FloorParams& fp);
void flush_floor_rings(const FloorParams& fp);
void flush_world_rects(entt::registry& reg);
void flush_gameplay_tris(entt::registry& reg);

} // namespace camera
