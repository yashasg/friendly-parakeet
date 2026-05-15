#pragma once

#include <raylib.h>
#include <cstdint>
#include <algorithm>

#include "../constants.h"

// Letterbox scale/offset: window to virtual space. camera_system ctx singleton,
// recomputed once per frame by compute_screen_transform() before input_system
// runs. Coordinate-conversion helper screen_to_virtual() lives in
// app/systems/camera_system.h. Relocated out of app/components/rendering.h
// (issue #1194 SPLIT).
struct ScreenTransform {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale    = 1.0f;
};

// Per-frame render parameters computed from SongState beat pulse.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};

namespace floor_visuals {

inline float calibrated_beat_time(float beat_time, float audio_offset_sec) {
    return beat_time + audio_offset_sec;
}

inline float beat_line_z(float song_time, float beat_time, float scroll_speed,
                         float audio_offset_sec) {
    return constants::PLAYER_Y
        + (song_time - calibrated_beat_time(beat_time, audio_offset_sec)) * scroll_speed;
}

inline float pulse_for_beat(float song_time, float beat_time, float audio_offset_sec) {
    const float time_since_beat =
        song_time - calibrated_beat_time(beat_time, audio_offset_sec);
    const float pulse_t =
        std::clamp(time_since_beat / constants::FLOOR_PULSE_DECAY, 0.0f, 1.0f);
    const float ease = 1.0f - (1.0f - pulse_t) * (1.0f - pulse_t);
    return 1.0f - ease;
}

} // namespace floor_visuals

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

namespace camera {

// RAII owner for GPU mesh/shader resources.
// Destructor calls unload only when owned == true, so both the manual
// camera::shutdown path and registry-ctx destruction are safe to run in
// any order without double-unloading.
struct ShapeMeshes {
    Mesh     shapes[4] = {};  // Circle, Square, Triangle, Hexagon
    Mesh     slab      = {};  // unit cube for obstacles
    Mesh     quad      = {};  // flat quad for particles
    Material material  = {};  // shared material with directional shading shader
    bool     owned     = false;

    ShapeMeshes() = default;
    ShapeMeshes(const ShapeMeshes&)            = delete;
    ShapeMeshes& operator=(const ShapeMeshes&) = delete;
    ShapeMeshes(ShapeMeshes&&) noexcept;
    ShapeMeshes& operator=(ShapeMeshes&&) noexcept;
    ~ShapeMeshes();

    // Idempotent GPU unload. Safe to call more than once; clears owned flag.
    void release();
};

} // namespace camera
