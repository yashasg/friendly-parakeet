#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>

// System-private state for obstacle_despawn_system. Stored in registry context,
// not emplaced on entities — this is per-system scratch, not component data.
// Relocated out of app/components/system_scratch.h (issue #1196).

struct ObstacleDespawnScratch {
    std::vector<entt::entity> to_destroy;
    uint32_t capacity_exceeded_count = 0;
};
