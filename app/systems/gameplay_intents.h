#pragma once

#include <cstdint>
#include <vector>

#include "../components/rhythm.h"

// Per-row value for one pending energy delta (issue #1627). Attached to
// an entity carrying `PendingEnergyEffectTag` (+ optional `EnergyFlashTag`)
// by `scoring_system` during the miss/hit passes; drained and destroyed by
// `energy_system` at the start of the same frame. Replaces the former
// `PendingEnergyEffects::events` `std::vector<Event>` array column
// (Fabian Principle 3 — no array columns).
struct EnergyDelta {
    float value = 0.0f;
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
