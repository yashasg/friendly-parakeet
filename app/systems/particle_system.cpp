#include "all_systems.h"
#include "../components/particle.h"
#include "../components/transform.h"
#include "../components/registry_context.h"
#include "../constants.h"
#include <vector>

namespace {

struct ParticleSystemScratch {
    std::vector<entt::entity> expired;
};

}  // namespace

void particle_system(entt::registry& reg, float dt) {
    auto& scratch = registry_ctx_get_or_emplace<ParticleSystemScratch>(reg);
    auto& expired = scratch.expired;
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
