#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../gameobjects/shape_obstacle.h"
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
        constexpr float COLLISION_MARGIN = 40.0f;
        float margin_offset = COLLISION_MARGIN / song->scroll_speed;
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
        float max_start_y = constants::PLAYER_Y - COLLISION_MARGIN;
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

        auto obstacle = reg.create();
        reg.emplace<ObstacleTag>(obstacle);
        reg.emplace<Velocity>(obstacle, 0.0f, song->scroll_speed);
        reg.emplace<DrawLayer>(obstacle, Layer::Game);
        reg.emplace<BeatInfo>(obstacle, entry.beat_index, beat_time, effective_spawn_time);

        switch (entry.kind) {
            case ObstacleKind::ShapeGate: {
                reg.emplace<Position>(obstacle, constants::LANE_X[entry.lane], start_y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
                reg.emplace<RequiredShape>(obstacle, entry.shape);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
                if (entry.shape == Shape::Circle)
                    reg.emplace<Color>(obstacle, Color{80, 200, 255, 255});
                else if (entry.shape == Shape::Square)
                    reg.emplace<Color>(obstacle, Color{255, 100, 100, 255});
                else
                    reg.emplace<Color>(obstacle, Color{100, 255, 100, 255});
                break;
            }
            case ObstacleKind::LaneBlock: {
                int display_lane = 1;
                for (int l = 0; l < 3; ++l) {
                    if ((entry.blocked_mask >> l) & 1) { display_lane = l; break; }
                }
                reg.emplace<Position>(obstacle, constants::LANE_X[display_lane], start_y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::LaneBlock, int16_t{constants::PTS_LANE_BLOCK});
                reg.emplace<BlockedLanes>(obstacle, entry.blocked_mask);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W / 3), 80.0f);
                reg.emplace<Color>(obstacle, Color{255, 60, 60, 255});
                break;
            }
            case ObstacleKind::LowBar: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], start_y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::LowBar, int16_t{constants::PTS_LOW_BAR});
                reg.emplace<RequiredVAction>(obstacle, VMode::Jumping);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 40.0f);
                reg.emplace<Color>(obstacle, Color{255, 180, 0, 255});
                break;
            }
            case ObstacleKind::HighBar: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], start_y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::HighBar, int16_t{constants::PTS_HIGH_BAR});
                reg.emplace<RequiredVAction>(obstacle, VMode::Sliding);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 40.0f);
                reg.emplace<Color>(obstacle, Color{180, 0, 255, 255});
                break;
            }
            case ObstacleKind::ComboGate: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], start_y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::ComboGate, int16_t{constants::PTS_COMBO_GATE});
                reg.emplace<RequiredShape>(obstacle, entry.shape);
                reg.emplace<BlockedLanes>(obstacle, entry.blocked_mask);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
                reg.emplace<Color>(obstacle, Color{200, 100, 255, 255});
                break;
            }
            case ObstacleKind::SplitPath: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], start_y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::SplitPath, int16_t{constants::PTS_SPLIT_PATH});
                reg.emplace<RequiredShape>(obstacle, entry.shape);
                reg.emplace<RequiredLane>(obstacle, entry.lane);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
                reg.emplace<Color>(obstacle, Color{255, 215, 0, 255});
                break;
            }
            case ObstacleKind::LanePushLeft:
            case ObstacleKind::LanePushRight: {
                reg.emplace<Position>(obstacle, constants::LANE_X[entry.lane], start_y);
                reg.emplace<Obstacle>(obstacle, entry.kind, int16_t{constants::PTS_LANE_PUSH});
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W / 3), 60.0f);
                // Orange arrows
                reg.emplace<Color>(obstacle, Color{255, 138, 101, 255});
                break;
            }
        }

        // Spawn visual mesh children for multi-slab obstacle types
        spawn_obstacle_meshes(reg, obstacle);

        song->next_spawn_idx++;
    }
}
