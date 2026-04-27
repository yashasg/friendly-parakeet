#pragma once

#include <cstdint>
#include <magic_enum/magic_enum.hpp>

struct ObstacleTag {};

enum class ObstacleKind : uint8_t {
    ShapeGate,
    LaneBlock,
    LowBar,
    HighBar,
    ComboGate,
    SplitPath,
    LanePushLeft,
    LanePushRight,
};

// magic_enum::enum_name_v is a static_str with a null-terminated char array,
// so .data() is safe for printf-style %s formatting.
inline const char* ToString(ObstacleKind k) noexcept {
    const auto name = magic_enum::enum_name(k);
    return name.empty() ? "???" : name.data();
}

struct Obstacle {
    ObstacleKind kind       = ObstacleKind::ShapeGate;
    int16_t      base_points = 200;
};

// Existential tag: presence means the obstacle has been cleared and awaits scoring.
struct ScoredTag {};

// Existential tag: scored obstacle was failed/missed and should not award points.
struct MissTag {};
