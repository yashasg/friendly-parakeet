#pragma once

#include <entt/entt.hpp>
#include "obstacle.h"  // ObstacleTag

// Maintained O(1) counter for live obstacle entities.
// Incremented via on_construct<ObstacleTag> signal.
// Decremented via on_destroy<ObstacleTag>  signal.
// game_state_system reads count == 0 to detect "all obstacles drained".
struct ObstacleCounter {
    int  count = 0;
    bool wired = false;
};

inline void on_obstacle_tag_constructed(entt::registry& reg, entt::entity /*e*/) {
    if (auto* oc = reg.ctx().find<ObstacleCounter>()) {
        ++oc->count;
    }
}

inline void on_obstacle_tag_destroyed(entt::registry& reg, entt::entity /*e*/) {
    if (auto* oc = reg.ctx().find<ObstacleCounter>()) {
        if (oc->count > 0) --oc->count;
    }
}

// Call once per registry lifetime to wire the signals.
// Safe to call multiple times — subsequent calls are no-ops (guarded by wired flag).
inline void wire_obstacle_counter(entt::registry& reg) {
    auto* oc = reg.ctx().find<ObstacleCounter>();
    if (!oc || oc->wired) return;
    reg.on_construct<ObstacleTag>().connect<&on_obstacle_tag_constructed>();
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_tag_destroyed>();
    oc->wired = true;
}
