#include "all_systems.h"
#include "../components/gameplay_intents.h"
#include "../components/rendering.h"
#include "../components/system_scratch.h"

#include <algorithm>
#include <cstddef>

namespace {

constexpr std::size_t kMinimumGameplayScratchCapacity = 8;

std::size_t gameplay_capacity(std::size_t beat_capacity) {
    return std::max(kMinimumGameplayScratchCapacity, beat_capacity);
}

}  // namespace

void runtime_system_scratch_init(entt::registry& reg) {
    reg.ctx().erase<ScoringSystemScratch>();
    reg.ctx().erase<PendingEnergyEffects>();
    reg.ctx().erase<ScorePopupRequestQueue>();
    reg.ctx().erase<ObstacleDespawnScratch>();
    reg.ctx().erase<PopupDisplayScratch>();
    reg.ctx().erase<ParticleSystemScratch>();
    reg.ctx().erase<MeshChildCleanupScratch>();

    reg.ctx().emplace<ScoringSystemScratch>();
    reg.ctx().emplace<PendingEnergyEffects>();
    reg.ctx().emplace<ScorePopupRequestQueue>();
    reg.ctx().emplace<ObstacleDespawnScratch>();
    reg.ctx().emplace<PopupDisplayScratch>();
    reg.ctx().emplace<ParticleSystemScratch>();
    reg.ctx().emplace<MeshChildCleanupScratch>();

    runtime_system_scratch_reserve(reg, 0);
}

void runtime_system_scratch_reserve(entt::registry& reg, std::size_t beat_capacity) {
    const std::size_t capacity = gameplay_capacity(beat_capacity);
    auto& scoring = reg.ctx().get<ScoringSystemScratch>();
    scoring.miss_buf.reserve(capacity);
    scoring.hit_buf.reserve(capacity);

    reg.ctx().get<PendingEnergyEffects>().events.reserve(capacity);
    reg.ctx().get<ScorePopupRequestQueue>().requests.reserve(capacity);
    reg.ctx().get<ObstacleDespawnScratch>().to_destroy.reserve(capacity);
    reg.ctx().get<PopupDisplayScratch>().expired.reserve(capacity);
    reg.ctx().get<ParticleSystemScratch>().expired.reserve(capacity);
    reg.ctx().get<MeshChildCleanupScratch>().stale_children.reserve(
        capacity * static_cast<std::size_t>(ObstacleChildren::MAX));
}
