#include "../components/obstacle.h"
#include "../components/registry_context.h"
#include "../components/transform.h"
#include "../constants.h"
#include "../entities/camera_entity.h"

#include <entt/entt.hpp>
#include <vector>

namespace {

struct ObstacleDespawnScratch {
    std::vector<entt::entity> to_destroy;
};

}  // namespace

// Destroys obstacle entities that have scrolled past the far-Z despawn boundary.
// Two structural paths are checked each frame:
//   1. Model-authority obstacles tracked via ObstacleScrollZ — threshold is the
//      game camera's position.z (read live from the registry).  In headless tests
//      where no GameCamera entity exists, falls back to constants::DESTROY_Y so
//      tests can construct minimal registries.
//   2. WorldTransform-authority obstacles (entities without ObstacleScrollZ) —
//      threshold is constants::DESTROY_Y on WorldTransform.position.y.
void obstacle_despawn_system(entt::registry& reg, float /*dt*/) {
    // Per-registry scratch retains capacity across frames without sharing mutable
    // state between registries.
    auto& scratch = registry_ctx_get_or_emplace<ObstacleDespawnScratch>(reg);
    auto& to_destroy = scratch.to_destroy;
    to_destroy.clear();

    // Resolve the camera-Z despawn threshold for model-authority obstacles.
    const auto* camera = try_game_camera(reg);
    const float camera_despawn_z = camera
        ? camera->cam.position.z
        : constants::DESTROY_Y;

    auto model_view = reg.view<ObstacleTag, ObstacleScrollZ>();
    for (auto [entity, oz] : model_view.each()) {
        if (oz.z > camera_despawn_z)
            to_destroy.push_back(entity);
    }

    for (auto e : to_destroy) reg.destroy(e);
    to_destroy.clear();

    auto view = reg.view<ObstacleTag, WorldTransform>(entt::exclude<ObstacleScrollZ>);
    for (auto [entity, wt] : view.each()) {
        if (wt.position.y > constants::DESTROY_Y)
            to_destroy.push_back(entity);
    }

    for (auto e : to_destroy) reg.destroy(e);
}
