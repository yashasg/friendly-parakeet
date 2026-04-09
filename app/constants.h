#pragma once

#include <cstdint>

namespace constants {

// ── Logical Resolution ────────────────────────────
constexpr int   SCREEN_W          = 720;
constexpr int   SCREEN_H          = 1280;

// ── Lanes ─────────────────────────────────────────
constexpr int   LANE_COUNT        = 3;
constexpr float LANE_X[3]         = { 180.0f, 360.0f, 540.0f };
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

// ── Burnout Zones (distance from player, in px) ──
constexpr float ZONE_SAFE_MAX     = 700.0f;
constexpr float ZONE_SAFE_MIN     = 500.0f;
constexpr float ZONE_RISKY_MIN    = 300.0f;
constexpr float ZONE_DANGER_MIN   = 140.0f;
constexpr float ZONE_DEAD_MIN     = 0.0f;

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
constexpr int   PTS_PER_SECOND    = 10;
constexpr int   CHAIN_BONUS[5]    = { 0, 0, 50, 100, 200 };

// ── Difficulty Timeline ───────────────────────────
constexpr float SPEED_RAMP_RATE   = 0.011f;
constexpr float SPAWN_RAMP_RATE   = 0.003f;
constexpr float BURNOUT_SHRINK    = 0.002f;
constexpr float INITIAL_SPAWN_INT = 1.8f;
constexpr float MIN_SPAWN_INT     = 0.5f;

// ── Rendering ─────────────────────────────────────
constexpr float POPUP_DURATION    = 1.2f;
constexpr float PARTICLE_LIFE     = 0.6f;
constexpr int   MAX_PARTICLES     = 50;

// ── Input ─────────────────────────────────────────
constexpr float SWIPE_ZONE_SPLIT  = 0.80f;
constexpr float MIN_SWIPE_DIST    = 50.0f;
constexpr float MAX_SWIPE_TIME    = 0.3f;
constexpr float BUTTON_DEBOUNCE   = 0.1f;

// ── UI Layout ─────────────────────────────────────
constexpr float BURNOUT_BAR_Y     = 1020.0f;
constexpr float BURNOUT_BAR_H     = 20.0f;
constexpr float BUTTON_Y          = 1140.0f;
constexpr float BUTTON_W          = 140.0f;
constexpr float BUTTON_H          = 100.0f;
constexpr float BUTTON_SPACING    = 60.0f;

} // namespace constants
