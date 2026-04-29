// Tutorial screen adapter - renders raygui layout and dispatches start action.

#include "../../components/game_state.h"
#include "adapter_base.h"
#include <entt/entt.hpp>

#include "../vendor/raygui.h"
#include "../generated/tutorial_layout.h"

namespace {

using TutorialAdapter = RGuiAdapter<TutorialLayoutState,
                                    &TutorialLayout_Init,
                                    &TutorialLayout_Render>;
TutorialAdapter tutorial_adapter;

} // anonymous namespace

void tutorial_adapter_init() {
    tutorial_adapter.init();
}

void tutorial_adapter_render(entt::registry& reg) {
    tutorial_adapter.render();

    if (tutorial_adapter.state().ContinueButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }
}
