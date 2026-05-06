#pragma once
// Pre-computed shape vertex lookup tables backed by glm::vec2.
// Used by game_render_system for floor ring geometry.

#include <glm/vec2.hpp>
#include <array>
#include <cmath>

namespace shape_verts {

// Element type alias (used in sizeof checks).
using V2 = glm::vec2;

// Number of segments used to approximate circle outlines.
constexpr int CIRCLE_SEGMENTS = 24;

namespace detail {
inline std::array<V2, CIRCLE_SEGMENTS> make_circle() noexcept {
    std::array<V2, CIRCLE_SEGMENTS> result{};
    for (int i = 0; i < CIRCLE_SEGMENTS; ++i) {
        const float angle =
            6.28318530717958647692f * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
        result[static_cast<std::size_t>(i)] = {std::cos(angle), std::sin(angle)};
    }
    return result;
}
}  // namespace detail

// Pre-computed unit-circle vertices: CIRCLE[i] = { cos(2π*i/CIRCLE_SEGMENTS),
//                                                   sin(2π*i/CIRCLE_SEGMENTS) }
inline const std::array<V2, CIRCLE_SEGMENTS> CIRCLE = detail::make_circle();

}  // namespace shape_verts
