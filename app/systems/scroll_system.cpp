#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include "../components/rhythm.h"
#include "../components/obstacle.h"
#include "../constants.h"

void scroll_system(entt::registry& reg, float dt) {
    auto* song = reg.ctx().find<SongState>();

    // Rhythm obstacles: position derived from song_time, not accumulated dt.
    // This prevents floating-point drift from desynchronizing collisions
    // with the beat grid. (See: "never use anything other than song position")
    if (song && song->playing) {
        auto model_beat_view = reg.view<ObstacleTag, ObstacleScrollZ, BeatInfo>();
        for (auto [entity, oz, info] : model_beat_view.each()) {
            (void)entity;
            oz.z = constants::SPAWN_Y
                 + (song->song_time - info.spawn_time) * song->scroll_speed;
            if (auto* wt = reg.try_get<WorldTransform>(entity)) {
                wt->position.y = oz.z;
            }
        }

        auto beat_view = reg.view<ObstacleTag, Position, BeatInfo>();
        for (auto [entity, pos, info] : beat_view.each()) {
            (void)entity;
            pos.y = constants::SPAWN_Y
                  + (song->song_time - info.spawn_time) * song->scroll_speed;
            if (auto* wt = reg.try_get<WorldTransform>(entity)) {
                wt->position.x = pos.x;
                wt->position.y = pos.y;
            }
        }
    }

    // Non-rhythm entities (freeplay obstacles): dt-based
    auto model_view =
        reg.view<ObstacleTag, ObstacleScrollZ, Velocity>(entt::exclude<BeatInfo>);
    for (auto [entity, oz, vel] : model_view.each()) {
        (void)entity;
        oz.z += vel.dy * dt;
        if (auto* wt = reg.try_get<WorldTransform>(entity)) {
            wt->position.y = oz.z;
        }
    }
}
