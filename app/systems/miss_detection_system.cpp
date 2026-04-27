#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/game_state.h"
#include "../components/song_state.h"
#include "../constants.h"

// Pure tagger: marks unscored obstacles that have scrolled past DESTROY_Y.
// Runs before scoring_system so that the MissTag is processed the same frame.
// Energy drain and miss_count are handled exclusively by scoring_system's MissTag branch.
void miss_detection_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* gos = reg.ctx().find<GameOverState>();  // #309: hoisted above loop
    auto view = reg.view<ObstacleTag, Obstacle, Position>(entt::exclude<ScoredTag>);
    for (auto [entity, obs, pos] : view.each()) {
        if (pos.y <= constants::DESTROY_Y) continue;

        reg.emplace<MissTag>(entity);
        reg.emplace<ScoredTag>(entity);

        // Set death-cause for the UI (does not override a prior cause).
        if (gos && gos->cause == DeathCause::None) {
            gos->cause = DeathCause::MissedABeat;
        }
    }
}
