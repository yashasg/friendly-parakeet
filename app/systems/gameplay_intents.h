#pragma once

#include <cstdint>
#include <vector>

#include "../components/rhythm.h"

struct PendingEnergyEffects {
    struct Event {
        float delta = 0.0f;
        bool  flash = false;
    };

    std::vector<Event> events;
    uint32_t capacity_exceeded_count = 0;
};

struct ScorePopupRequest {
    float      x = 0.0f;
    float      y = 0.0f;
    int        points = 0;
    bool       has_timing_tier = false;
    TimingTier timing_tier = TimingTier::Ok;
};

struct ScorePopupRequestQueue {
    std::vector<ScorePopupRequest> requests;
    uint32_t capacity_exceeded_count = 0;
};
