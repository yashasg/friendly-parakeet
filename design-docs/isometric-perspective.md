# Isometric Perspective Effect — Technical Spec (Per-Vertex)

## Goal

Make the flat 2D game look like the player is viewing a track receding
into the distance.  Every **vertex** of every world-space primitive is
individually projected through the perspective math, so shapes naturally
distort — a square becomes a trapezoid, a triangle's top vertex pulls
inward, circles compress at the top.

```
  Current (flat)                  Target (pseudo-isometric)

  ┌──────────────┐                  ╱──────────╲       ← far / narrow
  │  ○  ■  ▲     │                 ╱  ○  ■  ▲   ╲
  │              │                ╱              ╲
  │              │               ╱                ╲
  │              │              ╱                  ╲
  │   PLAYER     │             ╱     PLAYER         ╲
  └──────────────┘            ╱──────────────────────╲ ← near / full-width
```

The three lanes converge toward a vanishing point above the screen:

```
              * VP (y = -1060, above screen)
             ╱|╲
            ╱ | ╲
           ╱  |  ╲
    y=0   ╱───┼───╲──── top of viewport
         ╱    |    ╲
        ╱   ○ | ■   ╲
       ╱      |      ╲
 y=1280╱───PLAYER───╲── bottom of viewport
      ╱────────────────╲
```

---

## Why Per-VERTEX, Not Per-Object Uniform Scale

Transforming (centre, size) uniformly keeps shapes geometrically flat:

```
  Uniform scale (wrong)          Per-vertex (correct)

  ┌────┐  ← same width           ╱──╲   ← top edge narrower
  │    │     top & bottom        ╱    ╲     (vertices at smaller y
  └────┘                        ╱──────╲    pulled closer to centre)

  Square stays square.          Square becomes trapezoid.
  Still looks flat.             Looks like a surface receding away.
```

Each vertex has its **own y-coordinate**, and therefore its own depth
factor.  A shape spanning y=400..464 will have its top vertices more
compressed than its bottom vertices, because the top is "further away."

---

## Core Principle: Project Every Vertex

The single atomic operation is:

```
  project_vertex(x, y)  →  (x', y)

  x' = CENTER_X + (x - CENTER_X) * depth(y)
  y' = y                   // y is unchanged

  where:
    depth(y) = (y - VP_Y) / (SCREEN_H - VP_Y)
```

**Every point that gets handed to a raylib draw function** goes through
this.  Not centres.  Not bounding boxes.  Individual vertices.

```
Parameters (constants.h):
  CENTER_X  = SCREEN_W / 2.0    = 360.0
  VP_Y      = -1060.0           // vanishing point, above the screen (~17° convergence)
  BOTTOM_Y  = SCREEN_H          = 1280.0
```

### Visual Example — Lane Positions

```
  y       depth    scale    Lane0(180)→x'   Lane1(360)→x'   Lane2(540)→x'
  ────    ─────    ─────    ──────────────   ──────────────   ──────────────
 -1060    0.000    0.000      360 (VP)         360              360
     0    0.453    0.453      279              360              441
   640    0.726    0.726      229              360              491
  1280    1.000    1.000      180              360              540
```

```
     279  360  441       ← y=0   (top, scale=0.45)
      :   :    :
    229   360   491      ← y=640 (mid, scale=0.73)
      :   :      :
   180    360    540     ← y=1280 (bottom, scale=1.0)
```

---

## Per-Shape Vertex Projection

### Square (4 vertices → trapezoid)

```
  Before (flat):                After (per-vertex):

  V0─────────V1                 V0'───V1'        ← both at y=cy-half
  │          │                   ╱      ╲           (smaller depth, pulled in)
  │   (cx)   │                  ╱        ╲
  │          │                 ╱          ╲
  V3─────────V2               V3'────────V2'     ← both at y=cy+half
                                                    (larger depth, wider)

  V0 = (cx-half, cy-half)  →  V0' = (proj_x(cx-half, cy-half), cy-half)
  V1 = (cx+half, cy-half)  →  V1' = (proj_x(cx+half, cy-half), cy-half)
  V2 = (cx+half, cy+half)  →  V2' = (proj_x(cx+half, cy+half), cy+half)
  V3 = (cx-half, cy+half)  →  V3' = (proj_x(cx-half, cy+half), cy+half)

  Draw as two triangles: (V0', V3', V1') and (V1', V3', V2')
```

