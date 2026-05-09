#pragma once

#include <optional>
#include <vector>

#include "rhythm.h"

struct PendingEnergyEffects {
    struct Event {
        float delta = 0.0f;
        bool  flash = false;
    };

    std::vector<Event> events;
};

struct ScorePopupRequest {
    float                    x = 0.0f;
    float                    y = 0.0f;
    int                      points = 0;
    std::optional<TimingTier> timing_tier = std::nullopt;
};

struct ScorePopupRequestQueue {
    std::vector<ScorePopupRequest> requests;
};

