#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
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
        constexpr float COLLISION_MARGIN = 40.0f;
        float margin_offset = COLLISION_MARGIN / song->scroll_speed;
        float spawn_time = beat_time - song->lead_time + margin_offset;

        if (song->song_time < spawn_time) break;

        auto obstacle = reg.create();
        reg.emplace<ObstacleTag>(obstacle);
        reg.emplace<Velocity>(obstacle, 0.0f, song->scroll_speed);
        reg.emplace<DrawLayer>(obstacle, Layer::Game);
        reg.emplace<BeatInfo>(obstacle, entry.beat_index, beat_time, spawn_time);

        switch (entry.kind) {
            case ObstacleKind::ShapeGate: {
                reg.emplace<Position>(obstacle, constants::LANE_X[entry.lane], constants::SPAWN_Y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
                reg.emplace<RequiredShape>(obstacle, entry.shape);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
                if (entry.shape == Shape::Circle)
                    reg.emplace<DrawColor>(obstacle, uint8_t{80}, uint8_t{200}, uint8_t{255}, uint8_t{255});
                else if (entry.shape == Shape::Square)
                    reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{100}, uint8_t{100}, uint8_t{255});
                else
                    reg.emplace<DrawColor>(obstacle, uint8_t{100}, uint8_t{255}, uint8_t{100}, uint8_t{255});
                break;
            }
            case ObstacleKind::LaneBlock: {
                int display_lane = 1;
                for (int l = 0; l < 3; ++l) {
                    if ((entry.blocked_mask >> l) & 1) { display_lane = l; break; }
                }
                reg.emplace<Position>(obstacle, constants::LANE_X[display_lane], constants::SPAWN_Y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::LaneBlock, int16_t{constants::PTS_LANE_BLOCK});
                reg.emplace<BlockedLanes>(obstacle, entry.blocked_mask);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W / 3), 80.0f);
                reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{60}, uint8_t{60}, uint8_t{255});
                break;
            }
            case ObstacleKind::LowBar: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::LowBar, int16_t{constants::PTS_LOW_BAR});
                reg.emplace<RequiredVAction>(obstacle, VMode::Jumping);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 40.0f);
                reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{180}, uint8_t{0}, uint8_t{255});
                break;
            }
            case ObstacleKind::HighBar: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::HighBar, int16_t{constants::PTS_HIGH_BAR});
                reg.emplace<RequiredVAction>(obstacle, VMode::Sliding);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 40.0f);
                reg.emplace<DrawColor>(obstacle, uint8_t{180}, uint8_t{0}, uint8_t{255}, uint8_t{255});
                break;
            }
            case ObstacleKind::ComboGate: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::ComboGate, int16_t{constants::PTS_COMBO_GATE});
                reg.emplace<RequiredShape>(obstacle, entry.shape);
                reg.emplace<BlockedLanes>(obstacle, entry.blocked_mask);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
                reg.emplace<DrawColor>(obstacle, uint8_t{200}, uint8_t{100}, uint8_t{255}, uint8_t{255});
                break;
            }
            case ObstacleKind::SplitPath: {
                reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
                reg.emplace<Obstacle>(obstacle, ObstacleKind::SplitPath, int16_t{constants::PTS_SPLIT_PATH});
                reg.emplace<RequiredShape>(obstacle, entry.shape);
                reg.emplace<RequiredLane>(obstacle, entry.lane);
                reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
                reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{215}, uint8_t{0}, uint8_t{255});
                break;
            }
        }

        song->next_spawn_idx++;
    }
}
