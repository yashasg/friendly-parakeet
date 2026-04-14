#include "perspective.h"
#include "shape_vertices.h"
#include <raylib.h>

namespace perspective {

// ── Filled rectangle (axis-aligned in world → trapezoid after projection) ────
void draw_rect(float x, float y, float w, float h, Color c) {
    // Four corners at their own y-values
    float top_y = y;
    float bot_y = y + h;

    Vector2 tl = project(x,     top_y);
    Vector2 tr = project(x + w, top_y);
    Vector2 br = project(x + w, bot_y);
    Vector2 bl = project(x,     bot_y);

    // Two triangles — CCW winding for raylib
    DrawTriangle(tl, bl, tr, c);
    DrawTriangle(tr, bl, br, c);
}

// ── Filled shape (Circle, Square, Triangle, Hexagon) ─────────────────────────
void draw_shape(Shape shape, float cx, float cy, float size, Color c) {
    switch (shape) {
        case Shape::Circle: {
            float r = size / 2.0f;
            Vector2 centre = project(cx, cy);
            for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::CIRCLE_SEGMENTS;
                Vector2 v1 = project(cx + shape_verts::CIRCLE[i].x    * r,
                                     cy + shape_verts::CIRCLE[i].y    * r);
                Vector2 v2 = project(cx + shape_verts::CIRCLE[next].x * r,
                                     cy + shape_verts::CIRCLE[next].y * r);
                // CCW: centre, v2, v1  (vertices go CCW in the table,
                // so fan triangle centre→v2→v1 keeps CCW screen winding)
                DrawTriangle(centre, v2, v1, c);
            }
            break;
        }
        case Shape::Square: {
            float half = size / 2.0f;
            Vector2 corners[4];
            for (int i = 0; i < 4; ++i) {
                corners[i] = project(cx + shape_verts::SQUARE[i].x * half,
                                     cy + shape_verts::SQUARE[i].y * half);
            }
            // TL(0), TR(1), BR(2), BL(3) — two CCW triangles
            DrawTriangle(corners[0], corners[3], corners[1], c);
            DrawTriangle(corners[1], corners[3], corners[2], c);
            break;
        }
        case Shape::Triangle: {
            float half = size / 2.0f;
            Vector2 verts[3];
            for (int i = 0; i < 3; ++i) {
                verts[i] = project(cx + shape_verts::TRIANGLE[i].x * half,
                                   cy + shape_verts::TRIANGLE[i].y * half);
            }
            // Apex(0), BaseLeft(1), BaseRight(2) — CCW
            DrawTriangle(verts[0], verts[1], verts[2], c);
            break;
        }
        case Shape::Hexagon: {
            float radius = size * 0.6f;  // matches existing render_system scale
            Vector2 centre = project(cx, cy);
            for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::HEX_SEGMENTS;
                Vector2 v1 = project(cx + shape_verts::HEXAGON[i].x    * radius,
                                     cy + shape_verts::HEXAGON[i].y    * radius);
                Vector2 v2 = project(cx + shape_verts::HEXAGON[next].x * radius,
                                     cy + shape_verts::HEXAGON[next].y * radius);
                // CCW fan: centre→v2→v1
                DrawTriangle(centre, v2, v1, c);
            }
            break;
        }
    }
}

// ── Projected line ───────────────────────────────────────────────────────────
void draw_line(float x1, float y1, float x2, float y2, float thick, Color c) {
    Vector2 a = project(x1, y1);
    Vector2 b = project(x2, y2);
    DrawLineEx(a, b, thick, c);
}

// ── Projected ring (annulus) ─────────────────────────────────────────────────
void draw_ring(float cx, float cy, float inner_r, float outer_r, int segments, Color c) {
    int seg = (segments > 0 && segments <= shape_verts::CIRCLE_SEGMENTS)
              ? segments : shape_verts::CIRCLE_SEGMENTS;

    for (int i = 0; i < seg; ++i) {
        // Map evenly across the full 0..CIRCLE_SEGMENTS table so the ring
        // always closes correctly, regardless of the requested segment count.
        int idx      = (i       * shape_verts::CIRCLE_SEGMENTS) / seg;
        int next_idx = ((i + 1) * shape_verts::CIRCLE_SEGMENTS) / seg
                       % shape_verts::CIRCLE_SEGMENTS;

        Vector2 outer1 = project(cx + shape_verts::CIRCLE[idx].x      * outer_r,
                                 cy + shape_verts::CIRCLE[idx].y      * outer_r);
        Vector2 outer2 = project(cx + shape_verts::CIRCLE[next_idx].x * outer_r,
                                 cy + shape_verts::CIRCLE[next_idx].y * outer_r);
        Vector2 inner1 = project(cx + shape_verts::CIRCLE[idx].x      * inner_r,
                                 cy + shape_verts::CIRCLE[idx].y      * inner_r);
        Vector2 inner2 = project(cx + shape_verts::CIRCLE[next_idx].x * inner_r,
                                 cy + shape_verts::CIRCLE[next_idx].y * inner_r);

        // Two CCW triangles per segment forming the ring quad
        DrawTriangle(outer1, outer2, inner1, c);
        DrawTriangle(inner1, outer2, inner2, c);
    }
}

// ── Projected rectangle outline ──────────────────────────────────────────────
void draw_rect_lines(float x, float y, float w, float h, float thick, Color c) {
    float top_y = y;
    float bot_y = y + h;

    Vector2 tl = project(x,     top_y);
    Vector2 tr = project(x + w, top_y);
    Vector2 br = project(x + w, bot_y);
    Vector2 bl = project(x,     bot_y);

    DrawLineEx(tl, tr, thick, c);
    DrawLineEx(tr, br, thick, c);
    DrawLineEx(br, bl, thick, c);
    DrawLineEx(bl, tl, thick, c);
}

// ── Projected triangle outline ───────────────────────────────────────────────
void draw_tri_lines(Vector2 v1, Vector2 v2, Vector2 v3, Color c) {
    Vector2 p1 = project(v1.x, v1.y);
    Vector2 p2 = project(v2.x, v2.y);
    Vector2 p3 = project(v3.x, v3.y);

    DrawLineV(p1, p2, c);
    DrawLineV(p2, p3, c);
    DrawLineV(p3, p1, c);
}

} // namespace perspective
