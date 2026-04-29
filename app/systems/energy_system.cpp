#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/gameplay_intents.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <raymath.h>

void energy_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* energy = reg.ctx().find<EnergyState>();
    if (!energy) return;

    auto apply_clamped_delta = [&](float delta) {
        energy->energy = Clamp(energy->energy + delta, 0.0f, constants::ENERGY_MAX);
        if (energy->energy < 1e-6f) energy->energy = 0.0f;
    };

    // Apply deferred gameplay energy effects (single writer boundary).
    if (auto* pending = reg.ctx().find<PendingEnergyEffects>()) {
        // Preserve per-event clamp semantics (legacy scoring order: misses first, then hits).
        for (const auto& effect : pending->events) {
            apply_clamped_delta(effect.delta);
            if (effect.flash) {
                energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            }
        }
        pending->events.clear();

        // Compatibility path for direct tests still setting aggregate fields.
        if (pending->delta != 0.0f) {
            apply_clamped_delta(pending->delta);
            pending->delta = 0.0f;
        }
        if (pending->flash) {
            energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            pending->flash = false;
        }
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
