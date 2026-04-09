#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>

struct ScoreState {
    int32_t score             = 0;
    int32_t displayed_score   = 0;
    int32_t high_score        = 0;
    int32_t chain_count       = 0;
    float   chain_timer       = 0.0f;
    float   distance_traveled = 0.0f;
};

struct ScorePopup {
    int32_t value = 0;
    uint8_t tier  = 0;
};
