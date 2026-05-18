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
// the per-entity text-width cache lives in a sibling `PopupTextMeasured` row
// table whose presence + key-match expresses cache validity (issue #1549,
// Fabian Principle 3 — `.squad/decisions.md` § 9).
struct PopupDisplay {
    char     text[16] = {};
    FontSize font_size = FontSize::Small;
    uint8_t  r = 255, g = 255, b = 255, a = 255;
};

// Per-popup-entity text-measurement cache (row table). Emplaced/refreshed by
// `ui_render_system` on the first frame the entity is rendered (and again
// whenever the active font's identity changes, e.g. atlas reload). Membership
// + key-match IS the precondition "`half_width` is valid for `(font_base_size,
// font_texture_id)`"; absence forces a re-measurement.
//
// Replaces the prior `PopupDisplay { … float text_half_width; int
// measured_font_base_size = -1; unsigned int measured_font_texture_id = 0; }`
// shape (issue #1549) — the `-1` / `0` sentinels were NULL columns in disguise
// per Fabian Principle 3.
struct PopupTextMeasured {
    int          font_base_size;
    unsigned int font_texture_id;
    float        half_width;
};
