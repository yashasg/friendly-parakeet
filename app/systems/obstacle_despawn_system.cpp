#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"
#include "../entities/obstacle_render_entity.h"
#include "../tags/tags.h"

// Destroys obstacle entities that have scrolled past the despawn boundary.
void obstacle_despawn_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto view = reg.view<ObstacleTag, WorldPosition>();
    for (auto [entity, wt] : view.each()) {
        if (wt.position.y > constants::DESTROY_Y) {
            reg.emplace<PendingObstacleDespawnTag>(entity);
        }
    }

    // Drain: destroy_obstacle_with_children() destroys the parent (the
    // entity that owns PendingObstacleDespawnTag), which shrinks the
    // tag view's packed storage by one. Iterating via *view.begin()
    // until the view is empty is safe and avoids a temporary buffer.
    auto pending = reg.view<PendingObstacleDespawnTag>();
    while (!pending.empty()) {
        destroy_obstacle_with_children(reg, *pending.begin());
    }
}
