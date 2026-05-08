#include "input_routing.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../audio/audio_types.h"
#include "../components/haptics.h"
#include "../util/settings.h"

namespace {
bool game_state_handle_end_screen_press(entt::registry& reg, const ButtonPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if ((gs.phase != GamePhase::GameOver && gs.phase != GamePhase::SongComplete) ||
        gs.phase_timer <= 0.4f) {
        return false;
    }

    auto* haptics = reg.ctx().find<HapticQueue>();
    auto* settings = reg.ctx().find<SettingsState>();
    if (haptics && (!settings || settings->haptics_enabled) &&
        haptics->count < HapticQueue::MAX_QUEUED) {
        haptics->queue[haptics->count++] = evt.menu_action == MenuActionKind::Restart
                                            ? HapticEvent::RetryTap
                                            : HapticEvent::UIButtonTap;
    }

    if (evt.menu_action == MenuActionKind::Restart) {
        gs.end_choice = EndScreenChoice::Restart;
    } else if (evt.menu_action == MenuActionKind::GoLevelSelect) {
        gs.end_choice = EndScreenChoice::LevelSelect;
    } else if (evt.menu_action == MenuActionKind::GoMainMenu) {
        gs.end_choice = EndScreenChoice::MainMenu;
    }
    return true;
}
} // namespace

void game_state_handle_go(entt::registry& reg, const GoEvent& /*evt*/) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::Paused) return;
    enter_phase(gs, GamePhase::Playing);
}

void game_state_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    if (evt.kind != ButtonPressKind::Menu) return;  // ignore shape-button events
    auto& gs = reg.ctx().get<GameState>();

    if (gs.phase == GamePhase::Title) {
        auto* haptics = reg.ctx().find<HapticQueue>();
        auto* settings = reg.ctx().find<SettingsState>();
        if (haptics && (!settings || settings->haptics_enabled) &&
            haptics->count < HapticQueue::MAX_QUEUED) {
            haptics->queue[haptics->count++] = HapticEvent::UIButtonTap;
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

    if (game_state_handle_end_screen_press(reg, evt)) {
        return;
    }

    if (gs.phase == GamePhase::Paused) {
        enter_phase(gs, GamePhase::Playing);
        return;
    }
}
