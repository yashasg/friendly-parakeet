#pragma once

#include <cstdint>
#include <raylib.h>

namespace constants {

// ── Logical Resolution ────────────────────────────
constexpr int   SCREEN_W          = 720;
constexpr int   SCREEN_H          = 1280;
// Float companions used by obstacle drawing (avoids C-style functional casts)
constexpr float SCREEN_W_F        = static_cast<float>(SCREEN_W);
constexpr float SCREEN_H_F        = static_cast<float>(SCREEN_H);

// ── Lanes ─────────────────────────────────────────
constexpr int   LANE_COUNT        = 3;
constexpr float LANE_X[3]         = { 60.0f, 360.0f, 660.0f };
constexpr float LANE_SWITCH_SPEED = 12.0f;

// ── Player ────────────────────────────────────────
constexpr float PLAYER_Y          = 920.0f;
constexpr float PLAYER_SIZE       = 64.0f;
constexpr float JUMP_HEIGHT       = 160.0f;
constexpr float JUMP_DURATION     = 0.45f;
constexpr float SLIDE_DURATION    = 0.50f;
constexpr float MORPH_DURATION    = 0.12f;

// ── Obstacles ─────────────────────────────────────
constexpr float SPAWN_Y           = -120.0f;
constexpr float DESTROY_Y         = 1400.0f;
constexpr float BASE_SCROLL_SPEED = 400.0f;

// 3D slab heights for obstacle rendering (model-to-world Y scale)
constexpr float OBSTACLE_3D_HEIGHT = 20.0f;
constexpr float LOWBAR_3D_HEIGHT   = 30.0f;
constexpr float HIGHBAR_3D_HEIGHT  = 10.0f;

// ── Burnout Zones (distance from player, in px) ──
constexpr float ZONE_SAFE_MAX     = 700.0f;
constexpr float ZONE_SAFE_MIN     = 500.0f;
constexpr float ZONE_RISKY_MIN    = 300.0f;
constexpr float ZONE_DANGER_MIN   = 140.0f;

// ── Burnout Multipliers ───────────────────────────
constexpr float MULT_SAFE         = 1.0f;
constexpr float MULT_RISKY        = 1.5f;
constexpr float MULT_DANGER       = 3.0f;
constexpr float MULT_CLUTCH       = 5.0f;

// ── Scoring ───────────────────────────────────────
constexpr int   PTS_SHAPE_GATE    = 200;
constexpr int   PTS_LANE_BLOCK    = 100;
constexpr int   PTS_LOW_BAR       = 100;
constexpr int   PTS_HIGH_BAR      = 100;
constexpr int   PTS_COMBO_GATE    = 200;
constexpr int   PTS_SPLIT_PATH    = 300;
constexpr int   PTS_LANE_PUSH     = 0;  // passive — no score
constexpr int   PTS_PER_SECOND    = 10;
constexpr int   CHAIN_BONUS[5]    = { 0, 0, 50, 100, 200 };

// ── Difficulty Timeline ───────────────────────────
// Retained for legacy random-spawn mode
constexpr float SPEED_RAMP_RATE   = 0.011f;
constexpr float SPAWN_RAMP_RATE   = 0.003f;
constexpr float BURNOUT_SHRINK    = 0.002f;
constexpr float INITIAL_SPAWN_INT = 1.8f;
constexpr float MIN_SPAWN_INT     = 0.5f;

// ── Rhythm Constants ──────────────────────────────
constexpr float APPROACH_DIST      = 1040.0f;  // |PLAYER_Y - SPAWN_Y|
constexpr float COLLISION_MARGIN   = 40.0f;    // half-height of timing window (px)

// ── Energy Bar ────────────────────────────────────
constexpr float ENERGY_MAX              = 1.0f;
constexpr float ENERGY_DRAIN_MISS       = 0.20f;
constexpr float ENERGY_DRAIN_BAD        = 0.10f;
constexpr float ENERGY_RECOVER_OK       = 0.02f;
constexpr float ENERGY_RECOVER_GOOD     = 0.05f;
constexpr float ENERGY_RECOVER_PERFECT  = 0.10f;
constexpr float ENERGY_DISPLAY_SPEED    = 3.0f;
constexpr float ENERGY_FLASH_DURATION   = 0.3f;
constexpr float ENERGY_CRITICAL_THRESH  = 0.25f;

// ── Rendering ─────────────────────────────────────
constexpr float POPUP_DURATION    = 1.2f;

// ── Particles ─────────────────────────────────────
constexpr float PARTICLE_GRAVITY  = 600.0f;

// ── Input ─────────────────────────────────────────
constexpr float SWIPE_ZONE_SPLIT  = 0.80f;
constexpr float MIN_SWIPE_DIST    = 50.0f;
constexpr float MAX_SWIPE_TIME    = 0.3f;

// ── UI Layout (pixel-space; retained to derive the NDC constants below and for tests) ────
constexpr float BURNOUT_BAR_Y     = 1020.0f;
constexpr float BURNOUT_BAR_H     = 20.0f;
constexpr float BUTTON_Y          = 1140.0f;
constexpr float BUTTON_W          = 140.0f;
constexpr float BUTTON_H          = 100.0f;
constexpr float BUTTON_SPACING    = 60.0f;

// ── Normalised Viewport Layout (0.0–1.0 × SCREEN_W or SCREEN_H) ──────────
// HUD, scenes, and overlays are positioned in this space.  At draw time
// multiply by SCREEN_W (x) or SCREEN_H (y) to obtain virtual pixel coords.
// Using NDC here means the layout is readable and resolution-independent.

// Viewport horizontal centre (shared by most overlay text)
constexpr float VIEWPORT_CX_N         = 0.5f;

// HUD – score / status display
constexpr float HUD_SCORE_X_N         =  120.0f / SCREEN_W;  // ≈ 0.167
constexpr float HUD_SCORE_Y_N         =   20.0f / SCREEN_H;  // ≈ 0.016
constexpr float HUD_HISCORE_Y_N       =   50.0f / SCREEN_H;  // ≈ 0.039

// HUD – shape button row (NDC companions to the pixel constants above)
constexpr float BUTTON_Y_N            = BUTTON_Y       / SCREEN_H;  // ≈ 0.891
constexpr float BUTTON_W_N            = BUTTON_W       / SCREEN_W;  // ≈ 0.194
constexpr float BUTTON_H_N            = BUTTON_H       / SCREEN_H;  // ≈ 0.078
constexpr float BUTTON_SPACING_N      = BUTTON_SPACING / SCREEN_W;  // ≈ 0.083

// HUD – burnout bar (NDC companions to the pixel constants above)
constexpr float BURNOUT_BAR_Y_N       = BURNOUT_BAR_Y / SCREEN_H;   // ≈ 0.797
constexpr float BURNOUT_BAR_H_N       = BURNOUT_BAR_H / SCREEN_H;   // ≈ 0.016

// Scene – Title (shapes + text)
constexpr float SCENE_TITLE_SHAPES_Y_N       =  400.0f / SCREEN_H;  // ≈ 0.313
constexpr float SCENE_TITLE_SHAPES_SIZE_N    =   80.0f / SCREEN_W;  // ≈ 0.111
constexpr float SCENE_TITLE_SHAPES_OFFSET_N  =   80.0f / SCREEN_W;  // ≈ 0.111 (centre-to-centre)
constexpr float SCENE_TITLE_TEXT_Y_N         =  500.0f / SCREEN_H;  // ≈ 0.391
constexpr float SCENE_TITLE_PROMPT_Y_N       =  600.0f / SCREEN_H;  // ≈ 0.469

// Scene – Game Over overlay
constexpr float SCENE_GO_TITLE_Y_N    =  440.0f / SCREEN_H;  // ≈ 0.344
constexpr float SCENE_GO_SCORE_Y_N    =  510.0f / SCREEN_H;  // ≈ 0.398
constexpr float SCENE_GO_HISCORE_Y_N  =  560.0f / SCREEN_H;  // ≈ 0.438
constexpr float SCENE_GO_PROMPT_Y_N   =  650.0f / SCREEN_H;  // ≈ 0.508

// Scene – Song Complete overlay
constexpr float SCENE_SC_TITLE_Y_N    =  340.0f / SCREEN_H;  // ≈ 0.266
constexpr float SCENE_SC_SLABEL_Y_N   =  420.0f / SCREEN_H;  // ≈ 0.328
constexpr float SCENE_SC_SCORE_Y_N    =  455.0f / SCREEN_H;  // ≈ 0.355
constexpr float SCENE_SC_HSLABEL_Y_N  =  510.0f / SCREEN_H;  // ≈ 0.398
constexpr float SCENE_SC_HISCORE_Y_N  =  545.0f / SCREEN_H;  // ≈ 0.426
constexpr float SCENE_SC_TIMING_Y_N   =  600.0f / SCREEN_H;  // ≈ 0.469
constexpr float SCENE_SC_TIMING_DY_N  =   30.0f / SCREEN_H;  // ≈ 0.023 (line spacing)
constexpr float SCENE_SC_STATS_LX_N   =  240.0f / SCREEN_W;  // ≈ 0.333
constexpr float SCENE_SC_STATS_RX_N   =  500.0f / SCREEN_W;  // ≈ 0.694
constexpr float SCENE_SC_PROMPT_Y_N   =  800.0f / SCREEN_H;  // ≈ 0.625

// Scene – Paused overlay
constexpr float SCENE_PAUSE_Y_N       =  580.0f / SCREEN_H;  // ≈ 0.453

// ── Lane Floor ──────────────────────────────────
constexpr float FLOOR_SHAPE_SIZE     = 36.0f;
constexpr float FLOOR_SHAPE_SPACING  = 50.0f;
constexpr float FLOOR_Y_START        = 12.0f;
constexpr int   FLOOR_SHAPE_COUNT    = 21;
constexpr float FLOOR_OUTLINE_THICK  = 2.0f;
constexpr float FLOOR_ALPHA_REST     = 30.0f;
constexpr float FLOOR_ALPHA_PEAK     = 100.0f;
constexpr float FLOOR_SCALE_REST     = 1.0f;
constexpr float FLOOR_SCALE_PEAK     = 1.3f;
constexpr float FLOOR_PULSE_DECAY    = 0.15f;   // seconds

// ── Shape Colors ──────────────────────────────────
// Used for both obstacles and player character.
// Index by static_cast<int>(Shape).
constexpr Color SHAPE_COLORS[] = {
    {  80, 200, 255, 255 },   // Circle   — cyan/blue
    { 255, 100, 100, 255 },   // Square   — red
    { 100, 255, 100, 255 },   // Triangle — green
    {  80, 180, 255, 255 },   // Hexagon  — neutral blue
};

} // namespace constants
