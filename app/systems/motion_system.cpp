#include "all_systems.h"
#include "../components/transform.h"
#include "../components/rhythm.h"

void motion_system(entt::registry& reg, float dt) {
    // WorldTransform+MotionVelocity entities (obstacles, popups, particles, player).
    // Excludes BeatInfo entities (position derived from song_time in scroll_system).
    // Excludes ObstacleScrollZ entities (scroll_system's model_view owns their
    // dt-based integration via oz.z; including them here would double-integrate).
    auto motion_view = reg.view<WorldTransform, MotionVelocity>(entt::exclude<BeatInfo, ObstacleScrollZ>);
    for (auto [entity_id, transform, velocity] : motion_view.each()) {
        (void)entity_id;
        transform.position.x += velocity.value.x * dt;
        transform.position.y += velocity.value.y * dt;
    }
}
