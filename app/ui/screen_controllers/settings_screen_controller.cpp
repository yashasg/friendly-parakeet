// Settings screen controller - renders raygui layout with dynamic toggle labels
// and dispatches settings mutations and back-navigation to game state.

#include "../../components/game_state.h"
#include "../../util/settings.h"
#include "../../platform/haptics_backend.h"
#include "screen_controller_base.h"
#include <algorithm>
#include <cstdio>
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/settings_layout.h"

namespace {

using SettingsController = RGuiScreenController<SettingsLayoutState,
                                                 &SettingsLayout_Init,
                                                 &SettingsLayout_RenderStatic>;
SettingsController settings_controller;

} // anonymous namespace

void init_settings_screen_ui() {
    settings_controller.init();
}

void render_settings_screen_ui(entt::registry& reg) {
    auto* st = reg.ctx().find<SettingsState>();
    auto& gs = reg.ctx().get<GameState>();

    // Static controls: heading, audio offset label, +/- buttons, back button
    settings_controller.render();

    // Audio offset value display (centre slot between - and + buttons)
    if (st) {
        char offset_buf[16];
        std::snprintf(offset_buf, sizeof(offset_buf), "%+d ms", static_cast<int>(st->audio_offset_ms));
        GuiLabel(offset_rect(settings_controller.state().Anchor01, 252, 560, 216, 77), offset_buf);
    }

    // Dynamic toggle buttons with live labels
    {
        char haptics_label[24];
        bool haptics_on = !st || st->haptics_enabled;
        std::snprintf(haptics_label, sizeof(haptics_label), "HAPTICS: %s", haptics_on ? "ON" : "OFF");
        settings_controller.state().HapticsTogglePressed =
            GuiButton(offset_rect(settings_controller.state().Anchor01, 152, 720, 416, 100), haptics_label);
    }
    {
        char motion_label[24];
        bool reduce_on = st && st->reduce_motion;
        std::snprintf(motion_label, sizeof(motion_label), "MOTION: %s", reduce_on ? "OFF" : "ON");
        settings_controller.state().ReduceMotionTogglePressed =
            GuiButton(offset_rect(settings_controller.state().Anchor01, 152, 880, 416, 100), motion_label);
    }

    // Dispatch actions
    if (st) {
        if (settings_controller.state().AudioOffsetMinusPressed) {
            st->audio_offset_ms = static_cast<int16_t>(
                std::max(static_cast<int>(SettingsState::MIN_AUDIO_OFFSET_MS),
                         static_cast<int>(st->audio_offset_ms) - SettingsState::AUDIO_OFFSET_STEP_MS));
        }
        if (settings_controller.state().AudioOffsetPlusPressed) {
            st->audio_offset_ms = static_cast<int16_t>(
                std::min(static_cast<int>(SettingsState::MAX_AUDIO_OFFSET_MS),
                         static_cast<int>(st->audio_offset_ms) + SettingsState::AUDIO_OFFSET_STEP_MS));
        }
        if (settings_controller.state().HapticsTogglePressed) {
            st->haptics_enabled = !st->haptics_enabled;
            if (st->haptics_enabled) {
                platform::haptics::warmup();
            }
        }
        if (settings_controller.state().ReduceMotionTogglePressed) {
            st->reduce_motion = !st->reduce_motion;
        }
    }

    if (settings_controller.state().CloseButtonPressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
    }
}
