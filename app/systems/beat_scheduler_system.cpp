#include "all_systems.h"
#include "../entities/obstacle_entity.h"
#include "../components/beat_map.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"

void beat_scheduler_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    auto* map  = reg.ctx().find<BeatMap>();
    if (!song || !map || !song->playing) return;

    while (song->next_spawn_idx < map->beats.size()) {
        const auto& entry = map->beats[song->next_spawn_idx];

        float beat_time  = song->offset + entry.beat_index * song->beat_period;
        // Compensate for collision margin: collision resolves COLLISION_MARGIN px
        // before the obstacle reaches PLAYER_Y, so spawn slightly later.
        float margin_offset = constants::COLLISION_MARGIN / song->scroll_speed;
        float spawn_time = beat_time - song->lead_time + margin_offset;

        if (song->song_time < spawn_time) break;

        // Compensate for late spawn: if song_time overshot spawn_time,
        // place the obstacle below SPAWN_Y by the overshoot distance
        // so it arrives at the player at exactly beat_time.
        // Clamp the spawn position so a large overshoot cannot place the
        // obstacle at or below the collision window, where it may never be
        // scored before scrolling off-screen.
        float overshoot = song->song_time - spawn_time;
        float start_y = constants::SPAWN_Y + overshoot * song->scroll_speed;
        float max_start_y = constants::PLAYER_Y - constants::COLLISION_MARGIN;
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
        if (entry.kind == ObstacleKind::ShapeGate ||
            entry.kind == ObstacleKind::LanePushLeft ||
            entry.kind == ObstacleKind::LanePushRight) {
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
