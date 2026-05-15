#include "all_systems.h"
#include "particle_system.h"
#include "../components/particle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include "../entities/settings.h"

namespace {

ParticleSystemScratch& particle_scratch_for(entt::registry& reg) {
    return reg.ctx().get<ParticleSystemScratch>();
}

}  // namespace

void particle_system(entt::registry& reg, float dt) {
    auto& expired = particle_scratch_for(reg).expired;
    expired.clear();

    auto view = reg.view<ParticleTag, ParticleData>();
    for (auto [entity, pdata] : view.each()) {
        pdata.remaining -= dt;
        if (pdata.remaining <= 0.0f) {
            auto& scratch = particle_scratch_for(reg);
            if (expired.size() >= expired.capacity()) {
                ++scratch.capacity_exceeded_count;
            }
            expired.push_back(entity);
        }
    }

    for (auto entity : expired) {
        reg.destroy(entity);
    }

    // Reduce-motion (#534): clamp decorative particle velocity (and so
    // gravity accumulation) without removing the particles themselves.
    // The burst still spawns, ages, and despawns on the same schedule —
    // it just visibly calms down. Functional/scoring state is untouched
    // because particles carry no gameplay payload.
    const auto* settings_ptr = find_settings_state(reg);
    const bool reduce_motion = settings_ptr && settings_ptr->reduce_motion;

    auto vel_view = reg.view<ParticleTag, Vector2>();
    for (auto [entity, vel] : vel_view.each()) {
        (void)entity;
        if (reduce_motion) {
            vel.x = 0.0f;
            vel.y = 0.0f;
        } else {
            vel.y += constants::PARTICLE_GRAVITY * dt;
        }
    }
}
