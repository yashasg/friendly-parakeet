#pragma once
#include "constants.h"
#include "components/player.h"
#include <entt/entt.hpp>
#include <raylib.h>
#include <algorithm>
#include <cstdint>

namespace perspective {

// Pre-computed reciprocal — multiplication instead of division in the hot path.
constexpr float CENTER    = constants::SCREEN_W / 2.0f;
constexpr float VP_Y      = constants::VANISHING_POINT_Y;
constexpr float RANGE     = static_cast<float>(constants::SCREEN_H) - VP_Y;
constexpr float INV_RANGE = 1.0f / RANGE;

inline float depth(float y) {
    return std::clamp((y - VP_Y) * INV_RANGE, 0.01f, 1.5f);
}

inline Vector2 project(float x, float y) {
    float d = depth(y);
    return { CENTER + (x - CENTER) * d, y };
}

inline float project_x(float x, float y) {
    float d = depth(y);
    return CENTER + (x - CENTER) * d;
}

void draw_rect(float x, float y, float w, float h, Color c);
void draw_shape(Shape shape, float cx, float cy, float size, Color c);
void draw_line(float x1, float y1, float x2, float y2, float thick, Color c);
void draw_ring(float cx, float cy, float inner_r, float outer_r, int segments, Color c);
void draw_rect_lines(float x, float y, float w, float h, float thick, Color c);
void draw_tri_lines(Vector2 v1, Vector2 v2, Vector2 v3, Color c);

// ── Batched rect rendering from ECS ─────────────────────────────────────────
// Queries the registry for all obstacle and particle entities, computes
// their perspective-projected rect geometry, and submits everything in a
// single rlgl RL_QUADS batch.  No manual push() from application code —
// the batch reads the ECS by tag.
void flush_world_rects(entt::registry& reg);

} // namespace perspective
