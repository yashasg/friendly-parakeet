// Settings screen controller - renders raygui layout with dynamic toggle labels
// and dispatches settings mutations and back-navigation to game state.

#include "../../components/game_state.h"
#include "../../util/settings.h"
#include "../../util/settings_persistence.h"
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

} // anonymous namespace

void init_settings_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_settings_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<SettingsController>(reg);
    auto* st = reg.ctx().find<SettingsState>();
    auto& gs = reg.ctx().get<GameState>();

    // Static controls: heading, audio offset label, +/- buttons, back button
    controller.render();

    // Audio offset value display (centre slot between - and + buttons)
    if (st) {
        char offset_buf[16];
        std::snprintf(offset_buf, sizeof(offset_buf), "%+d ms", static_cast<int>(st->audio_offset_ms));
        GuiLabel(offset_rect(controller.state().Anchor01, 252, 560, 216, 77), offset_buf);
    }

    // Dynamic toggle buttons with live labels.
    // Issue #390: encode ON/OFF state with both an icon prefix and a colored
    // border/background so color-blind players can read state without relying
    // on the trailing text alone.
    auto draw_toggle = [](Rectangle bounds, const char* name, bool on) -> bool {
        constexpr int kProps[] = {
            BORDER_COLOR_NORMAL, BASE_COLOR_NORMAL, TEXT_COLOR_NORMAL,
            BORDER_COLOR_FOCUSED, BASE_COLOR_FOCUSED, TEXT_COLOR_FOCUSED,
        };
        int saved[sizeof(kProps) / sizeof(kProps[0])] = {0};
        for (int i = 0; i < (int)(sizeof(kProps) / sizeof(kProps[0])); ++i) {
            saved[i] = GuiGetStyle(BUTTON, kProps[i]);
        }

        // ON  → green border + dim green fill
        // OFF → grey border + dark fill
        const Color on_border  = {  64, 200, 110, 255 };
        const Color on_base    = {  20,  64,  36, 255 };
        const Color off_border = { 130, 130, 130, 255 };
        const Color off_base   = {  36,  36,  36, 255 };
        const Color text_color = { 230, 230, 230, 255 };

        const Color border = on ? on_border : off_border;
        const Color base   = on ? on_base   : off_base;
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL,  ColorToInt(border));
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToInt(border));
        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,    ColorToInt(base));
        GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED,   ColorToInt(base));
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,    ColorToInt(text_color));
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,   ColorToInt(text_color));

        char label[32];
        std::snprintf(label, sizeof(label), "[%s] %s: %s",
                      on ? "X" : " ", name, on ? "ON" : "OFF");
        bool pressed = GuiButton(bounds, label);

        for (int i = 0; i < (int)(sizeof(kProps) / sizeof(kProps[0])); ++i) {
            GuiSetStyle(BUTTON, kProps[i], saved[i]);
        }
        return pressed;
    };

    {
        bool haptics_on = !st || st->haptics_enabled;
        controller.state().HapticsTogglePressed = draw_toggle(
            offset_rect(controller.state().Anchor01, 152, 720, 416, 100),
            "HAPTICS", haptics_on);
    }
    {
        // Reduce-motion semantics: the visible toggle reads "MOTION ON" when
        // motion is NOT reduced. We therefore invert reduce_motion for display.
        bool motion_on = !st || !st->reduce_motion;
        controller.state().ReduceMotionTogglePressed = draw_toggle(
            offset_rect(controller.state().Anchor01, 152, 880, 416, 100),
            "MOTION", motion_on);
    }

    // Dispatch actions
    const bool close_pressed = controller.state().CloseButtonPressed;
    if (!st && close_pressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
        return;
    }
    if (!st) return;

    bool settings_changed = false;
    if (controller.state().AudioOffsetMinusPressed) {
        const auto before = st->audio_offset_ms;
        st->audio_offset_ms = static_cast<int16_t>(
            std::max(static_cast<int>(SettingsState::MIN_AUDIO_OFFSET_MS),
                     static_cast<int>(st->audio_offset_ms) - SettingsState::AUDIO_OFFSET_STEP_MS));
        settings_changed = settings_changed || (st->audio_offset_ms != before);
    }
    if (controller.state().AudioOffsetPlusPressed) {
        const auto before = st->audio_offset_ms;
        st->audio_offset_ms = static_cast<int16_t>(
            std::min(static_cast<int>(SettingsState::MAX_AUDIO_OFFSET_MS),
                     static_cast<int>(st->audio_offset_ms) + SettingsState::AUDIO_OFFSET_STEP_MS));
        settings_changed = settings_changed || (st->audio_offset_ms != before);
    }
    if (controller.state().HapticsTogglePressed) {
        st->haptics_enabled = !st->haptics_enabled;
        settings_changed = true;
        if (st->haptics_enabled) {
            platform::haptics::warmup(reg);
        }
    }
    if (controller.state().ReduceMotionTogglePressed) {
        st->reduce_motion = !st->reduce_motion;
        settings_changed = true;
    }

    if (settings_changed) {
        if (auto* persistence_state = reg.ctx().find<SettingsPersistence>()) {
            settings::mark_dirty_and_save(*persistence_state, *st);
        }
    }

    if (close_pressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
    }
}
