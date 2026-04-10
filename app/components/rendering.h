#pragma once

#include <cstdint>

struct DrawColor {
    uint8_t r = 255, g = 255, b = 255, a = 255;
};

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
