// Gameplay HUD adapter - renders the raygui Pause button overlay.
// The dynamic energy bar, shape buttons, and approach rings are rendered
// by draw_hud() in ui_render_system.cpp.

#include "../../components/game_state.h"
#include "adapter_base.h"
#include <entt/entt.hpp>

#include "../vendor/raygui.h"
#include "../generated/gameplay_hud_layout.h"

namespace {

using GameplayAdapter = RGuiAdapter<GameplayHudLayoutState,
                                    &GameplayHudLayout_Init,
                                    &GameplayHudLayout_Render>;
GameplayAdapter gameplay_adapter;

} // anonymous namespace

void gameplay_adapter_init() {
    gameplay_adapter.init();
}

void gameplay_adapter_render(entt::registry& reg) {
    gameplay_adapter.render();

    if (gameplay_adapter.state().PauseButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        if (gs.phase == GamePhase::Playing) {
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Paused;
        }
    }
}
