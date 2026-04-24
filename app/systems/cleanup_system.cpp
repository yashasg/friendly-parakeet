#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../constants.h"

void cleanup_system(entt::registry& reg, float /*dt*/) {
    auto view = reg.view<ObstacleTag, Position>();
    for (auto [entity, pos] : view.each()) {
        if (pos.y > constants::DESTROY_Y) {
            reg.destroy(entity);
        }
    }
}
