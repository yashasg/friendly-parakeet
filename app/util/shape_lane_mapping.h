#pragma once

#include "../components/player.h"

#include <cstdint>

inline constexpr int kShapeCount = 4;

// Shape is a 4-value ordinal label: the enumerator IS the index. Per Fabian's
// existential processing (issue #1202/#1204), the former 4-case discriminator
// switch was a hidden lookup table; we replace it with the direct ordinal
// dispatch the switch was hand-rolling. The bounds check rejects ill-formed
// casts (tests construct `static_cast<Shape>(255)` to probe error paths) so
// the contract is identical to the pre-migration switch.
inline constexpr int shape_index(Shape shape) noexcept {
    const auto raw = static_cast<uint8_t>(shape);
    return (raw < static_cast<uint8_t>(kShapeCount)) ? static_cast<int>(raw) : -1;
}

inline constexpr bool is_valid_shape(Shape shape) noexcept {
    return shape_index(shape) >= 0;
}
