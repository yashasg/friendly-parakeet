#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include "../components/rhythm.h"
#include "../components/obstacle.h"
#include "../constants.h"

void scroll_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();

    // Rhythm obstacles: position derived from song_time, not accumulated dt.
    // This prevents floating-point drift from desynchronizing collisions
    // with the beat grid. (See: "never use anything other than song position")
    if (song && song->playing) {
        auto model_beat_view = reg.view<ObstacleTag, ObstacleScrollZ, BeatInfo>();
        for (auto [entity, oz, info] : model_beat_view.each()) {
            oz.z = constants::SPAWN_Y
                 + (song->song_time - info.spawn_time) * song->scroll_speed;
            if (auto* transform = reg.try_get<WorldTransform>(entity)) {
                transform->position.y = oz.z;
            }
        }

        auto beat_view = reg.view<ObstacleTag, Position, BeatInfo>();
        for (auto [entity, pos, info] : beat_view.each()) {
            pos.y = constants::SPAWN_Y
                  + (song->song_time - info.spawn_time) * song->scroll_speed;
            if (auto* transform = reg.try_get<WorldTransform>(entity)) {
                transform->position.x = pos.x;
                transform->position.y = pos.y;
            }
        }
    }

    // Non-rhythm entities (particles, popups, freeplay obstacles): dt-based
    auto model_view = reg.view<ObstacleTag, ObstacleScrollZ, Velocity>(entt::exclude<BeatInfo>);
    for (auto [entity, oz, vel] : model_view.each()) {
        oz.z += vel.dy * dt;
        if (auto* transform = reg.try_get<WorldTransform>(entity)) {
            transform->position.y = oz.z;
        }
    }

    auto view = reg.view<Position, Velocity>(entt::exclude<BeatInfo>);
    for (auto [entity, pos, vel] : view.each()) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
        if (auto* transform = reg.try_get<WorldTransform>(entity)) {
            transform->position.x = pos.x;
            transform->position.y = pos.y;
        }
    }

    auto motion_view = reg.view<WorldTransform, MotionVelocity>(entt::exclude<BeatInfo>);
    for (auto [entity, transform, velocity] : motion_view.each()) {
        transform.position.x += velocity.value.x * dt;
        transform.position.y += velocity.value.y * dt;
    }
}
