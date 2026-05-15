#pragma once

#include <cstdint>
#include "rhythm.h"
#include "text.h"

struct ScorePopup {
    int32_t    value           = 0;
    bool       has_timing_tier = false;
    TimingTier timing_tier     = TimingTier::Ok;
    float      remaining       = 0.0f;
    float      max_time        = 0.0f;
};

// Pre-computed popup display data. Static text/color are initialized at spawn;
// ui_render_system lazily caches the text width once the active font is known.
struct PopupDisplay {
    char     text[16] = {};
    FontSize font_size = FontSize::Small;
    uint8_t  r = 255, g = 255, b = 255, a = 255;
    float    text_half_width = 0.0f;
    int      measured_font_base_size = -1;
    unsigned int measured_font_texture_id = 0;
};
