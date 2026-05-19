#pragma once

#include "../components/player.h"
#include "../components/transform.h"
#include "../constants.h"

#include <entt/entt.hpp>

#include <cmath>
#include <cstdint>

namespace lane_utils {

inline constexpr int8_t kDefaultLane = static_cast<int8_t>(constants::DEFAULT_LANE);

inline bool is_valid(int lane) {
    return constants::is_valid_lane(lane);
}

inline int8_t valid_or_default(int8_t lane) {
    return is_valid(lane) ? lane : kDefaultLane;
}

inline void snap_to_current_lane(const Lane& lane, WorldPosition& transform) {
    transform.position.x = constants::LANE_X[lane.current];
}

inline int8_t nearest_lane_for_x(float x) {
    int8_t nearest_lane = 0;
    float nearest_distance = std::abs(x - constants::LANE_X[0]);

    for (int lane = 1; lane < constants::LANE_COUNT; ++lane) {
        const float distance = std::abs(x - constants::LANE_X[lane]);
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_lane = static_cast<int8_t>(lane);
        }
    }

    return nearest_lane;
}

// Validates the player's lane row (issue #1533): an invalid `current` snaps
// back to the default lane and cancels any in-flight transition. Transition
// targets are guaranteed by `LaneTransition` construction.
inline bool normalize(entt::registry& reg,
                      entt::entity entity,
                      Lane& lane,
                      WorldPosition* transform = nullptr) {
    if (!is_valid(lane.current)) {
        lane.current = kDefaultLane;
        if (reg.all_of<LaneTransition>(entity)) {
            reg.remove<LaneTransition>(entity);
        }
        if (transform) {
            snap_to_current_lane(lane, *transform);
        }
        return true;
    }

    return false;
}

}  // namespace lane_utils
