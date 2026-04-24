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
    int32_t value       = 0;
    uint8_t tier        = 0;     // burnout tier (legacy)
    uint8_t timing_tier = 255;   // TimingTier value, 255 = no timing (non-shape obstacle)
};

// Pre-computed popup display data. Computed by popup_display_system,
// consumed by render_ui_system (just draws text at position with color).
// Pre-computed popup display data. Computed by popup_display_system,
// consumed by render_ui_system (just draws text at position with color).
struct PopupDisplay {
    char     text[16] = {};
    FontSize font_size = FontSize::Small;
    uint8_t  r = 255, g = 255, b = 255, a = 255;
};
