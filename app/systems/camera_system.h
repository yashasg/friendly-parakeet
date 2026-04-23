#pragma once
#include "../components/camera.h"
#include "../components/player.h"
#include "../components/shape_mesh.h"
#include <entt/entt.hpp>
#include <raylib.h>

namespace camera {

constexpr float WORLD_SCALE = 10.0f;

// ── GPU shape meshes (built once, drawn many) ────────────────────────────────
struct ShapeMeshes {
    Mesh     meshes[4];    // indexed by Shape enum
    Material material;     // shared material — diffuse color set per draw
};

ShapeMeshes build_shape_meshes();
void        unload_shape_meshes(ShapeMeshes& sm);

// ── GPU-batched render passes ────────────────────────────────────────────────
void flush_floor_lines(entt::registry& reg, const FloorParams& fp);
void flush_floor_rings(const FloorParams& fp);
void flush_world_rects(entt::registry& reg);
void flush_gameplay_tris(entt::registry& reg);

} // namespace camera
