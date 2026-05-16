#pragma once

#include <cstdint>
#include "rhythm.h"
#include "text.h"

// ScorePopup carries only the value + lifetime. The tier discriminator that
// used to live here has been migrated (issue #1202/#1204) to a per-tier tag
// (TimingPerfectTag/TimingGoodTag/TimingOkTag/TimingBadTag) emplaced on the
// popup entity itself. The static text + base color are baked into
// PopupDisplay at spawn-time; no system re-reads tier from ScorePopup.
struct ScorePopup {
    int32_t value     = 0;
    float   remaining = 0.0f;
    float   max_time  = 0.0f;
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
