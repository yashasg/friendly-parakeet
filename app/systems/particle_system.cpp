#include "all_systems.h"
#include "../components/particle.h"
#include "../components/transform.h"
#include "../components/lifetime.h"
#include "../components/rendering.h"

void particle_system(entt::registry& reg, float dt) {
    // Update existing particles: shrink over lifetime
    auto view = reg.view<ParticleTag, ParticleData, Lifetime>();
    for (auto [entity, pdata, life] : view.each()) {
        float ratio = life.remaining / life.max_time;
        pdata.size = pdata.start_size * ratio;
    }

    // Gravity on particles
    auto vel_view = reg.view<ParticleTag, Velocity>();
    for (auto [entity, vel] : vel_view.each()) {
        vel.dy += 600.0f * dt; // gravity
    }
}
