#include "all_systems.h"
#include "../session/play_session.h"
#include "../components/game_state.h"
#include "../util/obstacle_counter.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/scoring.h"
#include "../audio/audio_queue.h"
#include "../components/haptics.h"
#include "../util/haptic_queue.h"
#include "../util/settings.h"
#include "../components/rhythm.h"
#include "../components/high_score.h"
#include "../util/high_score_persistence.h"
#include "../constants.h"

static void enter_game_over(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    bool is_new_high_score = (score.score > score.high_score);
    if (is_new_high_score) {
        score.high_score = score.score;
        if (auto* hs = reg.ctx().find<HighScoreState>()) {
            high_score::update_if_higher(*hs, score.score);
            if (auto* hp = reg.ctx().find<HighScorePersistence>()) {
                hp->dirty = true;
                if (hp->path.empty()) {
                    hp->last_save = persistence::Result{persistence::Status::PathUnavailable, {}};
                } else {
                    hp->last_save = high_score::save_high_scores(*hs, hp->path);
                    if (hp->last_save.ok()) hp->dirty = false;
                }
            }
        }
    }
    audio_push(reg.ctx().get<AudioQueue>(), SFX::Crash);

    {
        auto* hq = reg.ctx().find<HapticQueue>();
        auto* st = reg.ctx().find<SettingsState>();
        if (hq) {
            bool haptics_on = !st || st->haptics_enabled;
            haptic_push(*hq, haptics_on, HapticEvent::DeathCrash);
            if (is_new_high_score) haptic_push(*hq, haptics_on, HapticEvent::NewHighScore);
        }
    }

    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;

    if (auto* song = reg.ctx().find<SongState>()) {
        song->finished = true;
        song->playing = false;
    }
}

static void enter_song_complete(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    bool is_new_high_score = (score.score > score.high_score);
    if (is_new_high_score) {
        score.high_score = score.score;
        if (auto* hs = reg.ctx().find<HighScoreState>()) {
            high_score::update_if_higher(*hs, score.score);
            if (auto* hp = reg.ctx().find<HighScorePersistence>()) {
                hp->dirty = true;
                if (hp->path.empty()) {
                    hp->last_save = persistence::Result{persistence::Status::PathUnavailable, {}};
                } else {
                    hp->last_save = high_score::save_high_scores(*hs, hp->path);
                    if (hp->last_save.ok()) hp->dirty = false;
                }
            }
        }
    }

    {
        auto* hq = reg.ctx().find<HapticQueue>();
        auto* st = reg.ctx().find<SettingsState>();
        if (hq && is_new_high_score) {
            haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::NewHighScore);
        }
    }

    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::SongComplete;
    gs.phase_timer = 0.0f;
}

void game_state_system(entt::registry& reg, float dt) {
    auto& gs = reg.ctx().get<GameState>();

    gs.phase_timer += dt;

    // ── Primary event drain ───────────────────────────────────────────────────
    // game_state_system runs first in tick_fixed_systems (game_loop.cpp) and
    // owns the authoritative drain for GoEvent and ButtonPressEvent.
    // Calling update<T>() here fires every registered listener in registration
    // order: game_state → level_select → player_input
    // (see wire_input_dispatcher in input/input_dispatcher.cpp).
    //
    // Events are enqueued earlier in the same frame by input_system (keyboard),
    // gesture_routing (swipes), and hit_test (taps) — all of
    // which run as direct pre-tick calls before the fixed-step loop.
    //
    // ⚠ Do NOT call disp.clear<GoEvent/ButtonPressEvent>() before this point
    //   within a frame.  Those pre-tick systems enqueue same-frame events that
    //   must reach player_input_handle_go / player_input_handle_press through
    //   this update() call.  clear() would silently drop them before delivery.
    //
    // player_input_system also calls update<GoEvent/ButtonPressEvent>() later
    // in the same tick, but the queue is already empty after this drain, so
    // those calls are no-ops — preserving the #213 no-replay invariant.
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<GoEvent>();
    disp.update<ButtonPressEvent>();

    if (gs.transition_pending) {
        gs.transition_pending = false;
        switch (gs.next_phase) {
            case GamePhase::Playing:      setup_play_session(reg);  break;
            case GamePhase::GameOver:
                enter_game_over(reg);
                break;
            case GamePhase::SongComplete:
                enter_song_complete(reg);
                break;
            case GamePhase::Paused:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Paused;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::Title:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Title;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::LevelSelect:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::LevelSelect;
                gs.phase_timer = 0.0f;
                {
                    auto& lss = reg.ctx().get<LevelSelectState>();
                    lss.confirmed = false;
                }
                break;
            case GamePhase::Settings:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Settings;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::Tutorial:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Tutorial;
                gs.phase_timer = 0.0f;
                break;
        }
        return;
    }

    // LevelSelect input handling
    if (gs.phase == GamePhase::LevelSelect && gs.phase_timer > 0.2f) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        if (lss.confirmed) {
            lss.confirmed = false;
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Playing;
        }
    }

    // GameOver → button choice (after brief delay)
    if (gs.phase == GamePhase::GameOver && gs.phase_timer > 0.4f && gs.end_choice != EndScreenChoice::None) {
        gs.transition_pending = true;
        if (gs.end_choice == EndScreenChoice::Restart)
            gs.next_phase = GamePhase::Playing;
        else if (gs.end_choice == EndScreenChoice::LevelSelect)
            gs.next_phase = GamePhase::LevelSelect;
        else
            gs.next_phase = GamePhase::Title;
        gs.end_choice = EndScreenChoice::None;
    }

    // Playing → SongComplete when song finishes (all obstacles cleared)
    if (gs.phase == GamePhase::Playing) {
        auto* energy = reg.ctx().find<EnergyState>();
        auto* song = reg.ctx().find<SongState>();
        if (energy && song && song->playing && energy->energy <= 0.0f) {
            if (auto* gos = reg.ctx().find<GameOverState>()) {
                if (gos->cause == DeathCause::None) {
                    gos->cause = DeathCause::EnergyDepleted;
                }
            }
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            return;
        }

        if (song && song->finished) {
            // Wait until all obstacle entities have been destroyed (O(1) counter).
            auto* oc = reg.ctx().find<ObstacleCounter>();
            if (!oc || oc->count == 0) {
                gs.transition_pending = true;
                gs.next_phase = GamePhase::SongComplete;
            }
        }
    }

    // SongComplete → button choice (after brief delay)
    if (gs.phase == GamePhase::SongComplete && gs.phase_timer > 0.5f && gs.end_choice != EndScreenChoice::None) {
        gs.transition_pending = true;
        if (gs.end_choice == EndScreenChoice::Restart)
            gs.next_phase = GamePhase::Playing;
        else if (gs.end_choice == EndScreenChoice::LevelSelect)
            gs.next_phase = GamePhase::LevelSelect;
        else
            gs.next_phase = GamePhase::Title;
        gs.end_choice = EndScreenChoice::None;
    }
}
