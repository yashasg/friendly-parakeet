#pragma once

#include "../components/player.h"
#include "shape_lane_mapping.h"

#include <array>

#include <raylib.h>

// Per-shape flat 2D draw table (issue #1202/#1204 mechanic; relocated out of
// `gameplay_hud_screen_controller.cpp` for shared use by `ui_render_system`
// per the Title-screen migration in #1294).
//
// Each former `switch (shape)` case is its own row in a function-pointer
// column indexed by `shape_index(shape)`. The dispatcher reads the column
// directly rather than branching on the discriminator. Same mechanic as
// `kShapeFlatDrawFns` in PR #1259 — function-pointer-per-row, no `switch`.

namespace shape_draw_2d {

inline void draw_circle(float cx, float cy, float size, Color color) {
    DrawCircleV({cx, cy}, size / 2.0f, color);
}

inline void draw_square(float cx, float cy, float size, Color color) {
    const float half = size / 2.0f;
    DrawRectangleRec({cx - half, cy - half, size, size}, color);
}

inline void draw_triangle(float cx, float cy, float size, Color color) {
    const float half = size / 2.0f;
    DrawTriangle({cx, cy - half}, {cx - half, cy + half}, {cx + half, cy + half}, color);
}

inline void draw_hexagon(float cx, float cy, float size, Color color) {
    DrawPoly({cx, cy}, 6, size * 0.6f, -90.0f, color);
}

using DrawFn = void (*)(float, float, float, Color);

inline constexpr std::array<DrawFn, kShapeCount> kFlatDrawFns{
    &draw_circle,
    &draw_square,
    &draw_triangle,
    &draw_hexagon,
};

inline void draw_flat(Shape shape, float cx, float cy, float size, Color color) {
    const int idx = shape_index(shape);
    if (idx < 0) return;
    kFlatDrawFns[static_cast<size_t>(idx)](cx, cy, size, color);
}

}  // namespace shape_draw_2d
