#include "perspective.h"
#include "shape_vertices.h"
#include <raylib.h>

namespace perspective {

// ── Filled rectangle (axis-aligned in world → trapezoid after projection) ────
// Depth only depends on y, and a rect has only 2 y-values (top/bottom).
// Compute depth once per scanline instead of once per corner.
void draw_rect(float x, float y, float w, float h, Color c) {
    float d_top = depth(y);
    float d_bot = depth(y + h);

    float tl_x = CENTER + (x     - CENTER) * d_top;
    float tr_x = CENTER + (x + w - CENTER) * d_top;
    float bl_x = CENTER + (x     - CENTER) * d_bot;
    float br_x = CENTER + (x + w - CENTER) * d_bot;

    Vector2 tl = {tl_x, y};
    Vector2 tr = {tr_x, y};
    Vector2 bl = {bl_x, y + h};
    Vector2 br = {br_x, y + h};

    DrawTriangle(tl, bl, tr, c);
    DrawTriangle(tr, bl, br, c);
}

// ── Filled shape (Circle, Square, Triangle, Hexagon) ─────────────────────────
void draw_shape(Shape shape, float cx, float cy, float size, Color c) {
    switch (shape) {
        case Shape::Circle: {
            float r = size / 2.0f;
            Vector2 centre = project(cx, cy);
            // Pre-project first vertex; reuse as prev in the loop
            Vector2 prev = project(cx + shape_verts::CIRCLE[0].x * r,
                                   cy + shape_verts::CIRCLE[0].y * r);
            for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::CIRCLE_SEGMENTS;
                Vector2 cur = project(cx + shape_verts::CIRCLE[next].x * r,
                                      cy + shape_verts::CIRCLE[next].y * r);
                DrawTriangle(centre, cur, prev, c);
                prev = cur;
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
            float radius = size * 0.6f;
            Vector2 centre = project(cx, cy);
            Vector2 prev = project(cx + shape_verts::HEXAGON[0].x * radius,
                                   cy + shape_verts::HEXAGON[0].y * radius);
            for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::HEX_SEGMENTS;
                Vector2 cur = project(cx + shape_verts::HEXAGON[next].x * radius,
                                      cy + shape_verts::HEXAGON[next].y * radius);
                DrawTriangle(centre, cur, prev, c);
                prev = cur;
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

        DrawTriangle(outer1, outer2, inner1, c);
        DrawTriangle(inner1, outer2, inner2, c);
    }
}

// ── Projected rectangle outline ──────────────────────────────────────────────
void draw_rect_lines(float x, float y, float w, float h, float thick, Color c) {
    float d_top = depth(y);
    float d_bot = depth(y + h);

    Vector2 tl = {CENTER + (x     - CENTER) * d_top, y};
    Vector2 tr = {CENTER + (x + w - CENTER) * d_top, y};
    Vector2 br = {CENTER + (x + w - CENTER) * d_bot, y + h};
    Vector2 bl = {CENTER + (x     - CENTER) * d_bot, y + h};

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
