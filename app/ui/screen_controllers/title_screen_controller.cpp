// Title screen controller - bridges generated rguilayout output to game systems.

#include "../../components/game_state.h"
#include "../../systems/input.h"
#include "screen_controller_base.h"
#include <raygui.h>
#include "title_screen_dead_zones.h"
#include <entt/entt.hpp>

#include "../generated/title_layout.h"

namespace {

using TitleController = RGuiScreenController<TitleLayoutState,
                                              &TitleLayout_Init,
                                              &TitleLayout_Render>;

bool read_title_pointer_release(const entt::registry& reg, Vector2& pointer) {
    const auto& input = reg.ctx().get<InputState>();
    if (!(input.click || input.touch_up)) return false;
    pointer = {input.end_x, input.end_y};
    return true;
}

bool is_start_tap(const entt::registry& reg, const TitleLayoutState& state) {
    Vector2 pointer = {};
    if (!read_title_pointer_release(reg, pointer)) return false;
    // Dead-zones are derived from the same accessors the layout uses to
    // render GuiButton (see title_screen_dead_zones.h). The EXIT region is
    // excluded on EVERY platform — on Web the EXIT button is hidden (#511),
    // but we still treat its bounds as a dead-zone so a cached/off-by-one
    // tap there cannot silently transition to LevelSelect.
    return !title_tap_in_dead_zone(pointer, state);
}

} // anonymous namespace

void init_title_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_title_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<TitleController>(reg);
    auto& state = controller.state();
    const int saved_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int saved_label_alignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 28);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    controller.render();
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, saved_label_alignment);
    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);

#ifndef PLATFORM_WEB
    if (state.ExitButtonPressed) {
        reg.ctx().get<InputState>().quit_requested = true;
    }
#endif

    if (state.SettingsButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Settings;
    }

    if (is_start_tap(reg, state)) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::LevelSelect;
    }
}
