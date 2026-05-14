#pragma once

#include <cstdint>

#include "player.h"

struct ObstacleTag {};

// Presence means scoring has consumed the obstacle's final hit/miss result.
struct ResolvedObstacleTag {};

enum class ObstacleKind : uint8_t {
    ShapeGate,
    LaneBlock,   // Legacy component fixture only; active beatmaps/factories reject it.
    ComboGate,   // Legacy component fixture only; active beatmaps/factories reject it.
    SplitPath,
    OnsetMarker,
};

constexpr bool obstacle_kind_is_legacy_lane_fixture(ObstacleKind kind) {
    return kind == ObstacleKind::LaneBlock || kind == ObstacleKind::ComboGate;
}

constexpr bool obstacle_kind_is_active_runtime_spawnable(ObstacleKind kind) {
    return kind == ObstacleKind::ShapeGate ||
           kind == ObstacleKind::SplitPath ||
           kind == ObstacleKind::OnsetMarker;
}

constexpr bool obstacle_kind_is_active_blocking_beatmap_kind(ObstacleKind kind) {
    return kind == ObstacleKind::ShapeGate || kind == ObstacleKind::SplitPath;
}

struct Obstacle {
    int16_t      base_points = 200;

    constexpr Obstacle() = default;
    constexpr explicit Obstacle(int16_t points) : base_points(points) {}
};

constexpr ObstacleKind obstacle_kind_from_components(bool has_required_shape,
                                                     bool has_blocked_lanes,
                                                     bool has_required_lane) {
    if (has_required_lane) {
        return ObstacleKind::SplitPath;
    }
    if (has_required_shape && has_blocked_lanes) {
        return ObstacleKind::ComboGate;
    }
    if (has_blocked_lanes) {
        return ObstacleKind::LaneBlock;
    }
    return ObstacleKind::ShapeGate;
}

// Existential tag: presence means the obstacle has been cleared and awaits scoring.
struct ScoredTag {};

// Existential tag: obstacle does not participate in the scoring ladder
// (no score popup, no chain contribution).
struct NonScorableTag {};

// Existential tag: scored obstacle was failed/missed and should not award points.
struct MissTag {};

struct RequiredShape {
    Shape shape = Shape::Circle;
};

struct BlockedLanes {
    uint8_t mask = 0;
};

struct RequiredLane {
    int8_t lane = 0;
};
