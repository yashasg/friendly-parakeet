#pragma once

// Shared helpers for burnout zone/multiplier math.
// Included by burnout_system.cpp, player_input_system.cpp, and tests.

#include "../components/burnout.h"
#include "../constants.h"

struct BurnoutSample {
    BurnoutZone zone;
    float       meter;
};

// Compute the burnout zone and meter fill [0,1] for a press made at dist
// pixels from the obstacle.  window_scale comes from DifficultyConfig.
inline BurnoutSample compute_burnout_for_distance(float dist, float window_scale) {
    float safe_max   = constants::ZONE_SAFE_MAX   * window_scale;
    float safe_min   = constants::ZONE_SAFE_MIN   * window_scale;
    float risky_min  = constants::ZONE_RISKY_MIN  * window_scale;
    float danger_min = constants::ZONE_DANGER_MIN * window_scale;

    if (dist > safe_max)
        return { BurnoutZone::None,   0.0f };
    if (dist > safe_min)
        return { BurnoutZone::Safe,   (1.0f - (dist - safe_min)   / (safe_max  - safe_min))  * 0.4f };
    if (dist > risky_min)
        return { BurnoutZone::Risky,  0.4f  + (1.0f - (dist - risky_min)  / (safe_min  - risky_min))  * 0.3f };
    if (dist > danger_min)
        return { BurnoutZone::Danger, 0.7f  + (1.0f - (dist - danger_min) / (risky_min - danger_min)) * 0.25f };
    return { BurnoutZone::Dead, 1.0f };
}

// Map a burnout zone to its score multiplier constant.
inline float multiplier_for_zone(BurnoutZone zone) {
    switch (zone) {
        case BurnoutZone::None:   return constants::MULT_SAFE;
        case BurnoutZone::Safe:   return constants::MULT_SAFE;
        case BurnoutZone::Risky:  return constants::MULT_RISKY;
        case BurnoutZone::Danger: return constants::MULT_DANGER;
        case BurnoutZone::Dead:   return constants::MULT_CLUTCH;
    }
    return constants::MULT_SAFE;
}
