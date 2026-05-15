#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

// Per-entity GPU mesh-render descriptors. Authoritative producer:
// camera_system (game_camera_system pass). Authoritative consumer:
// game_render_system. Universal across drawable archetypes (player,
// obstacles, popups, particles, mesh children) — not obstacle-specific.
//
// Relocated out of app/components/rendering.h (issue #1194 SPLIT).

enum class MeshType : uint8_t { Shape, Slab, Quad };

struct ModelTransform {
    Matrix   mat = MatrixIdentity();
    Color    tint{255, 255, 255, 255};
    uint8_t  mesh_index = 0;  // index into ShapeMeshes.shapes[] for Shape type
    MeshType mesh_type = MeshType::Shape;
};

// Visual mesh child; game_camera_system resolves parent transform + offsets.
struct MeshChild {
    entt::entity parent = entt::null;
    float x = 0.0f;             // absolute X position in game coords
    float z_offset = 0.0f;      // offset from parent WorldTransform.position.y (scroll axis)
    float width = 0.0f;         // slab width (game coords)
    float depth = 0.0f;         // slab depth (game coords)
    float height = 0.0f;        // slab height (game coords)
    Color tint{255, 255, 255, 255};
    uint8_t mesh_index = 0;  // index into ShapeMeshes.shapes[] for Shape type
    MeshType mesh_type = MeshType::Shape;
};
