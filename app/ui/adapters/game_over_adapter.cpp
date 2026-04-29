// Game Over screen adapter - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/game_over_layout.h"

namespace {

GameOverLayoutState game_over_layout_state;
bool game_over_initialized = false;

} // anonymous namespace

void game_over_adapter_init() {
    if (!game_over_initialized) {
        game_over_layout_state = GameOverLayout_Init();
        game_over_initialized = true;
    }
}

void game_over_adapter_render(entt::registry& reg) {
    if (!game_over_initialized) {
        game_over_adapter_init();
    }

    GameOverLayout_Render(&game_over_layout_state);

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.4f) return; // match existing delay guard

    if (game_over_layout_state.RestartButtonPressed)
        gs.end_choice = EndScreenChoice::Restart;
    else if (game_over_layout_state.LevelSelectButtonPressed)
        gs.end_choice = EndScreenChoice::LevelSelect;
    else if (game_over_layout_state.MenuButtonPressed)
        gs.end_choice = EndScreenChoice::MainMenu;
}
