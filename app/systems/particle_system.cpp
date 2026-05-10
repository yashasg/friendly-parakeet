#include "all_systems.h"
#include "../components/particle.h"
#include "../components/system_scratch.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include "../util/motion.h"
#include "../util/settings.h"

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
    const auto* settings_ptr = reg.ctx().find<SettingsState>();
    const bool reduce_motion = settings_ptr && settings_ptr->reduce_motion;
    const float vel_scale = motion::particle_velocity_scale(reduce_motion);

    auto vel_view = reg.view<ParticleTag, MotionVelocity>();
    for (auto [entity, vel] : vel_view.each()) {
        if (reduce_motion) {
            vel.value.x *= vel_scale;
            vel.value.y *= vel_scale;
        } else {
            vel.value.y += constants::PARTICLE_GRAVITY * dt;
        }
    }
}
