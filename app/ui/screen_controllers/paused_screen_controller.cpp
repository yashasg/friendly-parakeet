// Paused screen controller - renders raygui layout and dispatches resume/menu actions.

#include "../../components/game_state.h"
#include "../../systems/game_phase_transition.h"
#include "../../systems/input.h"
#include "../../constants.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/paused_layout.h"

namespace {

using PausedController = RGuiScreenController<PausedLayoutState,
                                               &PausedLayout_Init,
                                               &PausedLayout_Render>;

} // anonymous namespace

void init_paused_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_paused_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<PausedController>(reg);
    controller.render();

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;

    if (controller.state().ResumeButtonPressed) {
        // Deferred per #482 — the canonical state-machine swap (game_state_system)
        // routes Paused→Playing as a resume that preserves session state.
        request_phase_transition<NextPhasePlayingTag>(reg);
    }

    if (controller.state().MenuButtonPressed) {
        request_phase_transition<NextPhaseTitleTag>(reg);
    }
}
