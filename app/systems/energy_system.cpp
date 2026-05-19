#include "all_systems.h"
#include "../components/game_state.h"
#include "game_phase_transition.h"
#include "gameplay_intents.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <raymath.h>

void energy_system(entt::registry& reg, float dt) {
    if (!reg.ctx().contains<GamePhasePlayingTag>()) return;
    auto* energy = reg.ctx().find<EnergyState>();
    if (!energy) return;
    auto* song = reg.ctx().find<SongState>();

    auto apply_clamped_delta = [&](float delta) {
        energy->energy = Clamp(energy->energy + delta, 0.0f, constants::ENERGY_MAX);
        if (energy->energy < 1e-6f) energy->energy = 0.0f;
    };

    // Apply deferred gameplay energy effects (single writer boundary).
    // Per-frame row table (issue #1627): one entity per enqueued delta,
    // tagged `PendingEnergyEffectTag` (+ optional `EnergyFlashTag`).
    // EnTT's default forward iteration on `sparse_set` walks the packed
    // array back-to-front (last-emplaced first); the original
    // `vector<Event>` forward-iterated insertion order. Use `rbegin/rend`
    // to traverse the row table in insertion order, preserving the
    // "misses first, then per-tier hits" clamp semantics scoring_system
    // writes the rows in.
    {
        auto pending = reg.view<PendingEnergyEffectTag>();
        for (auto it = pending.rbegin(), last = pending.rend(); it != last; ++it) {
            const auto e = *it;
            apply_clamped_delta(reg.get<EnergyDelta>(e).value);
            if (reg.all_of<EnergyFlashTag>(e)) {
                energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            }
        }
        reg.destroy(pending.begin(), pending.end());
    }

    if (song && energy->energy <= 0.0f && !is_phase_transition_pending(reg)) {
        reg.ctx().insert_or_assign(EnergyDepletedDeath{});
        request_phase_transition<NextPhaseGameOverTag>(reg);
    }

    // Smooth display toward actual energy
    float diff = energy->energy - energy->display;
    energy->display += diff * constants::ENERGY_DISPLAY_SPEED * dt;
    energy->display = Clamp(energy->display, 0.0f, constants::ENERGY_MAX);

    // Tick flash timer
    if (energy->flash_timer > 0.0f) {
        energy->flash_timer -= dt;
        if (energy->flash_timer < 0.0f) energy->flash_timer = 0.0f;
    }
}
