#include "all_systems.h"
#include "../entities/obstacle_entity.h"
#include "../entities/beat_map.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include "../entities/settings.h"

void beat_scheduler_system(entt::registry& reg, float /*dt*/) {
    auto* song = reg.ctx().find<SongState>();
    auto* map  = find_beat_map(reg);
    if (!song || !map || !song->playing) return;
    if (song->scroll_speed <= 0.0f) {
        TraceLog(LOG_WARNING, "beat_scheduler_system skipped: invalid scroll_speed %.3f", song->scroll_speed);
        return;
    }

    // Player calibration (#474). Positive audio_offset_ms delays beat timing:
    // we push spawn_time forward by the same delta so obstacles arrive at
    // PLAYER_Y later, matching the player's perceived audio lag. Negative
    // values advance beat timing. See entities/settings.h.
    const auto* settings = find_settings_state(reg);
    const float audio_offset_sec = settings ? settings::audio_offset_seconds(*settings) : 0.0f;

    while (song->next_spawn_idx < map->beats.size()) {
        const auto& entry = map->beats[song->next_spawn_idx];
        if (entry.kind == ObstacleKind::OnsetMarker) {
            song->next_spawn_idx++;
            continue;
        }
        if (entry.kind != ObstacleKind::ShapeGate) {
            TraceLog(LOG_WARNING, "Skipping unsupported active beatmap obstacle kind %d",
                     static_cast<int>(entry.kind));
            song->next_spawn_idx++;
            continue;
        }

        float beat_time = song->offset + entry.beat_index * song->beat_period;
        if (!entry.has_time_sec &&
            !map->beat_times.empty() &&
            entry.beat_index >= 0 &&
            static_cast<size_t>(entry.beat_index) < map->beat_times.size()) {
            beat_time = map->beat_times[static_cast<size_t>(entry.beat_index)];
        }
        if (entry.has_time_sec) {
            beat_time = entry.time_sec;
        }
        // Beat line is the collision point: calibrated_arrival_time maps to
        // crossing PLAYER_Y, including player audio calibration.
        const float calibrated_arrival_time = beat_time + audio_offset_sec;
        float spawn_time = calibrated_arrival_time - song->lead_time;

        if (song->song_time < spawn_time) break;

        // Compensate for late spawn: if song_time overshot spawn_time,
        // place the obstacle below SPAWN_Y by the overshoot distance
        // so it arrives at the player at exactly calibrated_arrival_time.
        // Clamp the spawn position so a large overshoot cannot place the
        // obstacle below the beat line, where it may never be scored before
        // scrolling off-screen.
        float overshoot = song->song_time - spawn_time;
        float start_y = constants::SPAWN_Y + overshoot * song->scroll_speed;
        float max_start_y = constants::PLAYER_Y;
        float effective_spawn_time = spawn_time;
        if (start_y > max_start_y) {
            start_y = max_start_y;
            // Store an adjusted spawn_time so scroll_system reproduces
            // the clamped initial position via its song-time formula
            // (pos.y = SPAWN_Y + (song_time - spawn_time) * scroll_speed),
            // instead of snapping past the clamp on the first frame.
            effective_spawn_time = song->song_time
                - (max_start_y - constants::SPAWN_Y) / song->scroll_speed;
        }

        // Shape gates use the authored lane directly.
        float x_pos = constants::LANE_X[1];
        const int lane = static_cast<int>(entry.lane);
        if (lane >= 0 && lane < constants::LANE_COUNT) {
            x_pos = constants::LANE_X[lane];
        } else {
            TraceLog(LOG_WARNING, "Invalid shape_gate lane %d; defaulting to center lane", lane);
        }

        const BeatInfo bi{entry.beat_index, calibrated_arrival_time, effective_spawn_time};
        spawn_rhythm_obstacle(reg, {
            entry.kind,
            x_pos,
            start_y,
            entry.shape,
            entry.blocked_mask,
            entry.lane,
            song->scroll_speed
        }, bi);

        song->next_spawn_idx++;
    }
}
