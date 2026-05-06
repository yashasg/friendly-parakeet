#include "../components/game_state.h"
#include "../components/gameplay_intents.h"
#include "../components/registry_context.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <entt/entt.hpp>
#include <algorithm>

void energy_system(entt::registry& reg, float dt) {
    auto* pending = registry_ctx_find<PendingEnergyEffects>(reg);
    const auto* gs = registry_ctx_find<GameState>(reg);
    if (!gs || !is_playing_phase(gs->phase)) {
        if (pending) {
            pending->events.clear();
        }
        return;
    }
    auto* energy = registry_ctx_find<EnergyState>(reg);
    if (!energy) return;

    auto apply_clamped_delta = [&](float delta) {
        energy->energy = std::clamp(energy->energy + delta, 0.0f, constants::ENERGY_MAX);
        if (energy->energy < 1e-6f) energy->energy = 0.0f;
    };

    // Apply deferred gameplay energy effects (single writer boundary).
    // Preserve per-event clamp semantics (legacy scoring order: misses first, then hits).
    if (pending) {
        for (const auto& effect : pending->events) {
            apply_clamped_delta(effect.delta);
            if (effect.flash) {
                energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            }
        }
        pending->events.clear();
    }

    // Smooth display toward actual energy
    float diff = energy->energy - energy->display;
    energy->display += diff * constants::ENERGY_DISPLAY_SPEED * dt;
    energy->display = std::clamp(energy->display, 0.0f, constants::ENERGY_MAX);

    // Tick flash timer
    if (energy->flash_timer > 0.0f) {
        energy->flash_timer -= dt;
        if (energy->flash_timer < 0.0f) energy->flash_timer = 0.0f;
    }
}
