#pragma once

#include <cstdint>

enum class Shape : uint8_t {
    Circle   = 0,
    Square   = 1,
    Triangle = 2
};

struct PlayerTag {};

struct PlayerShape {
    Shape current  = Shape::Circle;
    Shape previous = Shape::Circle;
    float morph_t  = 1.0f;
};

struct Lane {
    int8_t current = 1;
    int8_t target  = -1;
    float  lerp_t  = 1.0f;
};

enum class VMode : uint8_t {
    Grounded = 0,
    Jumping  = 1,
    Sliding  = 2
};

struct VerticalState {
    VMode mode     = VMode::Grounded;
    float timer    = 0.0f;
    float y_offset = 0.0f;
};