### Triangle (3 vertices)

```
  Before:               After:

      V0                    V0'          ← apex at cy-half
     ╱  ╲                  ╱  ╲            (most compressed)
    ╱    ╲               ╱      ╲
  V1──────V2           V1'──────V2'      ← base at cy+half
                                           (least compressed)

  V0 = (cx,      cy-half)  →  V0' = (proj_x(cx,      cy-half), cy-half)
  V1 = (cx-half, cy+half)  →  V1' = (proj_x(cx-half, cy+half), cy+half)
  V2 = (cx+half, cy+half)  →  V2' = (proj_x(cx+half, cy+half), cy+half)
```

Triangle base stays wide, apex pulls toward centre — natural perspective.

### Circle (N vertices around perimeter)

```
  Before:               After:

    ╭────╮                ╭──╮           ← top vertices compressed
   │      │              ╱    ╲
   │      │             │      │         ← middle at cy, normal width
   │      │              ╲    ╱
    ╰────╯                ╰────╯         ← bottom vertices full width

  For each vertex i of N segments:
    angle = i * (2π / N)
    vx = cx + radius * cos(angle)
    vy = cy + radius * sin(angle)

    vx' = proj_x(vx, vy)
    vy' = vy

  Draw as N triangles from (proj_x(cx, cy), cy) to consecutive edge vertices.
```

The circle becomes slightly egg-shaped / wider at the bottom — this is
the correct perspective distortion.

### Hexagon (6 vertices)

Same as circle but with 6 points.  Each vertex projected independently.

### Rectangle obstacles (full-width bars, lane blocks, gates)

Already shown in the square example.  Each corner is a vertex with its
own y, so top edge is narrower than bottom edge.

```
  Full-width bar at y=300, h=20:

  Before:                        After:
  ┌──────────────────────┐       ╱──────────────────╲     ← y=300
  └──────────────────────┘       ╲────────────────────╱   ← y=320

  Top-left:  proj_x(0,   300)   Top-right:  proj_x(720, 300)
  Bot-left:  proj_x(0,   320)   Bot-right:  proj_x(720, 320)
```

---

## Pre-Cached Unit Vertex Buffers

**No trig at draw time.**  Every shape's vertices are pre-computed as
`constexpr` unit-space arrays (centred at origin, radius = 1.0).  At
draw time the hot loop is:

```
  for each unit vertex (ux, uy):
      world_x = cx + ux * radius
      world_y = cy + uy * radius
      screen_x = project_x(world_x, world_y)
      screen_y = world_y                        // y unchanged
      → emit to DrawTriangle
```

No `cosf`, no `sinf`, no `DEG2RAD` — just multiply + add + project.

### Unit vertex tables (constexpr)

All centred at (0, 0), unit radius = 1.0.  Stored in `app/shape_vertices.h`.

```
  CIRCLE (24 segments):
  ┌──────────────────────────────────────┐
  │  i=0:  (cos(0°),     sin(0°))       │    { 1.000,  0.000 }
  │  i=1:  (cos(15°),    sin(15°))      │    { 0.966,  0.259 }
  │  i=2:  (cos(30°),    sin(30°))      │    { 0.866,  0.500 }
  │  ...                                │
  │  i=23: (cos(345°),   sin(345°))     │    { 0.966, -0.259 }
  └──────────────────────────────────────┘
  24 perimeter vertices.  Centre is implicit (0, 0).
  Draw as 24 fan triangles: centre → v[i] → v[i+1 mod 24]

  HEXAGON (6 segments, pointy-top, offset -90°):
  ┌──────────────────────────────────────┐
  │  i=0:  (cos(-90°),   sin(-90°))     │    { 0.000, -1.000 }  ← top
  │  i=1:  (cos(-30°),   sin(-30°))     │    { 0.866, -0.500 }
  │  i=2:  (cos(30°),    sin(30°))      │    { 0.866,  0.500 }
  │  i=3:  (cos(90°),    sin(90°))      │    { 0.000,  1.000 }  ← bottom
  │  i=4:  (cos(150°),   sin(150°))     │    {-0.866,  0.500 }
  │  i=5:  (cos(210°),   sin(210°))     │    {-0.866, -0.500 }
  └──────────────────────────────────────┘
  6 perimeter vertices.  Radius scale = 0.6 applied at draw time.
  Draw as 6 fan triangles: centre → v[i] → v[i+1 mod 6]

  SQUARE (4 vertices, half-extent = 1.0):
  ┌──────────────────────────────────────┐
  │  v0 = (-1, -1)   v1 = (+1, -1)      │    top-left, top-right
  │  v2 = (+1, +1)   v3 = (-1, +1)      │    bot-right, bot-left
  └──────────────────────────────────────┘
  4 vertices.  Scale by half = size / 2.
  Draw as 2 triangles: (v0, v3, v1), (v1, v3, v2)

  TRIANGLE (3 vertices, half-extent = 1.0):
  ┌──────────────────────────────────────┐
  │  v0 = ( 0, -1)                       │    apex
  │  v1 = (-1, +1)                       │    base-left
  │  v2 = (+1, +1)                       │    base-right
  └──────────────────────────────────────┘
  3 vertices.  Scale by half = size / 2.
  Draw as 1 triangle: (v0, v1, v2)
```

