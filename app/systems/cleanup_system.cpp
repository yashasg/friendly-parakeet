#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"

#include <vector>

void cleanup_system(entt::registry& reg, float /*dt*/) {
    std::vector<entt::entity> to_destroy;

    auto view = reg.view<ObstacleTag, Position>();
    for (auto [entity, pos] : view.each()) {
        if (pos.y > constants::DESTROY_Y)
            to_destroy.push_back(entity);
    }

    for (auto e : to_destroy) reg.destroy(e);
}

