#include "../session/play_session.h"
#include "../components/audio.h"
#include "../components/game_state.h"
#include "../components/haptics.h"
#include "../components/high_score.h"
#include "../components/input.h"
#include "../util/obstacle_counter.h"
#include "../components/song_state.h"
#include "../components/scoring.h"
#include "../components/registry_context.h"
#include "input_routing.h"
#include "../util/haptic_queue.h"
#include "../util/high_score_persistence.h"
#include <entt/entt.hpp>

namespace {

void maybe_queue_end_screen_transition(GameState& gs) {
    const bool game_over_ready =
        gs.phase == GamePhase::GameOver &&
        gs.phase_timer > game_phase_timing::END_SCREEN_INPUT_DELAY_SEC;
    const bool song_complete_ready =
        gs.phase == GamePhase::SongComplete &&
        gs.phase_timer > game_phase_timing::SONG_COMPLETE_TRANSITION_DELAY_SEC;
    if (!(game_over_ready || song_complete_ready) ||
        gs.end_choice == EndScreenChoice::None) {
        return;
    }

    switch (gs.end_choice) {
        case EndScreenChoice::Restart:
            request_phase_transition(gs, GamePhase::Playing);
            break;
        case EndScreenChoice::LevelSelect:
            request_phase_transition(gs, GamePhase::LevelSelect);
            break;
        case EndScreenChoice::MainMenu:
        case EndScreenChoice::None:
            request_phase_transition(gs, GamePhase::Title);
            break;
    }
    gs.end_choice = EndScreenChoice::None;
}

bool update_and_persist_high_score(entt::registry& reg) {
    auto* score = registry_ctx_find<ScoreState>(reg);
    if (!score) return false;
    const bool is_new_high_score = (score->score > score->high_score);
    if (!is_new_high_score) return false;

    score->high_score = score->score;
    if (auto* hs = registry_ctx_find<HighScoreState>(reg)) {
        high_score::update_if_higher(*hs, score->score);
        if (auto* hp = registry_ctx_find<HighScorePersistence>(reg)) {
            high_score::mark_dirty_and_save(*hp, *hs);
        }
    }
    return true;
}

void emit_terminal_feedback(entt::registry& reg, GamePhase phase, bool is_new_high_score) {
    if (phase == GamePhase::GameOver) {
        registry_ctx_get<AudioQueue>(reg).push(SFX::Crash);
        haptic_feedback(reg, HapticEvent::DeathCrash);
    }
    if (is_new_high_score) {
        haptic_feedback(reg, HapticEvent::NewHighScore);
    }
}

void game_state_enter_terminal_phase(entt::registry& reg, GameState& gs, GamePhase phase) {
    const bool is_new_high_score = update_and_persist_high_score(reg);
    emit_terminal_feedback(reg, phase, is_new_high_score);
    enter_phase(gs, phase);

    if (phase != GamePhase::GameOver) return;

    if (auto* song = registry_ctx_find<SongState>(reg)) {
        song->finished = true;
        song->playing = false;
    }
}

void apply_non_session_phase_transition(entt::registry& reg, GameState& gs, GamePhase phase) {
    enter_phase(gs, phase);
    if (phase != GamePhase::LevelSelect) return;

    auto& level_select = registry_ctx_get<LevelSelectState>(reg);
    level_select.confirmed = false;
}

}  // namespace

void game_state_system(entt::registry& reg, float dt) {
    auto& gs = registry_ctx_get<GameState>(reg);

    gs.phase_timer += dt;

    // ── Primary event drain ───────────────────────────────────────────────────
    // game_state_system runs first in tick_fixed_systems (game_loop.cpp) and
    // owns the authoritative drain for GoEvent and ButtonPressEvent.
    // Calling update<T>() here fires every registered listener in registration
    // order: game_state → level_select → player_input
    // (see wire_input_dispatcher in input/input_dispatcher.cpp).
    //
    // Events are enqueued earlier in the same frame by input_system (keyboard)
    // and swipe routing (InputEvent -> GoEvent) before the fixed-step loop.
    //
    // ⚠ Do NOT call disp.clear<GoEvent/ButtonPressEvent>() before this point
    //   within a frame.  Those pre-tick systems enqueue same-frame events that
    //   must reach player_input_handle_go / player_input_handle_press through
    //   this update() call.  clear() would silently drop them before delivery.
    //
    // player_input_system also calls update<GoEvent/ButtonPressEvent>() later
    // in the same tick, but the queue is already empty after this drain, so
    // those calls are no-ops — preserving the #213 no-replay invariant.
    drain_semantic_input_events(reg);

    if (gs.transition_pending) {
        gs.transition_pending = false;

        // Clear any in-flight pointer capture when changing screens so
        // down/up state from the previous phase cannot leak into the next UI.
        if (auto* input = registry_ctx_find<InputState>(reg)) {
            reset_pointer_capture(*input);
        }

        switch (gs.next_phase) {
            case GamePhase::Playing:      setup_play_session(reg);  break;
            case GamePhase::GameOver:
                game_state_enter_terminal_phase(reg, gs, GamePhase::GameOver);
                break;
            case GamePhase::SongComplete:
                game_state_enter_terminal_phase(reg, gs, GamePhase::SongComplete);
                break;
            case GamePhase::Paused:
            case GamePhase::Title:
            case GamePhase::LevelSelect:
            case GamePhase::Settings:
            case GamePhase::Tutorial:
                apply_non_session_phase_transition(reg, gs, gs.next_phase);
                break;
        }
        return;
    }

    // LevelSelect input handling
    if (gs.phase == GamePhase::LevelSelect &&
        gs.phase_timer > game_phase_timing::LEVEL_SELECT_CONFIRM_DELAY_SEC) {
        auto& lss = registry_ctx_get<LevelSelectState>(reg);
        if (lss.confirmed) {
            lss.confirmed = false;
            request_phase_transition(gs, GamePhase::Playing);
        }
    }

    // Playing → SongComplete when song finishes (all obstacles cleared)
    if (gs.phase == GamePhase::Playing) {
        auto* energy = registry_ctx_find<EnergyState>(reg);
        auto* song = registry_ctx_find<SongState>(reg);
        if (energy && song && song->playing && energy->energy <= 0.0f) {
            auto* gos = registry_ctx_find<GameOverState>(reg);
            if (gos && gos->cause == DeathCause::None) {
                gos->cause = DeathCause::EnergyDepleted;
            }
            request_phase_transition(gs, GamePhase::GameOver);
            return;
        }

        if (song && song->finished) {
            // Wait until all obstacle entities have been destroyed (O(1) counter).
            auto* oc = registry_ctx_find<ObstacleCounter>(reg);
            if (!oc || oc->count == 0) {
                request_phase_transition(gs, GamePhase::SongComplete);
            }
        }
    }

    maybe_queue_end_screen_transition(gs);
}
