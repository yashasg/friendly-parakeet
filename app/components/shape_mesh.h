#pragma once
#include "shape_vertices.h"
#include "player.h"
#include <cstdint>

// ── Shape mesh data (pure data, no GPU dependency) ──────────────────────────
// Each game shape is an extruded prism: a ring of N vertices raised to a
// height.  Vertex positions and directional grayscale colors are computed
// once and stored here.  The render system uploads this to the GPU and draws
// with a transform matrix + tint color.

struct Vertex3  { float x, y, z; };
struct VertexColor { uint8_t r, g, b, a; };

constexpr int SHAPE_MAX_RING   = 12;
constexpr int SHAPE_MAX_TRIS   = 4 * SHAPE_MAX_RING;       // top + bottom + 2 per side
constexpr int SHAPE_MAX_VERTS  = SHAPE_MAX_TRIS * 3;       // 3 verts per tri = 144

struct ShapeMeshData {
    Vertex3     positions[SHAPE_MAX_VERTS];
    Vertex3     normals[SHAPE_MAX_VERTS];
    VertexColor colors[SHAPE_MAX_VERTS];
    int         vertex_count = 0;
    int         tri_count    = 0;
};

// Shape descriptor — maps Shape enum to unit ring + proportions.
struct ShapeDesc {
    const shape_verts::V2* ring;
    int    n;               // number of ring vertices
    float  radius_scale;    // multiplied by entity size to get world radius
    float  height_scale;    // multiplied by entity size to get extrusion height
};

// Grayscale shading levels baked into vertex colors.
// Material diffuse tint multiplies these at render time.
constexpr uint8_t SHADE_TOP   = 255;
constexpr uint8_t SHADE_FRONT = 166;
constexpr uint8_t SHADE_SIDE  = 128;
constexpr uint8_t SHADE_BOT   = 90;

// Build an extruded prism mesh from a shape descriptor.
// Pure function — no GPU, no raylib, fully testable.
ShapeMeshData build_prism(const ShapeDesc& desc);

// Build a unit slab (1×1×1 box) with baked directional grayscale.
// At draw time, non-uniform scale encodes width/height/depth.
ShapeMeshData build_unit_slab();

// Build a unit quad (1×1 flat square on XZ plane at y=0).
ShapeMeshData build_unit_quad();

// Compile-time shape table indexed by Shape enum.
extern const ShapeDesc SHAPE_TABLE[4];
