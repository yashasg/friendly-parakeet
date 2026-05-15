#pragma once

#include <cstdint>
#include <entt/entt.hpp>

#include "player.h"
#include "tags/tags.h"

enum class ObstacleKind : uint8_t {
    ShapeGate,
    LaneBlock,   // Legacy component fixture only; active beatmaps/factories reject it.
    ComboGate,   // Legacy component fixture only; active beatmaps/factories reject it.
    SplitPath,
    OnsetMarker,
};

constexpr bool obstacle_kind_is_active_runtime_spawnable(ObstacleKind kind) {
    return kind == ObstacleKind::ShapeGate ||
           kind == ObstacleKind::SplitPath ||
           kind == ObstacleKind::OnsetMarker;
}

constexpr bool obstacle_kind_is_active_blocking_beatmap_kind(ObstacleKind kind) {
    return kind == ObstacleKind::ShapeGate || kind == ObstacleKind::SplitPath;
}

constexpr bool obstacle_kind_is_active_beatmap_spawnable(ObstacleKind kind) {
    return obstacle_kind_is_active_blocking_beatmap_kind(kind) ||
           kind == ObstacleKind::OnsetMarker;
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

struct RequiredShape {
    Shape shape = Shape::Circle;
};

struct ShapeGateLane {
    int8_t lane = 0;
};

// Lane block mask: bit i set ⇒ lane i is blocked (0/1/2 are lane indices).
// Stored as a raw uint8_t component per wrapper-noise removal (issue #1198);
// the entity's ObstacleTag + archetype shape (presence/absence of
// RequiredShape / RequiredLane) carries the semantic role. No other
// uint8_t-typed component is emplaced anywhere in the codebase, so the slot
// is unambiguous.

struct RequiredLane {
    int8_t lane = 0;
};

// Owned mesh children for a logical obstacle entity. destroy_obstacle_with_children
// in app/entities/obstacle_render_entity.cpp cleans up in O(count). Relocated out
// of app/components/rendering.h (issue #1194 SPLIT) — child-ownership is an
// obstacle-archetype concern, not a generic render concept.
struct ObstacleChildren {
    static constexpr int MAX = 8;
    entt::entity children[MAX]{};
    int count = 0;
};
