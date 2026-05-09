#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"

#include <vector>

namespace {

struct ObstacleDespawnScratch {
    std::vector<entt::entity> to_destroy;
};

ObstacleDespawnScratch& despawn_scratch_for(entt::registry& reg) {
    if (auto* scratch = reg.ctx().find<ObstacleDespawnScratch>()) {
        return *scratch;
    }
    return reg.ctx().emplace<ObstacleDespawnScratch>();
}

}  // namespace

// Destroys obstacle entities that have scrolled past the despawn boundary.
void obstacle_despawn_system(entt::registry& reg, float /*dt*/) {
    // Per-registry scratch retains capacity across frames without sharing mutable
    // state between registries.
    auto& to_destroy = despawn_scratch_for(reg).to_destroy;
    to_destroy.clear();

    auto view = reg.view<ObstacleTag, WorldTransform>();
    for (auto [entity, wt] : view.each()) {
        if (wt.position.y > constants::DESTROY_Y)
            to_destroy.push_back(entity);
    }

    for (auto e : to_destroy) reg.destroy(e);
}
