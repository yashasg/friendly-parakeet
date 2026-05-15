#pragma once

#include "../components/player.h"

#include <cstdint>

inline constexpr int kShapeCount = 4;

inline constexpr int shape_index(Shape shape) noexcept {
    switch (shape) {
        case Shape::Circle:   return 0;
        case Shape::Square:   return 1;
        case Shape::Triangle: return 2;
        case Shape::Hexagon:  return 3;
    }
    return -1;
}

inline constexpr bool is_valid_shape(Shape shape) noexcept {
    return shape_index(shape) >= 0;
}
