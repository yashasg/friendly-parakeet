// Song Complete screen adapter - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/song_complete_layout.h"

namespace {

SongCompleteLayoutState song_complete_layout_state;
bool song_complete_initialized = false;

} // anonymous namespace

void song_complete_adapter_init() {
    if (!song_complete_initialized) {
        song_complete_layout_state = SongCompleteLayout_Init();
        song_complete_initialized = true;
    }
}

void song_complete_adapter_render(entt::registry& reg) {
    if (!song_complete_initialized) {
        song_complete_adapter_init();
    }

    SongCompleteLayout_Render(&song_complete_layout_state);

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.5f) return; // match existing delay guard

    if (song_complete_layout_state.RestartButtonPressed)
        gs.end_choice = EndScreenChoice::Restart;
    else if (song_complete_layout_state.LevelSelectButtonPressed)
        gs.end_choice = EndScreenChoice::LevelSelect;
    else if (song_complete_layout_state.MenuButtonPressed)
        gs.end_choice = EndScreenChoice::MainMenu;
}
