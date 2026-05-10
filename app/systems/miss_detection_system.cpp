#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/game_state.h"
#include "../constants.h"

// Pure tagger: marks unscored obstacles that have scrolled past DESTROY_Y.
// Runs before scoring_system so that the MissTag is processed the same frame.
// Energy drain and miss_count are handled exclusively by scoring_system's MissTag branch.
void miss_detection_system(entt::registry& reg, float /*dt*/) {
    auto view = reg.view<ObstacleTag, WorldTransform>(entt::exclude<ScoredTag>);
    for (auto [entity, wt] : view.each()) {
        if (wt.position.y <= constants::DESTROY_Y) continue;

        reg.get_or_emplace<MissTag>(entity);
        reg.get_or_emplace<ScoredTag>(entity);
    }
}
