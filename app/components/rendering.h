#pragma once

#include "tags/tags.h"

// Back-compat shim. The junk-drawer "rendering.h" was split (issue #1194):
//   ScreenTransform                -> components/camera_resources.h
//   MeshType / ModelTransform /
//     MeshChild                    -> components/render_mesh.h
//   ObstacleChildren               -> components/obstacle.h
// Re-included here so existing `#include "components/rendering.h"` sites keep
// compiling; new code should include the canonical header directly.
//
// The wrapper-noise types DrawSize / ScreenPosition remain here pending the
// wrapper deletion sweep (issue #1198). DrawLayer + the Layer enum were
// deleted as dead in the same sweep — render-pass dispatch already uses the
// TagWorldPass / TagEffectsPass / TagHUDPass tags (see app/tags/tags.h), no
// production system ever queried DrawLayer.
#include "camera_resources.h"
#include "obstacle.h"
#include "render_mesh.h"

struct DrawSize {
    float w = 64.0f;
    float h = 64.0f;
};

// Screen-space position computed by ui_camera_system for UI rendering.
struct ScreenPosition {
    float x = 0.0f;
    float y = 0.0f;
};

// Render-pass membership tags (TagWorldPass / TagEffectsPass / TagHUDPass)
// live in app/tags/tags.h; included above for back-compat.
