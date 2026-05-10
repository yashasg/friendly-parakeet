#pragma once

#include <array>
#include <cstdint>

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct ColorRGBA8 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// ECS tint/color component for gameplay entities.
struct TintColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

struct Mat4f {
    std::array<float, 16> value{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};
