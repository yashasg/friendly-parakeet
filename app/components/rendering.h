#pragma once

#include <cstdint>
#include <raylib.h>

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
// Computed by camera_system each frame from Position/Size/Shape.
// Consumed by render_system for DrawMesh calls.
// MeshType tells the render system which GPU mesh to draw.
enum class MeshType : uint8_t { Shape, Slab, Quad };

struct ModelTransform {
    Matrix   mat;
    Color    tint;
    MeshType mesh_type;
    int      mesh_index;  // index into ShapeMeshes.shapes[] for Shape type
};
