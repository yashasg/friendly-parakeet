// Gameplay HUD adapter - renders the raygui Pause button overlay.
// The dynamic energy bar, shape buttons, and approach rings are rendered
// by draw_hud() in ui_render_system.cpp.

#include "../../components/game_state.h"
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/gameplay_hud_layout.h"

namespace {

GameplayHudLayoutState gameplay_hud_layout_state;
bool gameplay_hud_initialized = false;

} // anonymous namespace

void gameplay_adapter_init() {
    if (!gameplay_hud_initialized) {
        gameplay_hud_layout_state = GameplayHudLayout_Init();
        gameplay_hud_initialized = true;
    }
}

void gameplay_adapter_render(entt::registry& reg) {
    if (!gameplay_hud_initialized) {
        gameplay_adapter_init();
    }

    GameplayHudLayout_Render(&gameplay_hud_layout_state);

    if (gameplay_hud_layout_state.PauseButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        if (gs.phase == GamePhase::Playing) {
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Paused;
        }
    }
}
