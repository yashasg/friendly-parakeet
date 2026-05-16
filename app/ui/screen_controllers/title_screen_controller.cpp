// Title screen controller — kept in tree per #1287 OoS-B pattern (legacy
// controllers are deleted in a single sweep after every screen migrates).
// The render path is no longer called from `ui_render_system.cpp`; entity
// dispatch flows through the codegen spawner + `title_start_tap_system`
// (issue #1294). The implementations below remain only to keep
// `RAYGUI_IMPLEMENTATION` rooted in a stable TU and to preserve the symbol
// shape until the bulk deletion.

#include "../../components/game_state.h"
#include "../../systems/game_phase_transition.h"
#include "../../systems/input.h"
#include "screen_controller_base.h"
#include <raygui.h>
#include <entt/entt.hpp>

#include "../generated/title_layout.h"

namespace {

using TitleController = RGuiScreenController<TitleLayoutState,
                                              &TitleLayout_Init,
                                              &TitleLayout_Render>;

} // anonymous namespace

void init_title_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_title_screen_ui(entt::registry& reg) {
    // No-op since #1294 — entity-driven render path runs in
    // `ui_render_system::render_ui_entities` plus `title_start_tap_system`.
    auto& controller = screen_controller<TitleController>(reg);
    (void)controller;
}
