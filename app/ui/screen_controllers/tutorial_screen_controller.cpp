// Tutorial screen controller - renders raygui layout and dispatches start action.

#include "../../components/game_state.h"
#include "../../components/input.h"
#include "../../constants.h"
#include "../../entities/settings.h"
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

Rectangle tutorial_continue_button_bounds(Vector2 anchor) {
    return offset_rect(anchor, 180, 1075, 360, 102);
}

bool tutorial_continue_tapped(const entt::registry& reg, Rectangle bounds) {
    const auto* input = reg.ctx().find<InputState>();
    if (!input || !(input->click || input->touch_up)) {
        return false;
    }
    return CheckCollisionPointRec({input->end_x, input->end_y}, bounds);
}

} // anonymous namespace

void init_tutorial_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void tutorial_screen_continue(entt::registry& reg) {
    if (auto* settings_ptr = find_settings_state(reg)) {
        settings::mark_ftue_complete(*settings_ptr);
        if (auto* persistence = find_settings_persistence(reg)) {
            settings::mark_dirty_and_save(*persistence, *settings_ptr);
        }
    }

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Playing;
}

void render_tutorial_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<TutorialController>(reg);
    controller.render();

    const bool prefer_touch = tutorial_prefer_touch_hint(reg);
    GuiLabel(tutorial_dodge_hint_bounds(controller.state().Anchor01),
             tutorial_dodge_hint_text(prefer_touch));

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;
    if (controller.state().ContinueButtonPressed ||
        tutorial_continue_tapped(reg, tutorial_continue_button_bounds(controller.state().Anchor01))) {
        tutorial_screen_continue(reg);
    }
}
