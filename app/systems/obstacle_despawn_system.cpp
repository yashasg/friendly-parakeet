#include "all_systems.h"
#include "obstacle_despawn_system.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"
#include "../entities/obstacle_render_entity.h"

// Destroys obstacle entities that have scrolled past the despawn boundary.
void obstacle_despawn_system(entt::registry& reg, [[maybe_unused]] float dt) {
    // Per-registry scratch retains capacity across frames without sharing mutable
    // state between registries.
    auto& scratch = reg.ctx().get<ObstacleDespawnScratch>();
    auto& to_destroy = scratch.to_destroy;
    to_destroy.clear();

    auto view = reg.view<ObstacleTag, WorldPosition>();
    for (auto [entity, wt] : view.each()) {
        if (wt.position.y > constants::DESTROY_Y) {
            if (to_destroy.size() >= to_destroy.capacity()) {
                ++scratch.capacity_exceeded_count;
            }
            to_destroy.push_back(entity);
        }
    }

    for (auto e : to_destroy) destroy_obstacle_with_children(reg, e);
}
