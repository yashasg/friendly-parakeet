#include "input_routing.h"
#include "../ui/ui_button_spawner.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../audio/audio_types.h"
#include "../components/haptics.h"
#include "../util/haptic_queue.h"
#include "../util/settings.h"

void game_state_handle_go(entt::registry& reg, const GoEvent& /*evt*/) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::Paused) return;
    auto mv = reg.view<MenuButtonTag>();
    reg.destroy(mv.begin(), mv.end());
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
}

void game_state_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    if (evt.kind != ButtonPressKind::Menu) return;  // ignore shape-button events
    auto& gs = reg.ctx().get<GameState>();

    if (gs.phase == GamePhase::Title) {
        {
            auto* hq = reg.ctx().find<HapticQueue>();
            auto* st = reg.ctx().find<SettingsState>();
            if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::UIButtonTap);
        }
        if (evt.menu_action == MenuActionKind::Exit) {
#ifndef PLATFORM_WEB
            reg.ctx().get<InputState>().quit_requested = true;
#endif
        } else if (evt.menu_action == MenuActionKind::Confirm) {
            gs.transition_pending = true;
            gs.next_phase = GamePhase::LevelSelect;
        }
        return;
    }

    if ((gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete)
        && gs.phase_timer > 0.4f) {
        {
            auto* hq = reg.ctx().find<HapticQueue>();
            auto* st = reg.ctx().find<SettingsState>();
            if (hq) {
                bool haptics_on = !st || st->haptics_enabled;
                if (evt.menu_action == MenuActionKind::Restart)
                    haptic_push(*hq, haptics_on, HapticEvent::RetryTap);
                else
                    haptic_push(*hq, haptics_on, HapticEvent::UIButtonTap);
            }
        }
        if (evt.menu_action == MenuActionKind::Restart)
            gs.end_choice = EndScreenChoice::Restart;
        else if (evt.menu_action == MenuActionKind::GoLevelSelect)
            gs.end_choice = EndScreenChoice::LevelSelect;
        else if (evt.menu_action == MenuActionKind::GoMainMenu)
            gs.end_choice = EndScreenChoice::MainMenu;
        return;
    }

    if (gs.phase == GamePhase::Paused) {
        auto mv = reg.view<MenuButtonTag>();
        reg.destroy(mv.begin(), mv.end());
        gs.previous_phase = gs.phase;
        gs.phase = GamePhase::Playing;
        gs.phase_timer = 0.0f;
        return;
    }
}
