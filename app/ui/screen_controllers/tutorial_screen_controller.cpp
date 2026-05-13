// Tutorial screen controller - renders raygui layout and dispatches start action.

#include "../../components/game_state.h"
#include "../../constants.h"
#include "../../systems/web_input_policy.h"
#include "../tutorial_dodge_hint.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/tutorial_layout.h"

namespace {

using TutorialController = RGuiScreenController<TutorialLayoutState,
                                                 &TutorialLayout_Init,
                                                 &TutorialLayout_Render>;

// Returns true when the dodge-lane hint should display swipe copy instead
// of the keyboard copy. Selection happens at runtime so a touch-only Web
// browser sees the correct gesture text (#513) even though the build
// defines PLATFORM_HAS_KEYBOARD for the Web target.
bool tutorial_prefer_touch_hint(const entt::registry& reg) {
#if defined(PLATFORM_WEB)
    return web_input_touch_capable(reg);
#elif defined(PLATFORM_HAS_KEYBOARD)
    (void)reg;
    return false;
#else
    (void)reg;
    return true;
#endif
}

} // anonymous namespace

void init_tutorial_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_tutorial_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<TutorialController>(reg);
    controller.render();

    const bool prefer_touch = tutorial_prefer_touch_hint(reg);
    GuiLabel(tutorial_dodge_hint_bounds(controller.state().Anchor01),
             tutorial_dodge_hint_text(prefer_touch));

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;

    if (controller.state().ContinueButtonPressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }
}
