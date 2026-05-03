#include "all_systems.h"
#include "../components/transform.h"
#include "../components/rhythm.h"

void motion_system(entt::registry& reg, float dt) {
    // Legacy Position+Velocity entities (particles, popups, freeplay entities
    // not yet migrated to WorldTransform+MotionVelocity). Excludes BeatInfo
    // entities whose positions are derived from song_time in scroll_system.
    auto vel_view = reg.view<Position, Velocity>(entt::exclude<BeatInfo>);
    for (auto [entity, pos, vel] : vel_view.each()) {
        (void)entity;
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
        if (auto* wt = reg.try_get<WorldTransform>(entity)) {
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
