#pragma once

#include "../components/player.h"

#include <cstdint>

enum class KeyboardShapeSlot : uint8_t {
    Left,
    Center,
    Right,
};

inline constexpr Shape shape_for_keyboard_slot(KeyboardShapeSlot slot) noexcept {
    switch (slot) {
        case KeyboardShapeSlot::Left:   return Shape::Triangle;
        case KeyboardShapeSlot::Center: return Shape::Square;
        case KeyboardShapeSlot::Right:  return Shape::Circle;
    }
    return Shape::Square;
}
