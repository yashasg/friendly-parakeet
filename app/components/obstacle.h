#pragma once

#include <cstdint>

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

struct Obstacle {
    ObstacleKind kind       = ObstacleKind::ShapeGate;
    int16_t      base_points = 200;
};

// Existential tag: presence means the obstacle has been cleared and awaits scoring.
struct ScoredTag {};

// Existential tag: obstacle does not participate in the scoring ladder
// (no score popup, no chain contribution). Emplaced at spawn for passive
// obstacles such as LanePushLeft and LanePushRight.
struct NonScorableTag {};

// Existential tag: obstacle is a bar-type (LowBar or HighBar).
// Emplaced at spawn; consumed by scoring_system to set DeathCause::HitABar.
struct BarObstacleTag {};

// Existential tag: scored obstacle was failed/missed and should not award points.
struct MissTag {};

// Bridge-state component for Model-authority obstacles (LowBar, HighBar).
// Holds the scroll-axis Z coordinate in lieu of Position.y.
// Updated each frame by scroll_system; consumed by collision_system,
// obstacle_despawn_system, miss_detection_system, and scoring_system.
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

// Spawn-time component on LanePush obstacles: direction encoded as ±1.
// -1 = LanePushLeft, +1 = LanePushRight. Emplaced in obstacle_entity.cpp;
// consumed by collision_system to populate PendingLanePush.
struct LanePushDelta {
    int8_t delta = 0;
};
