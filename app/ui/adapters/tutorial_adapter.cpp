// Tutorial screen adapter - renders raygui layout and dispatches start action.

#include "../../components/game_state.h"
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/tutorial_layout.h"

namespace {

TutorialLayoutState tutorial_layout_state;
bool tutorial_initialized = false;

} // anonymous namespace

void tutorial_adapter_init() {
    if (!tutorial_initialized) {
        tutorial_layout_state = TutorialLayout_Init();
        tutorial_initialized = true;
    }
}

void tutorial_adapter_render(entt::registry& reg) {
    if (!tutorial_initialized) {
        tutorial_adapter_init();
    }

    TutorialLayout_Render(&tutorial_layout_state);

    if (tutorial_layout_state.ContinueButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }
}
