#pragma once

#include "../components/plain_types.h"
#include <raylib.h>

[[nodiscard]] inline Color to_raylib_color(const TintColor& c) noexcept {
    return Color{c.r, c.g, c.b, c.a};
}

[[nodiscard]] inline Color to_raylib_color(const ColorRGBA8& c) noexcept {
    return Color{c.r, c.g, c.b, c.a};
}

[[nodiscard]] inline TintColor to_tint_color(const Color& c) noexcept {
    return TintColor{c.r, c.g, c.b, c.a};
}

[[nodiscard]] inline Mat4f to_mat4f(const Matrix& m) noexcept {
    return Mat4f{{m.m0, m.m4, m.m8, m.m12,
                  m.m1, m.m5, m.m9, m.m13,
                  m.m2, m.m6, m.m10, m.m14,
                  m.m3, m.m7, m.m11, m.m15}};
}

[[nodiscard]] inline Matrix to_raylib_matrix(const Mat4f& m) noexcept {
    const auto& v = m.value;
    return Matrix{
        v[0], v[4], v[8],  v[12],
        v[1], v[5], v[9],  v[13],
        v[2], v[6], v[10], v[14],
        v[3], v[7], v[11], v[15]
    };
}
