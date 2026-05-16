#include "all_systems.h"
#include "../components/game_state.h"
#include "game_phase_transition.h"
#include "gameplay_intents.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <raymath.h>

namespace {

void request_energy_depleted_game_over(entt::registry& reg) {
    if (is_phase_transition_pending(reg)) return;

    reg.ctx().insert_or_assign(EnergyDepletedDeath{});
    request_phase_transition<NextPhaseGameOverTag>(reg);
}

}  // namespace

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
    {
        auto& pending = reg.ctx().get<PendingEnergyEffects>();
        // Preserve per-event clamp semantics (misses first, then hits).
        for (const auto& effect : pending.events) {
            apply_clamped_delta(effect.delta);
            if (effect.flash) {
                energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            }
        }
        pending.events.clear();
    }

    if (song && energy->energy <= 0.0f) {
        request_energy_depleted_game_over(reg);
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
