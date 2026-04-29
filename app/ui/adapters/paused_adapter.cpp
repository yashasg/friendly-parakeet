// Paused screen adapter - renders raygui layout and dispatches resume/menu actions.

#include "../../components/game_state.h"
#include "../../components/input.h"
#include "../../components/input_events.h"
#include "adapter_base.h"
#include <entt/entt.hpp>

#include "../vendor/raygui.h"
#include "../generated/paused_layout.h"

namespace {

using PausedAdapter = RGuiAdapter<PausedLayoutState,
                                  &PausedLayout_Init,
                                  &PausedLayout_Render>;
PausedAdapter paused_adapter;

} // anonymous namespace

void paused_adapter_init() {
    paused_adapter.init();
}

void paused_adapter_render(entt::registry& reg) {
    paused_adapter.render();

    auto& gs = reg.ctx().get<GameState>();

    if (paused_adapter.state().ResumeButtonPressed) {
        auto mv = reg.view<MenuButtonTag>();
        reg.destroy(mv.begin(), mv.end());
        gs.previous_phase = gs.phase;
        gs.phase = GamePhase::Playing;
        gs.phase_timer = 0.0f;
    }

    if (paused_adapter.state().MenuButtonPressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
    }
}
