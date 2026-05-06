#include "../entities/obstacle_entity.h"
#include "../components/beat_map.h"
#include "../components/registry_context.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../constants.h"
#include <entt/entt.hpp>

namespace {

float scheduled_beat_time(const SongState& song, const BeatMap& map, int beat_index) {
    if (beat_index >= 0 && static_cast<size_t>(beat_index) < map.beat_times.size()) {
        return map.beat_times[static_cast<size_t>(beat_index)];
    }
    return song.offset + beat_index * song.beat_period;
}

}  // namespace

void beat_scheduler_system(entt::registry& reg, float /*dt*/) {
    auto* song = registry_ctx_find<SongState>(reg);
    auto* map  = registry_ctx_find<BeatMap>(reg);
    if (!song || !map || !song->playing) return;

    while (song->next_spawn_idx < map->beats.size()) {
        const auto& entry = map->beats[song->next_spawn_idx];

        const float beat_time = scheduled_beat_time(*song, *map, entry.beat_index);
        // Beat line is the collision point: beat_time maps to crossing PLAYER_Y.
        float spawn_time = beat_time - song->lead_time;

        if (song->song_time < spawn_time) break;

        // Compensate for late spawn: if song_time overshot spawn_time,
        // place the obstacle below SPAWN_Y by the overshoot distance
        // so it arrives at the player at exactly beat_time.
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

        // Compute x: LaneBlock derives it from blocked_mask; lane-based kinds
        // use entry.lane directly; bar/gate types default to center lane.
        float x_pos = constants::LANE_X[1];
        if (entry.kind == ObstacleKind::ShapeGate) {
            x_pos = constants::LANE_X[static_cast<int>(entry.lane)];
        } else if (entry.kind == ObstacleKind::LaneBlock) {
            int display_lane = 1;
            for (int l = 0; l < 3; ++l) {
                if ((entry.blocked_mask >> l) & 1) { display_lane = l; break; }
            }
            x_pos = constants::LANE_X[display_lane];
        }

        const BeatInfo bi{entry.beat_index, beat_time, effective_spawn_time};
        spawn_obstacle(reg, {
            entry.kind,
            x_pos,
            start_y,
            entry.shape,
            entry.blocked_mask,
            entry.lane,
            song->scroll_speed
        }, &bi);

        song->next_spawn_idx++;
    }
}
