#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/game_state.h"
#include "../constants.h"

// Pure tagger: marks unscored obstacles that have scrolled past DESTROY_Y.
// Runs before scoring_system so that the MissTag is processed the same frame.
// Energy drain and miss_count are handled exclusively by scoring_system's MissTag branch.
void miss_detection_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto model_view = reg.view<ObstacleTag, ObstacleScrollZ>(entt::exclude<ScoredTag>);
    for (auto [entity, oz] : model_view.each()) {
        if (oz.z <= constants::DESTROY_Y) continue;

        reg.emplace<MissTag>(entity);
        reg.emplace<ScoredTag>(entity);
    }

    auto view = reg.view<ObstacleTag, Position>(entt::exclude<ScoredTag>);
    for (auto [entity, pos] : view.each()) {
        if (pos.y <= constants::DESTROY_Y) continue;

        reg.emplace<MissTag>(entity);
        reg.emplace<ScoredTag>(entity);
    }
}
