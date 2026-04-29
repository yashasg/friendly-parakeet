#pragma once
#include <raylib.h>

// Pre-computed, pixel-space layout data for the Gameplay HUD.
// Populated by gameplay_hud_screen_controller fallback constants when no
// runtime override exists.
struct HudLayout {
    // shape_button_row
    float btn_w                        = 0.0f;
    float btn_h                        = 0.0f;
    float btn_spacing                  = 0.0f;
    float btn_y                        = 0.0f;
    Color active_bg                    = {};
    Color inactive_bg                  = {};
    Color active_border                = {};
    Color inactive_border              = {};
    Color active_icon                  = {};
    Color inactive_icon                = {};
    float approach_ring_max_radius_scale = 0.0f;
    Color ring_perfect                 = {};
    Color ring_near                    = {};
    Color ring_far                     = {};
    // lane_divider (optional)
    bool  has_lane_divider             = false;
    float lane_divider_y               = 0.0f;
    Color lane_divider_color           = {};
    // Validity gate for optional runtime overrides.
    bool  valid                        = false;
};
