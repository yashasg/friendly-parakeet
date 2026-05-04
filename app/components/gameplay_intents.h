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

    // Legacy compatibility for direct test setup; new gameplay writes events.
    float delta = 0.0f;
    bool  flash = false;
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
