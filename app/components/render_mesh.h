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
//
// Per-mesh dispatch lives on its own per-kind table (existential processing,
// issue #1202/#1204). A drawable entity carries `ModelTransform` plus one of:
//   - `MeshKindShape{mesh_index}` (defined here — carries the shape lookup index)
//   - `MeshKindSlab` (empty tag in app/tags/tags.h)
//   - `MeshKindQuad` (empty tag in app/tags/tags.h)
// The renderer iterates one view per kind tag; no `switch` on a discriminator.

struct ModelTransform {
    Matrix mat = MatrixIdentity();
    Color  tint{255, 255, 255, 255};
};

// Shape-kind row: column is the index into ShapeMeshes.shapes[]. Non-empty
// (1NF) so it lives here, not in app/tags/tags.h (which is empty tags only).
struct MeshKindShape {
    uint8_t mesh_index = 0;
};

// Visual mesh child; game_camera_system resolves parent transform + offsets.
// Pair with one of MeshKindShape / MeshKindSlab on the same entity to pick
// the matrix builder and renderer.
struct MeshChild {
    entt::entity parent = entt::null;
    float x = 0.0f;             // absolute X position in game coords
    float z_offset = 0.0f;      // offset from parent WorldPosition.position.y (scroll axis)
    float width = 0.0f;         // slab width (game coords)
    float depth = 0.0f;         // slab depth (game coords)
    float height = 0.0f;        // slab height (game coords)
    Color tint{255, 255, 255, 255};
};
