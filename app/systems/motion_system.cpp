#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include "../components/rhythm.h"

void motion_system(entt::registry& reg, float dt) {
    // Legacy Position+Velocity entities (freeplay obstacles not yet migrated
    // to ObstacleScrollZ+MotionVelocity, issue #349).  Popups and particles
    // have already migrated to WorldTransform+MotionVelocity; this view is now
    // exclusive to freeplay (non-BeatInfo) obstacles.  Excludes BeatInfo
    // entities whose positions are derived from song_time in scroll_system.
    auto vel_view = reg.view<Position, Velocity>(entt::exclude<BeatInfo>);
    for (auto [entity, pos, vel] : vel_view.each()) {
        (void)entity;
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
        if (auto* wt = reg.try_get<WorldTransform>(entity)) {
            // migration bridge for issue #349 — delete when obstacles fully migrate to WorldTransform+MotionVelocity
            wt->position.x = pos.x;
            wt->position.y = pos.y;
        }
    }

    // WorldTransform+MotionVelocity entities. Excludes BeatInfo entities whose
    // positions are derived from song_time in scroll_system.
    auto motion_view = reg.view<WorldTransform, MotionVelocity>(entt::exclude<BeatInfo>);
    for (auto [entity_id, transform, velocity] : motion_view.each()) {
        (void)entity_id;
        transform.position.x += velocity.value.x * dt;
        transform.position.y += velocity.value.y * dt;
    }
}
