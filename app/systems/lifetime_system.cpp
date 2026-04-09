#include "all_systems.h"
#include "../components/lifetime.h"

void lifetime_system(entt::registry& reg, float dt) {
    auto view = reg.view<Lifetime>();
    for (auto [entity, life] : view.each()) {
        life.remaining -= dt;
        if (life.remaining <= 0.0f) {
            reg.destroy(entity);
        }
    }
}
