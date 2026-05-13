#pragma once

#include "../components/player.h"

#include <cstdint>

// Historical content motif used by shipped beatmap audits. Runtime input must
// not use this helper to move lanes; lane changes come from explicit movement.
inline constexpr int8_t lane_for_shape(Shape shape) noexcept {
    switch (shape) {
        case Shape::Circle:   return 0;
        case Shape::Square:   return 1;
        case Shape::Triangle: return 2;
        case Shape::Hexagon:  return -1;
    }
    return -1;
}
