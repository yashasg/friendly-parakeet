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

// Per-tier popup request (no tier discriminator field — the queue it lives
// in IS the discriminator, per #1202/#1204 per-case-table mechanic).
struct TimedPopupRequest {
    float x      = 0.0f;
    float y      = 0.0f;
    int   points = 0;
};

struct UntimedPopupRequest {
    float x      = 0.0f;
    float y      = 0.0f;
    int   points = 0;
};

// One queue per former TimingTier value (Perfect/Good/Ok/Bad) plus one
// "untimed" queue for hits without a timing grade. popup_feedback_system
// runs one transform per queue — no switch on a discriminator.
struct ScorePopupRequestQueue {
    std::vector<TimedPopupRequest>   perfect;
    std::vector<TimedPopupRequest>   good;
    std::vector<TimedPopupRequest>   ok;
    std::vector<TimedPopupRequest>   bad;
    std::vector<UntimedPopupRequest> untimed;
    uint32_t capacity_exceeded_count = 0;
};
