#pragma once

#include <cstdint>

#include "tags/tags.h"

// Back-compat shim. The junk-drawer "rendering.h" was split (issue #1194):
//   ScreenTransform                -> components/camera_resources.h
//   MeshType / ModelTransform /
//     MeshChild                    -> components/render_mesh.h
//   ObstacleChildren               -> components/obstacle.h
// Re-included here so existing `#include "components/rendering.h"` sites keep
// compiling; new code should include the canonical header directly.
//
// The wrapper-noise types DrawSize / DrawLayer / Layer / ScreenPosition remain
// here pending the wrapper deletion sweep (issue #1198).
#include "camera_resources.h"
#include "obstacle.h"
#include "render_mesh.h"

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

// Screen-space position computed by ui_camera_system for UI rendering.
struct ScreenPosition {
    float x = 0.0f;
    float y = 0.0f;
};

// Render-pass membership tags (TagWorldPass / TagEffectsPass / TagHUDPass)
// live in app/tags/tags.h; included above for back-compat.
