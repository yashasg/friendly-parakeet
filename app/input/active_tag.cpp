// Owns the registry-mutating active-tag synchronization logic (#317).
// Keeps component header input_events.h focused on pure data/tag declarations.
//
// Two entry points:
//   invalidate_active_tag_cache  — mark UIActiveCache stale; call after
//                                  button spawn/destroy or out-of-seam phase mutation.
//   ensure_active_tags_synced    — sync ActiveTag structural tags against current
//                                  GameState.phase; O(1) when phase is unchanged.

#include "input_routing.h"
#include "phase_activation.h"
#include "../components/input_events.h"
#include "../components/game_state.h"
#include <stdexcept>

void invalidate_active_tag_cache(entt::registry& reg) {
    auto* cache = reg.ctx().find<UIActiveCache>();
    if (!cache) {
        throw std::logic_error("invalidate_active_tag_cache requires UIActiveCache in registry context");
    }
    cache->valid = false;
}

void ensure_active_tags_synced(entt::registry& reg) {
    auto* cache = reg.ctx().find<UIActiveCache>();
    if (!cache) {
        throw std::logic_error("ensure_active_tags_synced requires UIActiveCache in registry context");
    }
    auto* game = reg.ctx().find<GameState>();
    if (!game) {
        throw std::logic_error("ensure_active_tags_synced requires GameState in registry context");
    }
    auto phase = game->phase;
    if (cache->valid && cache->phase == phase) return;

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
    cache->phase = phase;
    cache->valid = true;
}
