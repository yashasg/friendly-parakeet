#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>

// System-private state for popup_display_system. Stored in registry context,
// not emplaced on entities — this is per-system scratch, not component data.
// Relocated out of app/components/system_scratch.h (issue #1196).

struct PopupDisplayScratch {
    std::vector<entt::entity> expired;
    uint32_t capacity_exceeded_count = 0;
};
