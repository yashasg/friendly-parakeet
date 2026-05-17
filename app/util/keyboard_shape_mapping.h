#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "../components/player.h"

enum class KeyboardShapeSlot : uint8_t {
    Left,
    Center,
    Right,
};

inline constexpr Shape shape_for_keyboard_slot(KeyboardShapeSlot slot) noexcept {
    // Fabian Principle 1 / issue #1309: enum-as-lookup-key into a static
    // table. Row order must match the `KeyboardShapeSlot` declaration above;
    // the `static_assert` pins the table size to the trailing enumerator
    // (`Right`).
    constexpr std::array<Shape, 3> kShapeBySlot{{
        /* Left   */ Shape::Circle,
        /* Center */ Shape::Square,
        /* Right  */ Shape::Triangle,
    }};
    static_assert(kShapeBySlot.size() ==
                  static_cast<std::size_t>(KeyboardShapeSlot::Right) + 1,
                  "kShapeBySlot must cover every KeyboardShapeSlot enumerator");

    const auto idx = static_cast<std::size_t>(slot);
    if (idx >= kShapeBySlot.size()) return Shape::Square;
    return kShapeBySlot[idx];
}

inline constexpr Shape kKeyboardShapeLeft = shape_for_keyboard_slot(KeyboardShapeSlot::Left);
inline constexpr Shape kKeyboardShapeCenter = shape_for_keyboard_slot(KeyboardShapeSlot::Center);
inline constexpr Shape kKeyboardShapeRight = shape_for_keyboard_slot(KeyboardShapeSlot::Right);
