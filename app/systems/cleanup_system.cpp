#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/song_state.h"
#include "../constants.h"

#include <vector>

void cleanup_system(entt::registry& reg, float /*dt*/) {
    std::vector<entt::entity> to_destroy;

    auto view = reg.view<ObstacleTag, Position>();
    for (auto [entity, pos] : view.each()) {
        if (pos.y <= constants::DESTROY_Y) continue;

        if (!reg.all_of<ScoredTag>(entity)) {
            if (reg.all_of<Obstacle>(entity)) {
                // Scoreable obstacle scrolled past without being graded — count as a miss.
                reg.emplace<MissTag>(entity);
                reg.emplace<ScoredTag>(entity);

                if (auto* energy = reg.ctx().find<EnergyState>()) {
                    energy->energy -= constants::ENERGY_DRAIN_MISS;
                    if (energy->energy < 1e-6f) energy->energy = 0.0f;
                }
                if (auto* results = reg.ctx().find<SongResults>()) {
                    results->miss_count++;
                }
            } else {
                to_destroy.push_back(entity);
            }
        } else {
            to_destroy.push_back(entity);
        }
    }

    for (auto e : to_destroy) reg.destroy(e);
}
