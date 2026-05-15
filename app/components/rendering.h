#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <raylib.h>
#include <raymath.h>

#include "tags/tags.h"

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

// Letterbox scale/offset: window to virtual space.
// Coordinate-conversion helper lives in app/systems/camera_system.h
// (screen_to_virtual) — components headers stay data-only.
struct ScreenTransform {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale    = 1.0f;
};

// Computed by game_camera_system from WorldTransform/DrawSize/Shape.
// MeshType tells the render system which GPU mesh to draw.
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

// Owned mesh children; destroy_obstacle_with_children cleans up in O(count).
struct ObstacleChildren {
    static constexpr int MAX = 8;
    entt::entity children[MAX]{};
    int count = 0;
};

// Screen-space position computed by ui_camera_system for UI rendering.
struct ScreenPosition {
    float x = 0.0f;
    float y = 0.0f;
};


// Render-pass membership tags (TagWorldPass / TagEffectsPass / TagHUDPass)
// live in app/tags/tags.h; included above for back-compat.
