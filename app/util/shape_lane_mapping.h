#pragma once

#include "../components/player.h"

#include <cstdint>

// Canonical shape→lane pairing authored by shipped beatmaps and HUD buttons.
// Keep runtime input auto-targeting aligned with this helper.
inline constexpr int8_t lane_for_shape(Shape shape) noexcept {
    switch (shape) {
        case Shape::Circle:   return 0;
        case Shape::Square:   return 1;
        case Shape::Triangle: return 2;
        case Shape::Hexagon:  return -1;
    }
    return -1;
}
