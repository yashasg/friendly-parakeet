#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <algorithm>

void energy_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* energy = reg.ctx().find<EnergyState>();
    if (!energy) return;

    auto* song = reg.ctx().find<SongState>();
    if (!song || !song->playing) return;

    // Depletion check
    if (energy->energy <= 0.0f) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::GameOver;
        song->finished = true;
        song->playing  = false;
        return;
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
