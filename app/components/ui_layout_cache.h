#pragma once
#include <raylib.h>

// Pre-computed, pixel-space layout data for the Gameplay HUD.
// Built once at "gameplay" screen load by build_hud_layout() in ui_loader.cpp.
// Stored in reg.ctx() by ui_navigation_system; consumed by ui_render_system.
// All coordinate fields are already multiplied by SCREEN_W / SCREEN_H.
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
    // Set false when required elements are missing from the screen JSON.
    bool  valid                        = false;
};

// Pre-computed, pixel-space layout data for the Level Select screen.
// Built once at "level_select" screen load by build_level_select_layout() in ui_loader.cpp.
// Stored in reg.ctx(); consumed by ui_render_system.
struct LevelSelectLayout {
    // song_cards (card_list)
    float card_x              = 0.0f;
    float start_y             = 0.0f;
    float card_w              = 0.0f;
    float card_h              = 0.0f;
    float card_gap            = 0.0f;
    float card_corner_radius  = 0.0f;
    Color selected_bg         = {};
    Color unselected_bg       = {};
    Color selected_border     = {};
    Color unselected_border   = {};
    float title_offset_x      = 0.0f;
    float title_offset_y      = 0.0f;
    // difficulty_buttons (button_row)
    float diff_y_offset       = 0.0f;
    float dx_start            = 0.0f;
    float diff_btn_w          = 0.0f;
    float diff_btn_h          = 0.0f;
    float diff_btn_gap        = 0.0f;
    float diff_corner_radius  = 0.0f;
    Color diff_active_bg      = {};
    Color diff_active_border  = {};
    Color diff_active_text    = {};
    Color diff_inactive_bg    = {};
    Color diff_inactive_border = {};
    Color diff_inactive_text  = {};
    // Set false when required elements are missing from the screen JSON.
    bool  valid               = false;
};
