// Settings screen controller — kept in tree per #1287 OoS-B pattern (legacy
// controllers are deleted in a single sweep after every screen migrates).
// The render path is no longer called from `ui_render_system.cpp`; entity
// dispatch flows through the codegen spawner + `settings_screen_bind_system`
// + `ui_update_system` action handlers (issue #1295). The implementations
// below remain only to preserve the symbol shape until the bulk deletion.

#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/settings_layout.h"

namespace {

using SettingsController = RGuiScreenController<SettingsLayoutState,
                                                 &SettingsLayout_Init,
                                                 &SettingsLayout_RenderStatic>;

} // anonymous namespace

void init_settings_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_settings_screen_ui(entt::registry& reg) {
    // No-op since #1295 — entity-driven render path runs in
    // `ui_render_system::render_ui_entities` plus
    // `settings_screen_bind_system`.
    auto& controller = screen_controller<SettingsController>(reg);
    (void)controller;
}
