#include "all_systems.h"
#include "../components/transform.h"

void scroll_system(entt::registry& reg, float dt) {
    auto view = reg.view<Position, Velocity>();
    for (auto [entity, pos, vel] : view.each()) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    }
}
