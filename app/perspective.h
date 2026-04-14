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

// ── GPU-batched render passes ────────────────────────────────────────────────
// Three ordered passes, each a single rlgl begin/end.  Sorted by primitive
// type (lines → quads → triangles) to minimize GPU state changes.
//
//   Pass 1: RL_LINES      — corridor edges, floor connectors, floor outlines
//   Pass 2: RL_QUADS      — obstacle rects, particle rects
//   Pass 3: RL_TRIANGLES  — floor rings, ghost shapes, player shape

// Floor geometry params (computed per-frame from SongState pulse).
struct FloorParams {
    float size;
    float half;
    float thick;
    uint8_t alpha;
};

void flush_world_lines(entt::registry& reg, const FloorParams& fp);
void flush_world_rects(entt::registry& reg);
void flush_world_tris(entt::registry& reg, const FloorParams& fp);

} // namespace perspective
