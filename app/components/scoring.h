#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>
#include "../components/text.h"

struct ScoreState {
    int32_t score             = 0;
    int32_t displayed_score   = 0;
    int32_t high_score        = 0;
    int32_t chain_count       = 0;
    float   chain_timer       = 0.0f;
    float   distance_traveled = 0.0f;
};

struct ScorePopup {
    // Sentinel for timing_tier meaning "no timing grade" (non-shape obstacle
    // or freeplay mode).  Defined as a named constant so readers don't need
    // to interpret the raw 255 at every use site.
    static constexpr uint8_t TIMING_TIER_NONE = 255;

    int32_t value       = 0;
    uint8_t tier        = 0;              // burnout tier (legacy)
    uint8_t timing_tier = TIMING_TIER_NONE;
};

// Pre-computed popup display data. Computed by popup_display_system,
// consumed by ui_render_system (just draws text at position with color).
struct PopupDisplay {
    char     text[16] = {};
    FontSize font_size = FontSize::Small;
    uint8_t  r = 255, g = 255, b = 255, a = 255;
};
