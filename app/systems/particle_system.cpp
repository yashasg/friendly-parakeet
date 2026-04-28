#include "all_systems.h"
#include "../components/particle.h"
#include "../components/transform.h"
#include "../components/lifetime.h"
#include "../components/rendering.h"
#include "../constants.h"

void particle_system(entt::registry& reg, float dt) {
    auto vel_view = reg.view<ParticleTag, Velocity>();
    for (auto [entity, vel] : vel_view.each()) {
        vel.dy += constants::PARTICLE_GRAVITY * dt;
    }
}
