#pragma once

#include <cstdint>
#include "../components/player.h"

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

inline constexpr Shape kKeyboardShapeLeft = shape_for_keyboard_slot(KeyboardShapeSlot::Left);
inline constexpr Shape kKeyboardShapeCenter = shape_for_keyboard_slot(KeyboardShapeSlot::Center);
inline constexpr Shape kKeyboardShapeRight = shape_for_keyboard_slot(KeyboardShapeSlot::Right);
