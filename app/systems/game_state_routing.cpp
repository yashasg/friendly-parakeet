#include "input_routing.h"
#include "../components/game_state.h"
#include "input.h"
#include "input_events.h"
#include "audio_events.h"
#include "haptics.h"
#include "../entities/settings.h"
#include "../constants.h"

namespace {
bool game_state_handle_end_screen_press(entt::registry& reg, const MenuPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::GameOver && gs.phase != GamePhase::SongComplete) {
        return false;
    }

    const float input_delay = (gs.phase == GamePhase::SongComplete)
        ? constants::SONG_COMPLETE_INPUT_DELAY
        : constants::GAME_OVER_INPUT_DELAY;
    if (gs.phase_timer <= input_delay) {
        return false;
    }

    const MenuActionKind action =
        (evt.action == MenuActionKind::Confirm) ? MenuActionKind::Restart
                                                : evt.action;

    if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
        disp->enqueue<PlayHapticEvent>({action == MenuActionKind::Restart
                                           ? HapticEvent::RetryTap
                                           : HapticEvent::UIButtonTap});
    }

    if (action == MenuActionKind::Restart) {
        reg.ctx().insert_or_assign(EndChoiceRestart{});
    } else if (action == MenuActionKind::GoLevelSelect) {
        reg.ctx().insert_or_assign(EndChoiceLevelSelect{});
    } else if (action == MenuActionKind::GoMainMenu) {
        reg.ctx().insert_or_assign(EndChoiceMainMenu{});
    }
    return true;
}
} // namespace

void game_state_handle_go(entt::registry& reg, const GoEvent& /*evt*/) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::Paused) return;
    if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;
    // Deferred per #482 — let game_state_system perform the resume swap.
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Playing;
}

void game_state_handle_press_menu(entt::registry& reg, const MenuPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();

    if (gs.phase == GamePhase::Title) {
        if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
            disp->enqueue<PlayHapticEvent>({HapticEvent::UIButtonTap});
        }
        if (evt.action == MenuActionKind::Exit) {
#ifndef PLATFORM_WEB
            reg.ctx().get<InputState>().quit_requested = true;
#endif
        } else if (evt.action == MenuActionKind::Confirm) {
            gs.transition_pending = true;
            gs.next_phase = GamePhase::LevelSelect;
        }
        return;
    }

    if (game_state_handle_end_screen_press(reg, evt)) {
        return;
    }

    if (gs.phase == GamePhase::Tutorial) {
        if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;
        if (evt.action != MenuActionKind::Confirm) return;
        if (auto* settings_state = find_settings_state(reg)) {
            settings::mark_ftue_complete(*settings_state);
            if (auto* persistence = find_settings_persistence(reg)) {
                settings::mark_dirty_and_save(*persistence, *settings_state);
            }
        }
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
        return;
    }

    if (gs.phase == GamePhase::Paused) {
        if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;
        // Deferred per #482 — see game_state_handle_go above.
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
        return;
    }
}
