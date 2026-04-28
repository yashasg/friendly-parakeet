#include "all_systems.h"
#include "../components/lifetime.h"
#include <vector>

void lifetime_system(entt::registry& reg, float dt) {
    static std::vector<entt::entity> expired;  // #308: retain capacity across frames
    expired.clear();
    auto view = reg.view<Lifetime>();
    for (auto [entity, life] : view.each()) {
        life.remaining -= dt;
        if (life.remaining <= 0.0f) {
            expired.push_back(entity);
        }
    }
    for (auto e : expired) {
        reg.destroy(e);
    }
}
