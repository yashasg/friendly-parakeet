#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"
#include "../entities/camera_entity.h"

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

// Destroys obstacle entities that have scrolled past the far-Z despawn boundary.
// Two structural paths are checked each frame:
//   1. Model-authority obstacles tracked via ObstacleScrollZ — threshold is the
//      game camera's position.z (read live from the registry).  In headless tests
//      where no GameCamera entity exists, falls back to constants::DESTROY_Y so
//      tests can construct minimal registries.
//   2. Legacy position-authority obstacles tracked via Position.y — threshold is
//      constants::DESTROY_Y.
void obstacle_despawn_system(entt::registry& reg, float /*dt*/) {
    // Per-registry scratch retains capacity across frames without sharing mutable
    // state between registries.
    auto& to_destroy = despawn_scratch_for(reg).to_destroy;
    to_destroy.clear();

    // Resolve the camera-Z despawn threshold for model-authority obstacles.
    auto cam_view = reg.view<GameCamera>();
    const float camera_despawn_z = cam_view.empty()
        ? constants::DESTROY_Y
        : cam_view.get<GameCamera>(cam_view.front()).cam.position.z;

    auto model_view = reg.view<ObstacleTag, ObstacleScrollZ>();
    for (auto [entity, oz] : model_view.each()) {
        if (oz.z > camera_despawn_z)
            to_destroy.push_back(entity);
    }

    for (auto e : to_destroy) reg.destroy(e);
    to_destroy.clear();

    auto view = reg.view<ObstacleTag, Position>();
    for (auto [entity, pos] : view.each()) {
        if (pos.y > constants::DESTROY_Y)
            to_destroy.push_back(entity);
    }

    for (auto e : to_destroy) reg.destroy(e);
}
