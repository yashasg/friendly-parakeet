#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"
#include <entt/entt.hpp>

// Pure tagger: marks unscored obstacles that have scrolled past DESTROY_Y.
// Runs before scoring_system so that the MissTag is processed the same frame.
// Energy drain and miss_count are handled exclusively by scoring_system's MissTag branch.
void miss_detection_system(entt::registry& reg, float /*dt*/) {
    auto model_view = reg.view<ObstacleTag, ObstacleScrollZ>(entt::exclude<ScoredTag>);
    for (auto [entity, oz] : model_view.each()) {
        if (oz.z <= constants::DESTROY_Y) continue;

        reg.emplace<MissTag>(entity);
        reg.emplace<ScoredTag>(entity);
    }

    auto view = reg.view<ObstacleTag, WorldTransform>(entt::exclude<ScoredTag, ObstacleScrollZ>);
    for (auto [entity, wt] : view.each()) {
        if (wt.position.y <= constants::DESTROY_Y) continue;

        reg.emplace<MissTag>(entity);
        reg.emplace<ScoredTag>(entity);
    }
}
