// Tutorial screen controller - renders raygui layout and dispatches start action.

#include "../../components/game_state.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/tutorial_layout.h"

namespace {

using TutorialController = RGuiScreenController<TutorialLayoutState,
                                                 &TutorialLayout_Init,
                                                 &TutorialLayout_Render>;
TutorialController tutorial_controller;

} // anonymous namespace

void init_tutorial_screen_ui() {
    tutorial_controller.init();
}

void render_tutorial_screen_ui(entt::registry& reg) {
    tutorial_controller.render();

    if (tutorial_controller.state().ContinueButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }
}
