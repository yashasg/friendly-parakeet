#pragma once

#include "../components/obstacle.h"
#include "../components/player.h"
#include "../components/rhythm.h"
#include <magic_enum/magic_enum.hpp>

inline const char* ToString(Shape shape) noexcept {
    const auto name = magic_enum::enum_name(shape);
    return name.empty() ? "???" : name.data();
}

inline const char* ToString(ObstacleKind kind) noexcept {
    const auto name = magic_enum::enum_name(kind);
    return name.empty() ? "???" : name.data();
}

inline const char* ToString(TimingTier tier) noexcept {
    const auto name = magic_enum::enum_name(tier);
    return name.empty() ? "???" : name.data();
}
