#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include "plain_types.h"

struct DrawSize {
    float w = 64.0f;
    float h = 64.0f;
};

enum class Layer : uint8_t {
    Background = 0,
    Game       = 1,
    Effects    = 2,
    HUD        = 3
};

struct DrawLayer {
    Layer layer = Layer::Game;
};

// ── Screen Transform (letterbox scale/offset: window → virtual space) ───────
struct ScreenTransform {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale    = 1.0f;
};

[[nodiscard]] inline Vec2f screen_to_virtual(Vec2f screen_pos,
                                             const ScreenTransform& st) noexcept {
    return {
        (screen_pos.x - st.offset_x) / st.scale,
        (screen_pos.y - st.offset_y) / st.scale
    };
}

// ── Model-to-world transform ────────────────────────────────────────────────
// Computed by game_camera_system each frame from WorldTransform/DrawSize/Shape.
// Consumed by game_render_system for DrawMesh calls.
// MeshType tells the render system which GPU mesh to draw.
enum class MeshType : uint8_t { Shape, Slab, Quad };

struct ModelTransform {
    Mat4f mat;
    TintColor tint;
    uint8_t mesh_index = 0;  // index into ShapeMeshes.shapes[] for Shape type
    MeshType mesh_type;
};

// Visual mesh child of a logical entity (e.g., obstacle slabs, ghost shapes).
// Created at spawn time by game object factories. game_camera_system reads
// parent WorldTransform + child offsets to compute ModelTransform each frame.
struct MeshChild {
    entt::entity parent;
    float x;             // absolute X position in game coords
    float z_offset;      // offset from parent WorldTransform.position.y (scroll axis)
    float width;         // slab width (game coords)
    float depth;         // slab depth (game coords)
    float height;        // slab height (game coords)
    TintColor tint;
    uint8_t mesh_index = 0;  // index into ShapeMeshes.shapes[] for Shape type
    MeshType mesh_type;
};

// ── Mesh-child ownership ─────────────────────────────────────────────────────
// Emplaced on logical obstacle entities by spawn_obstacle_meshes.
// on_obstacle_destroy reads this to destroy children in O(count) without
// scanning the entire MeshChild pool.
struct ObstacleChildren {
    static constexpr int MAX = 8;
    entt::entity children[MAX];
    int count = 0;
};

// ── Screen-space position ───────────────────────────────────────────────────
// Computed by camera_system via GetWorldToScreenEx projection.
// Used by UI render pass to draw popups at the correct screen location.
struct ScreenPosition {
    float x = 0.0f;
    float y = 0.0f;
};


// ── Render-pass membership tags ──────────────────────────────────────────────
// Empty tag components that declare which render pass an entity belongs to.
// game_render_system queries each pass view independently.
// One tag per entity.
struct TagWorldPass   {};  // drawn in BeginMode3D (3D world geometry)
struct TagEffectsPass {};  // particles, post-process overlays
struct TagHUDPass     {};  // screen-space UI elements
