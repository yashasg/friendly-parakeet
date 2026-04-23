#pragma once
#include <cstdint>

// ── Camera configuration ────────────────────────────────────────────────────
// Game logic operates in "game-pixel" coordinates (720×1280).
// The 3D world uses a scaled-down coordinate system so objects fit the
// Camera3D frustum.  WORLD_SCALE is the single conversion factor:
//   world_coord = game_coord / WORLD_SCALE

constexpr float WORLD_SCALE = 10.0f;
constexpr float INV_WORLD_SCALE = 1.0f / WORLD_SCALE;

// Convert a game-pixel coordinate to world units.
constexpr float to_world(float game_coord) { return game_coord * INV_WORLD_SCALE; }

// Per-frame render parameters computed from SongState beat pulse.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};

