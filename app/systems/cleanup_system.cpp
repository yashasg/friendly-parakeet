#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"

#include <vector>

void cleanup_system(entt::registry& reg, float /*dt*/) {
    // Static buffer: capacity is retained across frames; clear() keeps the allocation.
    // This eliminates per-frame heap allocation on the hot path (#242).
    static std::vector<entt::entity> to_destroy;
    to_destroy.clear();

    auto model_view = reg.view<ObstacleTag, ObstacleScrollZ>();
    for (auto [entity, oz] : model_view.each()) {
        if (oz.z > constants::DESTROY_Y)
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
