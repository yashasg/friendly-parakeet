#include "all_systems.h"
#include "../components/particle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include <vector>

namespace {

struct ParticleSystemScratch {
    std::vector<entt::entity> expired;
};

ParticleSystemScratch& scratch_for(entt::registry& reg) {
    if (auto* scratch = reg.ctx().find<ParticleSystemScratch>()) {
        return *scratch;
    }
    return reg.ctx().emplace<ParticleSystemScratch>();
}

}  // namespace

void particle_system(entt::registry& reg, float dt) {
    auto& expired = scratch_for(reg).expired;
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

    auto vel_view = reg.view<ParticleTag, MotionVelocity>();
    for (auto [entity, vel] : vel_view.each()) {
        vel.value.y += constants::PARTICLE_GRAVITY * dt;
    }
}
