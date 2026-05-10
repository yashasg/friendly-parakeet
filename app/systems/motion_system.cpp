#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include "../components/rhythm.h"

void motion_system(entt::registry& reg, float dt) {
    // MotionVelocity is the ownership marker for dt-integrated movement.
    // Song-time-authoritative rhythm obstacles have BeatInfo instead.
    auto motion_view = reg.view<WorldTransform, MotionVelocity>();
    for (auto [entity_id, transform, velocity] : motion_view.each()) {
        (void)entity_id;
        transform.position.x += velocity.value.x * dt;
        transform.position.y += velocity.value.y * dt;
    }
}
