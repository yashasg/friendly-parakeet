#include "all_systems.h"
#include "camera_system.h"
#include "game_state_system.h"
#include "obstacle_despawn_system.h"
#include "particle_system.h"
#include "popup_display_system.h"
#include "scoring_system.h"
#include "gameplay_intents.h"
#include "../components/rendering.h"

#include <algorithm>
#include <cstddef>

namespace {

constexpr std::size_t kMinimumGameplayScratchCapacity = 8;

}  // namespace

void runtime_system_scratch_init(entt::registry& reg) {
    reg.ctx().insert_or_assign(ScoringSystemScratch{});
    reg.ctx().insert_or_assign(ScorePopupRequestQueue{});
    reg.ctx().insert_or_assign(ObstacleDespawnScratch{});
    reg.ctx().insert_or_assign(PopupDisplayScratch{});
    reg.ctx().insert_or_assign(ParticleSystemScratch{});
    reg.ctx().insert_or_assign(MeshChildCleanupScratch{});
    // WasmSmokeLastLane uses row presence as the "lane reported" predicate
    // (Fabian Principle 3 — no sentinel NULL columns). Erase any stale row
    // carried over from a prior session so the next title update rewrites
    // unconditionally.
    reg.ctx().erase<WasmSmokeLastLane>();

    runtime_system_scratch_reserve(reg, 0);
}

void runtime_system_scratch_reserve(entt::registry& reg, std::size_t beat_capacity) {
    const std::size_t capacity = std::max(kMinimumGameplayScratchCapacity, beat_capacity);
    auto& scoring = reg.ctx().get<ScoringSystemScratch>();
    scoring.miss_buf.reserve(capacity);
    scoring.hit_buf.reserve(capacity);

    // PendingEnergyEffects events were a `std::vector<Event>` array column —
    // eradicated by issue #1627 into a per-frame row table. No reserve
    // needed; entt's storage grows as `scoring_system` emplaces rows.
    // Each per-tier popup queue is reserved independently — the total capacity
    // budget is sized so any single tier can absorb a full-frame burst without
    // reallocating. (Per #1202/#1204, ScorePopupRequestQueue is now 5 per-tier
    // vectors instead of one tagged-union vector.)
    auto& popup_queue = reg.ctx().get<ScorePopupRequestQueue>();
    popup_queue.perfect.reserve(capacity);
    popup_queue.good.reserve(capacity);
    popup_queue.ok.reserve(capacity);
    popup_queue.bad.reserve(capacity);
    popup_queue.untimed.reserve(capacity);
    reg.ctx().get<ObstacleDespawnScratch>().to_destroy.reserve(capacity);
    reg.ctx().get<PopupDisplayScratch>().expired.reserve(capacity);
    reg.ctx().get<ParticleSystemScratch>().expired.reserve(capacity);
    reg.ctx().get<MeshChildCleanupScratch>().stale_children.reserve(
        capacity * static_cast<std::size_t>(ObstacleChildren::MAX));
}
