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

namespace {
float obstacle_half_height_for_kind(ObstacleKind kind) {
    switch (kind) {
        case ObstacleKind::LowBar:
        case ObstacleKind::HighBar:
            return 20.0f;
        case ObstacleKind::LanePushLeft:
        case ObstacleKind::LanePushRight:
            return 30.0f;
        case ObstacleKind::ShapeGate:
        case ObstacleKind::LaneBlock:
        case ObstacleKind::ComboGate:
        case ObstacleKind::SplitPath:
            return 40.0f;
    }
    return constants::COLLISION_MARGIN;
}
}

void beat_scheduler_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    auto* map  = reg.ctx().find<BeatMap>();
    if (!song || !map || !song->playing) return;

    while (song->next_spawn_idx < map->beats.size()) {
        const auto& entry = map->beats[song->next_spawn_idx];
        if (entry.kind == ObstacleKind::LowBar || entry.kind == ObstacleKind::HighBar) {
            ++song->next_spawn_idx;
            continue;
        }

        float beat_time  = song->offset + entry.beat_index * song->beat_period;
        if (!map->beat_times.empty()) {
            beat_time = map->beat_times[static_cast<size_t>(entry.beat_index)];
        }
        float half_h = obstacle_half_height_for_kind(entry.kind);
        // Beat line is the front edge of the obstacle: beat_time maps to
        // center crossing (PLAYER_Y - half_height).
        float edge_target_y = constants::PLAYER_Y - half_h;
        float edge_lead_time = (edge_target_y - constants::SPAWN_Y) / song->scroll_speed;
        float spawn_time = beat_time - edge_lead_time;

        if (song->song_time < spawn_time) break;

        // Compensate for late spawn: if song_time overshot spawn_time,
        // place the obstacle below SPAWN_Y by the overshoot distance
        // so it arrives at the player at exactly beat_time.
        // Clamp the spawn position so a large overshoot cannot place the
        // obstacle below the beat line, where it may never be scored before
        // scrolling off-screen.
        float overshoot = song->song_time - spawn_time;
        float start_y = constants::SPAWN_Y + overshoot * song->scroll_speed;
        float max_start_y = edge_target_y;
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
