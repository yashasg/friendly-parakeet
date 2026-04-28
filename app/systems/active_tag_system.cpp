// active_tag_system.cpp
//
// Owns the registry-mutating active-tag synchronization logic (#317).
// Keeps component header input_events.h focused on pure data/tag declarations.
//
// Two entry points:
//   invalidate_active_tag_cache  — mark UIActiveCache stale; call after
//                                  button spawn/destroy or out-of-seam phase mutation.
//   ensure_active_tags_synced    — sync ActiveTag structural tags against current
//                                  GameState.phase; O(1) when phase is unchanged.

#include "all_systems.h"
#include "../components/input_events.h"
#include "../components/game_state.h"

void invalidate_active_tag_cache(entt::registry& reg) {
    reg.ctx().get<UIActiveCache>().valid = false;
}

void ensure_active_tags_synced(entt::registry& reg) {
    auto& cache = reg.ctx().get<UIActiveCache>();
    auto phase = reg.ctx().get<GameState>().phase;
    if (cache.valid && cache.phase == phase) return;

    auto view = reg.view<ActiveInPhase>();
    for (auto [e, aip] : view.each()) {
        const bool should_be_active = phase_active(aip, phase);
        const bool has_tag          = reg.all_of<ActiveTag>(e);
        if (should_be_active && !has_tag) {
            reg.emplace<ActiveTag>(e);
        } else if (!should_be_active && has_tag) {
            reg.remove<ActiveTag>(e);
        }
    }
    cache.phase = phase;
    cache.valid = true;
}
