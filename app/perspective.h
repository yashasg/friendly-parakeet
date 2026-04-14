#pragma once
#include "constants.h"
#include "components/player.h"
#include <raylib.h>

namespace perspective {

inline Vector2 project(float x, float y) {
    constexpr float center = constants::SCREEN_W / 2.0f;
    constexpr float vp_y   = constants::VANISHING_POINT_Y;
    constexpr float range  = static_cast<float>(constants::SCREEN_H) - vp_y;

    float depth = (y - vp_y) / range;
    if (depth < 0.01f) depth = 0.01f;
    if (depth > 1.5f)  depth = 1.5f;

    return { center + (x - center) * depth, y };
}

inline float project_x(float x, float y) {
    return project(x, y).x;
}

void draw_rect(float x, float y, float w, float h, Color c);
void draw_shape(Shape shape, float cx, float cy, float size, Color c);
void draw_line(float x1, float y1, float x2, float y2, float thick, Color c);
void draw_ring(float cx, float cy, float inner_r, float outer_r, int segments, Color c);
void draw_rect_lines(float x, float y, float w, float h, float thick, Color c);
void draw_tri_lines(Vector2 v1, Vector2 v2, Vector2 v3, Color c);

} // namespace perspective
