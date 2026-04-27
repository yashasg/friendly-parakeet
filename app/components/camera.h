#pragma once
#include <cstdint>
#include <raylib.h>

// ── Camera configuration ────────────────────────────────────────────────────
// Game logic and 3D world share the same coordinate space (1 unit = 1 cm).
// The model-to-world transform is scale + translate in game coordinates.

// Tagged camera singletons so the registry distinguishes 3D vs 2D.
struct GameCamera  { Camera3D cam; };
struct UICamera    { Camera2D cam; };

// Per-frame render parameters computed from SongState beat pulse.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};

// Render targets for world (3D) and UI (2D) layers.
// RAII owner: destructor calls UnloadRenderTexture only when owned == true,
// so both the manual camera::shutdown path and registry-ctx destruction are
// safe to run in any order without double-unloading.
struct RenderTargets {
    RenderTexture2D world = {};
    RenderTexture2D ui    = {};
    bool            owned = false;

    RenderTargets() = default;
    // Construct from two live GPU render textures; sets owned = true.
    RenderTargets(RenderTexture2D w, RenderTexture2D u) noexcept
        : world{w}, ui{u}, owned{true} {}

    RenderTargets(const RenderTargets&)            = delete;
    RenderTargets& operator=(const RenderTargets&) = delete;
    RenderTargets(RenderTargets&&) noexcept;
    RenderTargets& operator=(RenderTargets&&) noexcept;
    ~RenderTargets();

    // Idempotent GPU unload. Safe to call more than once; clears owned flag.
    void release();
};
