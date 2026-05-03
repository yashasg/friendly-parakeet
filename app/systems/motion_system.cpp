#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include "../components/rhythm.h"

void motion_system(entt::registry& reg, float dt) {
    // WorldTransform+MotionVelocity entities (obstacles, popups, particles, player).
    // Excludes BeatInfo entities whose positions are derived from song_time in
    // scroll_system.  Bridges to Position when present (obstacle Position authority).
    auto motion_view = reg.view<WorldTransform, MotionVelocity>(entt::exclude<BeatInfo>);
    for (auto [entity_id, transform, velocity] : motion_view.each()) {
        (void)entity_id;
        transform.position.x += velocity.value.x * dt;
        transform.position.y += velocity.value.y * dt;
        if (auto* pos = reg.try_get<Position>(entity_id)) {
            pos->x = transform.position.x;
            pos->y = transform.position.y;
        }
    }
}
