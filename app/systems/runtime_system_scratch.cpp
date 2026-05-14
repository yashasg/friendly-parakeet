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
    reg.ctx().insert_or_assign(ScoringSystemScratch{});
    reg.ctx().insert_or_assign(PendingEnergyEffects{});
    reg.ctx().insert_or_assign(ScorePopupRequestQueue{});
    reg.ctx().insert_or_assign(ObstacleDespawnScratch{});
    reg.ctx().insert_or_assign(PopupDisplayScratch{});
    reg.ctx().insert_or_assign(ParticleSystemScratch{});
    reg.ctx().insert_or_assign(MeshChildCleanupScratch{});
    reg.ctx().insert_or_assign(WasmSmokeLaneMarkerState{});

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
