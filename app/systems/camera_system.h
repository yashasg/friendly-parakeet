#pragma once
#include "../components/camera.h"
#include "../components/player.h"
#include "../components/shape_vertices.h"
#include <entt/entt.hpp>
#include <raylib.h>

namespace camera {

constexpr float WORLD_SCALE = 10.0f;

// ── Shape geometry descriptors (compile-time) ────────────────────────────────
struct ShapeDesc {
    const shape_verts::V2* ring;
    int    n;
    float  radius_scale;
    float  height_scale;
};

// ── GPU shape meshes (built once, drawn many) ────────────────────────────────
struct ShapeMeshes {
    Mesh     meshes[4];    // indexed by Shape enum
    Material material;     // shared material — diffuse color set per draw
};

ShapeMeshes build_shape_meshes();
void        unload_shape_meshes(ShapeMeshes& sm);

// ── Standalone 3D shape draw (immediate mode fallback) ───────────────────────
void draw_shape(Shape shape, float cx, float y_3d, float cz, float size, Color c);

// ── GPU-batched render passes ────────────────────────────────────────────────
void flush_floor_lines(entt::registry& reg, const FloorParams& fp);
void flush_floor_rings(const FloorParams& fp);
void flush_world_rects(entt::registry& reg);
void flush_gameplay_tris(entt::registry& reg);

} // namespace camera
