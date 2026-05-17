#pragma once

#include <cstdint>
#include <algorithm>

#include "../constants.h"

// Plain-data, entity/registry-ctx siblings of the camera/render subsystem.
// The RAII GPU resource owners (`RenderTargets`, `camera::ShapeMeshes`) live
// in `app/systems/camera_resources.h` (issue #1351) so that `app/components/`
// stays reserved for atomic, queryable entity-owned tables per
// .squad/decisions.md §"app/components parallel audit verdict (consolidated)".

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
