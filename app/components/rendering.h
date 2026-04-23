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
// Computed once per frame in main.cpp before input_system runs.
// Keeps raylib.h out of this header; input_system uses it to normalise coords.
struct ScreenTransform {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale    = 1.0f;
};
