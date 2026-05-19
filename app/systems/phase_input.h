#pragma once

#include "../components/game_state.h"
#include "../constants.h"

#include <entt/entt.hpp>

[[nodiscard]] inline bool phase_input_unlocked(const GameState& state,
                                               const float debounce_sec = constants::UI_ENTRY_DEBOUNCE) {
    return state.phase_timer > debounce_sec;
}

[[nodiscard]] inline bool phase_input_unlocked(entt::registry& reg,
                                               const float debounce_sec = constants::UI_ENTRY_DEBOUNCE) {
    return phase_input_unlocked(reg.ctx().get<GameState>(), debounce_sec);
}
