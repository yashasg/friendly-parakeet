#pragma once
#include "plain_types.h"

// Pre-computed, pixel-space layout data for the Gameplay HUD.
// Populated by gameplay_hud_screen_controller fallback constants when no
// runtime override exists.
struct HudLayout {
    // shape_button_row
    float btn_w                        = 0.0f;
    float btn_h                        = 0.0f;
    float btn_spacing                  = 0.0f;
    float btn_y                        = 0.0f;
    ColorRGBA8 active_bg                    = {};
    ColorRGBA8 inactive_bg                  = {};
    ColorRGBA8 active_border                = {};
    ColorRGBA8 inactive_border              = {};
    ColorRGBA8 active_icon                  = {};
    ColorRGBA8 inactive_icon                = {};
    float approach_ring_max_radius_scale = 0.0f;
    ColorRGBA8 ring_perfect                 = {};
    ColorRGBA8 ring_near                    = {};
    ColorRGBA8 ring_far                     = {};
    // lane_divider (optional)
    bool  has_lane_divider             = false;
    float lane_divider_y               = 0.0f;
    ColorRGBA8 lane_divider_color           = {};
    // Validity gate for optional runtime overrides.
    bool  valid                        = false;
};
