// Settings screen adapter - renders raygui layout with dynamic toggle labels
// and dispatches settings mutations and back-navigation to game state.

#include "../../components/game_state.h"
#include "../../util/settings.h"
#include <algorithm>
#include <cstdio>
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/settings_layout.h"

namespace {

SettingsLayoutState settings_layout_state;
bool settings_initialized = false;

} // anonymous namespace

void settings_adapter_init() {
    if (!settings_initialized) {
        settings_layout_state = SettingsLayout_Init();
        settings_initialized = true;
    }
}

void settings_adapter_render(entt::registry& reg) {
    if (!settings_initialized) {
        settings_adapter_init();
    }

    auto* st = reg.ctx().find<SettingsState>();
    auto& gs = reg.ctx().get<GameState>();

    // Static controls: heading, audio offset label, +/- buttons, back button
    SettingsLayout_RenderStatic(&settings_layout_state);

    // Audio offset value display (centre slot between - and + buttons)
    if (st) {
        char offset_buf[16];
        std::snprintf(offset_buf, sizeof(offset_buf), "%+d ms", static_cast<int>(st->audio_offset_ms));
        GuiLabel((Rectangle){ settings_layout_state.Anchor01.x + 252,
                               settings_layout_state.Anchor01.y + 560, 216, 77 }, offset_buf);
    }

    // Dynamic toggle buttons with live labels
    {
        char haptics_label[24];
        bool haptics_on = !st || st->haptics_enabled;
        std::snprintf(haptics_label, sizeof(haptics_label), "HAPTICS: %s", haptics_on ? "ON" : "OFF");
        settings_layout_state.HapticsTogglePressed =
            GuiButton((Rectangle){ settings_layout_state.Anchor01.x + 152,
                                   settings_layout_state.Anchor01.y + 720, 416, 100 }, haptics_label);
    }
    {
        char motion_label[24];
        bool reduce_on = st && st->reduce_motion;
        std::snprintf(motion_label, sizeof(motion_label), "MOTION: %s", reduce_on ? "OFF" : "ON");
        settings_layout_state.ReduceMotionTogglePressed =
            GuiButton((Rectangle){ settings_layout_state.Anchor01.x + 152,
                                   settings_layout_state.Anchor01.y + 880, 416, 100 }, motion_label);
    }

    // Dispatch actions
    if (st) {
        if (settings_layout_state.AudioOffsetMinusPressed) {
            st->audio_offset_ms = static_cast<int16_t>(
                std::max(static_cast<int>(SettingsState::MIN_AUDIO_OFFSET_MS),
                         static_cast<int>(st->audio_offset_ms) - SettingsState::AUDIO_OFFSET_STEP_MS));
        }
        if (settings_layout_state.AudioOffsetPlusPressed) {
            st->audio_offset_ms = static_cast<int16_t>(
                std::min(static_cast<int>(SettingsState::MAX_AUDIO_OFFSET_MS),
                         static_cast<int>(st->audio_offset_ms) + SettingsState::AUDIO_OFFSET_STEP_MS));
        }
        if (settings_layout_state.HapticsTogglePressed) {
            st->haptics_enabled = !st->haptics_enabled;
        }
        if (settings_layout_state.ReduceMotionTogglePressed) {
            st->reduce_motion = !st->reduce_motion;
        }
    }

    if (settings_layout_state.CloseButtonPressed) {
        // Return to Title (or previous phase if that's safer)
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
    }
}
