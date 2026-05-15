#pragma once

#include <entt/entt.hpp>
#include <raylib.h>
#include <cstdint>
#include <vector>

#include "../components/obstacle.h"
#include "../components/rhythm.h"

// System-private state for scoring_system. Stored in registry context, not
// emplaced on entities — this is per-system scratch, not component data.
// Relocated out of app/components/system_scratch.h (issue #1196).

struct MissRecord {
    entt::entity e = entt::null;
    bool has_timing = false;
};

struct HitRecord {
    entt::entity e = entt::null;
    Vector2 popup_xy = {0.0f, 0.0f};
    Obstacle obs;
    bool has_timing = false;
    TimingGrade timing{};
};

struct ScoringSystemScratch {
    std::vector<MissRecord> miss_buf;
    std::vector<HitRecord> hit_buf;
    uint32_t miss_capacity_exceeded_count = 0;
    uint32_t hit_capacity_exceeded_count = 0;
};
