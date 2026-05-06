#include "../components/transform.h"
#include "../components/rhythm.h"
#include "../components/obstacle.h"
#include "../components/registry_context.h"
#include "../components/song_state.h"
#include "../constants.h"
#include <entt/entt.hpp>

void scroll_system(entt::registry& reg, float dt) {
    auto* song = registry_ctx_find<SongState>(reg);

    // Rhythm obstacles: position derived from song_time, not accumulated dt.
    // This prevents floating-point drift from desynchronizing collisions
    // with the beat grid. (See: "never use anything other than song position")
    if (song && song->playing) {
        auto model_beat_view = reg.view<ObstacleTag, ObstacleScrollZ, BeatInfo>();
        for (auto [entity, oz, info] : model_beat_view.each()) {
            (void)entity;
            oz.z = constants::SPAWN_Y
                 + (song->song_time - info.spawn_time) * song->scroll_speed;
        }

        auto beat_view = reg.view<ObstacleTag, WorldTransform, BeatInfo>(entt::exclude<ObstacleScrollZ>);
        for (auto [entity, wt, info] : beat_view.each()) {
            (void)entity;
            wt.position.y = constants::SPAWN_Y
                          + (song->song_time - info.spawn_time) * song->scroll_speed;
        }
    }

    // Non-rhythm entities (freeplay obstacles): dt-based
    auto model_view =
        reg.view<ObstacleTag, ObstacleScrollZ, MotionVelocity>(entt::exclude<BeatInfo>);
    for (auto [entity, oz, vel] : model_view.each()) {
        (void)entity;
        oz.z += vel.value.y * dt;
    }
}
