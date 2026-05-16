#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include "../components/rhythm.h"

void motion_system(entt::registry& reg, float dt) {
    // A raw Vector2 component is the ownership marker for dt-integrated
    // movement (issue #1198: MotionVelocity wrapper unwrapped). Song-time-
    // authoritative rhythm obstacles have BeatInfo instead.
    auto motion_view = reg.view<WorldPosition, Vector2>();
    for (auto [entity_id, transform, velocity] : motion_view.each()) {
        (void)entity_id;
        transform.position.x += velocity.x * dt;
        transform.position.y += velocity.y * dt;
    }
}
