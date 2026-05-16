#pragma once

#include <cstdint>
#include <entt/entt.hpp>

#include "player.h"
#include "tags/tags.h"

struct Obstacle {
    int16_t      base_points = 200;

    constexpr Obstacle() = default;
    constexpr explicit Obstacle(int16_t points) : base_points(points) {}
};

// `RequiredShape` (formerly a single-column `{ Shape shape; }` row) was
// eradicated in issue #1202/#1204; the obstacle's required shape lives as
// one of `RequiredShapeCircleTag` / `RequiredShapeSquareTag` /
// `RequiredShapeTriangleTag` / `RequiredShapeHexagonTag` on the obstacle
// entity. Use `set_required_shape_tag()` from `app/util/shape_tag.h` to
// emplace it, and `current_required_shape()` / `has_required_shape_tag()`
// to read it back.

struct ShapeGateLane {
    int8_t lane = 0;
};

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
