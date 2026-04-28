#pragma once

#include <cstdint>
#include <magic_enum/magic_enum.hpp>

#include "player.h"

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

// Bridge-state component for Model-authority obstacles (LowBar, HighBar).
// Holds the scroll-axis Z coordinate in lieu of Position.y.
// Updated each frame by scroll_system; consumed by collision_system,
// cleanup_system, miss_detection_system, and scoring_system.
struct ObstacleScrollZ {
    float z = 0.0f;
};

struct RequiredShape {
    Shape shape = Shape::Circle;
};

struct BlockedLanes {
    uint8_t mask = 0;
};

struct RequiredLane {
    int8_t lane = 0;
};

struct RequiredVAction {
    VMode action = VMode::Jumping;
};
