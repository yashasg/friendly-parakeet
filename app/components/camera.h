#pragma once
#include <cstdint>
#include <raylib.h>

// ── Camera configuration ────────────────────────────────────────────────────
// Game logic and 3D world share the same coordinate space (game-pixel units).
// No scaling is applied; the model-to-world transform is scale + translate
// in game coordinates directly.

// Per-frame render parameters computed from SongState beat pulse.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};

// Render targets for world (3D) and UI (2D) layers.
struct RenderTargets {
    RenderTexture2D world;
    RenderTexture2D ui;
};
