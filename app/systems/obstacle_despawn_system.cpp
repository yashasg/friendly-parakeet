#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/system_scratch.h"
#include "../components/transform.h"
#include "../constants.h"
#include "../entities/obstacle_render_entity.h"

namespace {

ObstacleDespawnScratch& despawn_scratch_for(entt::registry& reg) {
    return reg.ctx().get<ObstacleDespawnScratch>();
}

}  // namespace

// Destroys obstacle entities that have scrolled past the despawn boundary.
void obstacle_despawn_system(entt::registry& reg, [[maybe_unused]] float dt) {
    // Per-registry scratch retains capacity across frames without sharing mutable
    // state between registries.
    auto& to_destroy = despawn_scratch_for(reg).to_destroy;
    to_destroy.clear();

    auto view = reg.view<ObstacleTag, WorldTransform>();
    for (auto [entity, wt] : view.each()) {
        if (wt.position.y > constants::DESTROY_Y) {
            auto& scratch = despawn_scratch_for(reg);
            if (to_destroy.size() >= to_destroy.capacity()) {
                ++scratch.capacity_exceeded_count;
            }
            to_destroy.push_back(entity);
        }
    }

    for (auto e : to_destroy) destroy_obstacle_with_children(reg, e);
}
