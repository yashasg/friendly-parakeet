#pragma once
#include "shape_vertices.h"
#include "player.h"
#include <raylib.h>

// ── Shape mesh data ─────────────────────────────────────────────────────────
// Each game shape is an extruded prism built directly into a raylib Mesh.
// Vertex positions and directional grayscale colors are baked once at
// startup.  The render system draws with a transform matrix + tint color.

// Shape descriptor — maps Shape enum to unit ring + proportions.
struct ShapeDesc {
    const shape_verts::V2* ring;
    int    n;
    float  radius_scale;
    float  height_scale;
};

// Grayscale shading levels baked into vertex colors.
constexpr uint8_t SHADE_TOP   = 255;
constexpr uint8_t SHADE_FRONT = 166;
constexpr uint8_t SHADE_SIDE  = 128;
constexpr uint8_t SHADE_BOT   = 90;

// Build meshes directly into raylib Mesh structs (ready for UploadMesh).
Mesh build_prism(const ShapeDesc& desc);
Mesh build_unit_slab();
Mesh build_unit_quad();

// Compile-time shape table indexed by Shape enum.
extern const ShapeDesc SHAPE_TABLE[4];
