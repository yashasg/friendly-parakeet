#include "all_systems.h"
#include "../components/particle.h"
#include "../components/transform.h"
#include "../components/lifetime.h"
#include "../components/rendering.h"

void particle_system(entt::registry& reg, float dt) {
    // Gravity on particles
    auto vel_view = reg.view<ParticleTag, Velocity>();
    for (auto [entity, vel] : vel_view.each()) {
        vel.dy += 600.0f * dt; // gravity
    }
}
