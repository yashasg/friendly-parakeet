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

} // anonymous namespace

void init_tutorial_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_tutorial_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<TutorialController>(reg);
    controller.render();

    if (controller.state().ContinueButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }
}