### File: `app/shape_vertices.h`

```cpp
#pragma once
#include <cstdint>

namespace shape_verts {

struct V2 { float x, y; };

// ── Circle: 24 perimeter points, unit radius ────
constexpr int CIRCLE_SEGMENTS = 24;
constexpr V2 CIRCLE[CIRCLE_SEGMENTS] = {
    { 1.000000f,  0.000000f}, { 0.965926f,  0.258819f},
    { 0.866025f,  0.500000f}, { 0.707107f,  0.707107f},
    { 0.500000f,  0.866025f}, { 0.258819f,  0.965926f},
    { 0.000000f,  1.000000f}, {-0.258819f,  0.965926f},
    {-0.500000f,  0.866025f}, {-0.707107f,  0.707107f},
    {-0.866025f,  0.500000f}, {-0.965926f,  0.258819f},
    {-1.000000f,  0.000000f}, {-0.965926f, -0.258819f},
    {-0.866025f, -0.500000f}, {-0.707107f, -0.707107f},
    {-0.500000f, -0.866025f}, {-0.258819f, -0.965926f},
    { 0.000000f, -1.000000f}, { 0.258819f, -0.965926f},
    { 0.500000f, -0.866025f}, { 0.707107f, -0.707107f},
    { 0.866025f, -0.500000f}, { 0.965926f, -0.258819f},
};

// ── Hexagon: 6 perimeter points, pointy-top (-90° offset), unit radius ──
constexpr int HEX_SEGMENTS = 6;
constexpr V2 HEXAGON[HEX_SEGMENTS] = {
    { 0.000000f, -1.000000f},  //   0° - 90° = top
    { 0.866025f, -0.500000f},  //  60° - 90°
    { 0.866025f,  0.500000f},  // 120° - 90°
    { 0.000000f,  1.000000f},  // 180° - 90° = bottom
    {-0.866025f,  0.500000f},  // 240° - 90°
    {-0.866025f, -0.500000f},  // 300° - 90°
};

// ── Square: 4 corners, unit half-extent ─────────
constexpr V2 SQUARE[4] = {
    {-1.0f, -1.0f},  // top-left
    { 1.0f, -1.0f},  // top-right
    { 1.0f,  1.0f},  // bot-right
    {-1.0f,  1.0f},  // bot-left
};

// ── Triangle: 3 vertices, unit half-extent ──────
constexpr V2 TRIANGLE[3] = {
    { 0.0f, -1.0f},  // apex
    {-1.0f,  1.0f},  // base-left
    { 1.0f,  1.0f},  // base-right
};

} // namespace shape_verts
```

### Draw-time hot path (perspective.cpp)

Zero trig.  Just table lookup → scale → offset → project:

