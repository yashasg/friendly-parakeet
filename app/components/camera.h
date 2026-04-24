#pragma once
#include <cstdint>
#include <raylib.h>

// ── Camera configuration ────────────────────────────────────────────────────
// Game logic uses game-pixel coordinates (720×1280).
// 3D world uses 1/10 scale: world_coord = game_coord / WORLD_SCALE.

constexpr float WORLD_SCALE = 10.0f;
constexpr float INV_WORLD_SCALE = 1.0f / WORLD_SCALE;
constexpr float to_world(float game_coord) { return game_coord * INV_WORLD_SCALE; }

// Per-frame render parameters computed from SongState beat pulse.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};

// Render targets for world (3D) and UI (2D) layers.
struct RenderTargets {
    RenderTexture2D world;
    RenderTexture2D ui;
};
