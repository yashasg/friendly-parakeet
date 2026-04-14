#pragma once
#include "constants.h"
#include "components/player.h"
#include <raylib.h>
#include <algorithm>

namespace perspective {

// Pre-computed reciprocal — multiplication instead of division in the hot path.
// Called 1000–10000× per frame; division is 10–20 cycles, multiply is 3–5.
constexpr float CENTER    = constants::SCREEN_W / 2.0f;
constexpr float VP_Y      = constants::VANISHING_POINT_Y;
constexpr float RANGE     = static_cast<float>(constants::SCREEN_H) - VP_Y;
constexpr float INV_RANGE = 1.0f / RANGE;

// Compute depth factor for a given y.  Useful when callers need depth
// separately (e.g., floor shapes that scale size by depth then draw flat).
inline float depth(float y) {
    return std::clamp((y - VP_Y) * INV_RANGE, 0.01f, 1.5f);
}

// Project a world-space vertex.  x is warped toward centre; y is unchanged.
inline Vector2 project(float x, float y) {
    float d = depth(y);
    return { CENTER + (x - CENTER) * d, y };
}

// Project just the x component (avoids constructing a Vector2).
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

} // namespace perspective