```cpp
void perspective::draw_shape(Shape shape, float cx, float cy,
                             float size, Color c) {
    switch (shape) {
        case Shape::Circle: {
            float r = size / 2.0f;
            Vector2 center = project(cx, cy);
            for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
                int j = (i + 1) % shape_verts::CIRCLE_SEGMENTS;
                // Lookup unit vertex, scale, offset — no trig
                float wx1 = cx + shape_verts::CIRCLE[i].x * r;
                float wy1 = cy + shape_verts::CIRCLE[i].y * r;
                float wx2 = cx + shape_verts::CIRCLE[j].x * r;
                float wy2 = cy + shape_verts::CIRCLE[j].y * r;
                // Project each vertex at its own y
                Vector2 p1 = project(wx1, wy1);
                Vector2 p2 = project(wx2, wy2);
                DrawTriangle(center, p2, p1, c);
            }
            break;
        }
        case Shape::Hexagon: {
            float r = size * 0.6f;
            Vector2 center = project(cx, cy);
            for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
                int j = (i + 1) % shape_verts::HEX_SEGMENTS;
                float wx1 = cx + shape_verts::HEXAGON[i].x * r;
                float wy1 = cy + shape_verts::HEXAGON[i].y * r;
                float wx2 = cx + shape_verts::HEXAGON[j].x * r;
                float wy2 = cy + shape_verts::HEXAGON[j].y * r;
                Vector2 p1 = project(wx1, wy1);
                Vector2 p2 = project(wx2, wy2);
                DrawTriangle(center, p2, p1, c);
            }
            break;
        }
        case Shape::Square: {
            float half = size / 2.0f;
            Vector2 v[4];
            for (int i = 0; i < 4; ++i) {
                float wx = cx + shape_verts::SQUARE[i].x * half;
                float wy = cy + shape_verts::SQUARE[i].y * half;
                v[i] = project(wx, wy);
            }
            // TL(0), BL(3), TR(1) then TR(1), BL(3), BR(2)
            DrawTriangle(v[0], v[3], v[1], c);
            DrawTriangle(v[1], v[3], v[2], c);
            break;
        }
        case Shape::Triangle: {
            float half = size / 2.0f;
            Vector2 v[3];
            for (int i = 0; i < 3; ++i) {
                float wx = cx + shape_verts::TRIANGLE[i].x * half;
                float wy = cy + shape_verts::TRIANGLE[i].y * half;
                v[i] = project(wx, wy);
            }
            DrawTriangle(v[0], v[1], v[2], c);
            break;
        }
    }
}
```

### Performance comparison

```
  Before (trig at draw time):           After (cached vertices):
  ┌──────────────────────────┐          ┌──────────────────────────┐
  │  per circle:             │          │  per circle:             │
  │    24× cosf()  = 24 trig│          │    24× table[i].x * r   │
  │    24× sinf()  = 24 trig│          │    24× table[i].y * r   │
  │    total: 48 trig calls  │          │    total: 0 trig calls   │
  │                          │          │    48 mul + 48 add       │
  │  per hexagon:            │          │  per hexagon:            │
  │    12 trig calls         │          │    0 trig calls          │
  └──────────────────────────┘          └──────────────────────────┘
```

---

## Implementation Structure

### New file: `app/shape_vertices.h`

Pre-cached constexpr unit vertex tables (shown above).

### New file: `app/perspective.h`

Pure helper — no game logic, no registry access:

```cpp
#pragma once
#include "constants.h"
#include <raylib.h>

namespace perspective {

// Project a single vertex: x is warped toward centre based on y-depth.
// y is unchanged.
inline Vector2 project(float x, float y) {
    constexpr float center = constants::SCREEN_W / 2.0f;
    constexpr float vp_y   = constants::VANISHING_POINT_Y;
    constexpr float range  = static_cast<float>(constants::SCREEN_H) - vp_y;

    float depth = (y - vp_y) / range;
    if (depth < 0.01f) depth = 0.01f;
    if (depth > 1.5f)  depth = 1.5f;

    return { center + (x - center) * depth, y };
}

// Convenience: project just the x component
inline float project_x(float x, float y) {
    return project(x, y).x;
}

// ── Perspective shape drawing (all vertices projected) ──────

// Replaces DrawRectangleRec — draws a trapezoid with 4 projected vertices
void draw_rect(float x, float y, float w, float h, Color c);

// Replaces draw_shape — reads from cached vertex tables, projects per-vertex
void draw_shape(Shape shape, float cx, float cy, float size, Color c);

// Replaces DrawLineEx — both endpoints projected
void draw_line(float x1, float y1, float x2, float y2, float thick, Color c);

// Replaces DrawRing — ring outline with projected vertices from circle table
void draw_ring(float cx, float cy, float inner_r, float outer_r,
               int segments, Color c);

// Replaces DrawRectangleLinesEx — outline trapezoid, 4 projected edges
void draw_rect_lines(float x, float y, float w, float h, float thick, Color c);

// Replaces DrawTriangleLines — outline triangle, 3 projected vertices
void draw_tri_lines(Vector2 v1, Vector2 v2, Vector2 v3, Color c);

} // namespace perspective
```

