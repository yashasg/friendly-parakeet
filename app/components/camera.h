#pragma once
#include <cstdint>

// ── Camera configuration ────────────────────────────────────────────────────
// World coordinates = game-pixel coordinates (1:1). No conversion needed.
// Camera3D near/far planes are sized for pixel-scale distances.

// Per-frame render parameters computed from SongState beat pulse.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};
