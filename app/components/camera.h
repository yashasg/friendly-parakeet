#pragma once
#include <cstdint>

// Per-frame render parameters computed from SongState beat pulse.
// Passed to floor rendering passes.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};

// Depth-scaled vertical bounds for a rectangle.
struct ScaledRectY {
    float top;
    float bot;
    float d_top;
    float d_bot;
};