### New file: `app/perspective.cpp`

Uses `shape_vertices.h` tables — zero trig at draw time.
Full implementation shown in the "Draw-time hot path" section above.

### Constants to add: `app/constants.h`

```cpp
// ── Perspective / Isometric Effect ───────────────
constexpr float PERSPECTIVE_ANGLE_DEG = 17.0f;    // full convergence angle
// VP_Y is derived at compile time from PERSPECTIVE_ANGLE_DEG (~17° → -1060)
// depth(y) = (y - VP_Y) / (SCREEN_H - VP_Y)
// At y=0:    depth ≈ 0.453  (top ~45% scale)
// At y=1280: depth = 1.0    (full scale)
```

---

## Render System Changes

### Pattern: replace ALL world-space draws

Every raylib draw call between `BeginMode2D` / `EndMode2D` is replaced
with its `perspective::` equivalent.  The new functions handle vertex
projection internally.

| Before | After |
|---|---|
| `draw_shape(shape, cx, cy, size, c)` | `perspective::draw_shape(shape, cx, cy, size, c)` |
| `DrawRectangleRec({x, y, w, h}, c)` | `perspective::draw_rect(x, y, w, h, c)` |
| `DrawLineEx({x1,y1}, {x2,y2}, t, c)` | `perspective::draw_line(x1, y1, x2, y2, t, c)` |
| `DrawRing({cx,cy}, r1, r2, ...)` | `perspective::draw_ring(cx, cy, r1, r2, segs, c)` |
| `DrawRectangleLinesEx({x,y,w,h}, t, c)` | `perspective::draw_rect_lines(x, y, w, h, t, c)` |
| `DrawTriangleLines(v1, v3, v2, c)` | `perspective::draw_tri_lines(v1, v3, v2, c)` |

The old `static draw_shape()` function at the top of render_system.cpp is
replaced entirely by `perspective::draw_shape()`.

### NOT transformed (viewport-space, lines 590–618)

- HUD (score, buttons, energy bar, proximity rings)
- Title screen, Level select
- Overlays (game over, song complete, paused)
- Input zones

---

## Input & Collision

**No changes needed.**

- Collision operates in logical space (Position components) — unaffected.
- HUD buttons are in viewport space — unaffected.
- Swipe gestures are direction-based — unaffected.

---

## Edge Cases

1. **Objects above y=0** (SPAWN_Y = -120): depth ≈ 0.27, very compressed.
   They grow as they scroll down — natural "approaching" feel.

2. **Player at y=920**: depth ≈ 0.81.  Player shape is slightly compressed
   at the top edge, wider at the bottom.  Acceptable.  Could clamp to 1.0
   if undesired.

3. **Lane connecting lines**: Each endpoint projected at its own y, so
   lines angle inward toward the vanishing point — this IS the desired
   converging-lane effect.

4. **Circle at top of screen**: Becomes egg-shaped (wider bottom, narrower
   top) — correct perspective for a circle on a receding surface.

5. **DrawRing for lane floor circles**: Must be rebuilt as projected
   triangles (inner/outer radius vertices each projected).  The existing
   `DrawRing` call won't work since it can't per-vertex project.

---

## Testing

- Existing unit tests: unaffected (they test gameplay logic, not rendering)
- Visual verification: set VP_Y to -99999 (≈flat) to revert to current look
- Lane convergence: floor shapes should form three converging columns
- Shape fidelity: squares become trapezoids, triangles taper naturally

---

## File Changes Summary

| File | Change |
|---|---|
| `app/constants.h` | Add `VANISHING_POINT_Y` constant |
| `app/shape_vertices.h` | **New** — `constexpr` unit vertex tables for all shapes (zero runtime trig) |
| `app/perspective.h` | **New** — `project()`, all perspective draw function declarations |
| `app/perspective.cpp` | **New** — per-vertex draw implementations using cached vertex tables |
| `app/systems/render_system.cpp` | Replace all world-space draw calls with `perspective::` calls |
| `CMakeLists.txt` | Add `perspective.cpp` to sources |

No changes to: ECS components, game logic systems, input system, collision,
scoring, HUD, overlays, or menus.
