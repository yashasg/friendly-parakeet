#pragma once

#include <cstdint>
#include <optional>
#include "../components/text.h"
#include "../components/rhythm.h"

struct ScoreState {
    int32_t score             = 0;
    int32_t displayed_score   = 0;
    int32_t high_score        = 0;
    int32_t chain_count       = 0;
    float   chain_timer       = 0.0f;
    float   distance_traveled = 0.0f;
};

struct ScorePopup {
    int32_t                    value       = 0;
    std::optional<TimingTier>  timing_tier = std::nullopt;
    float                      remaining   = 0.0f;
    float                      max_time    = 0.0f;
};

// Pre-computed popup display data. Initialized once at popup spawn,
// then popup_display_system only updates alpha over time.
struct PopupDisplay {
    char     text[16] = {};
    FontSize font_size = FontSize::Small;
    uint8_t  r = 255, g = 255, b = 255, a = 255;
    uint8_t  base_a = 255;
};
