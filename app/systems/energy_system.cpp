#include "all_systems.h"
#include "../components/game_state.h"
#include "gameplay_intents.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <raymath.h>

namespace {

void request_energy_depleted_game_over(entt::registry& reg) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.transition_pending) return;

    auto* gos = reg.ctx().find<GameOverState>();
    if (gos) {
        gos->cause = DeathCause::EnergyDepleted;
    }
    gs.transition_pending = true;
    gs.next_phase = GamePhase::GameOver;
}

}  // namespace

void energy_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;
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
