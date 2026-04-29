// Paused screen controller - renders raygui layout and dispatches resume/menu actions.

#include "../../components/game_state.h"
#include "../../components/input.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/paused_layout.h"

namespace {

using PausedController = RGuiScreenController<PausedLayoutState,
                                               &PausedLayout_Init,
                                               &PausedLayout_Render>;
PausedController paused_controller;

} // anonymous namespace

void init_paused_screen_ui() {
    paused_controller.init();
}

void render_paused_screen_ui(entt::registry& reg) {
    paused_controller.render();

    auto& gs = reg.ctx().get<GameState>();

    if (paused_controller.state().ResumeButtonPressed) {
        gs.previous_phase = gs.phase;
        gs.phase = GamePhase::Playing;
        gs.phase_timer = 0.0f;
    }

    if (paused_controller.state().MenuButtonPressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
    }
}
