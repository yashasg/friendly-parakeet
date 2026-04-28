#pragma once

#include <cstdint>
#include <raylib.h>
#include <entt/entt.hpp>

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

// ── Model-to-world transform ────────────────────────────────────────────────
// Computed by game_camera_system each frame from Position/Size/Shape.
// Consumed by game_render_system for DrawMesh calls.
// MeshType tells the render system which GPU mesh to draw.
enum class MeshType : uint8_t { Shape, Slab, Quad };

struct ModelTransform {
    Matrix   mat;
    Color    tint;
    MeshType mesh_type;
    int      mesh_index;  // index into ShapeMeshes.shapes[] for Shape type
};

// Visual mesh child of a logical entity (e.g., obstacle slabs, ghost shapes).
// Created at spawn time by game object factories. game_camera_system reads
// parent Position + child offsets to compute ModelTransform each frame.
struct MeshChild {
    entt::entity parent;
    float x;             // absolute X position in game coords
    float z_offset;      // offset from parent Position.y (scroll axis)
    float width;         // slab width (game coords)
    float depth;         // slab depth (game coords)
    float height;        // slab height (game coords)
    Color tint;
    MeshType mesh_type;
    int mesh_index;      // index into ShapeMeshes.shapes[] for Shape type
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

// ── GPU model ownership (RAII) ───────────────────────────────────────────────
// Emplaced on LowBar/HighBar entities by build_obstacle_model().
// on_obstacle_model_destroy calls UnloadModel when owned == true.
// Default-constructed with owned=false → safe in headless/test contexts.
struct ObstacleModel {
    Model model{};
    bool  owned = false;

    ObstacleModel() = default;
    ObstacleModel(const ObstacleModel&) = delete;
    ObstacleModel& operator=(const ObstacleModel&) = delete;
    ObstacleModel(ObstacleModel&& o) noexcept : model(o.model), owned(o.owned) {
        o.owned = false;
    }
    ObstacleModel& operator=(ObstacleModel&& o) noexcept {
        if (this != &o) {
            if (owned && model.meshes) UnloadModel(model);
            model   = o.model;
            owned   = o.owned;
            o.owned = false;
        }
        return *this;
    }
};

// Geometry descriptor for a Model-authority obstacle (LowBar, HighBar).
// Populated by build_obstacle_model() at spawn alongside ObstacleModel.
// game_camera_system reads these fields + ObstacleScrollZ.z to recompute
// model.transform each frame without accessing raw mesh vertex data.
// All fields are world-space coordinates; no logic methods.
struct ObstacleParts {
    float cx     = 0.0f;  // slab left-edge X in world coords (0 = left screen edge)
    float cy     = 0.0f;  // reserved Y offset (currently unused; kept for future slabs)
    float cz     = 0.0f;  // local Z origin offset from ObstacleScrollZ.z
    float width  = 0.0f;  // slab width in world coords
    float height = 0.0f;  // slab height in world coords (bar thickness)
    float depth  = 0.0f;  // slab depth in world coords (scroll-axis thickness)
};

// ── Render-pass membership tags ──────────────────────────────────────────────
// Empty tag components that declare which render pass an entity belongs to.
// game_render_system queries each pass view independently.
// One tag per entity.
struct TagWorldPass   {};  // drawn in BeginMode3D (3D world geometry)
struct TagEffectsPass {};  // particles, post-process overlays
struct TagHUDPass     {};  // screen-space UI elements
