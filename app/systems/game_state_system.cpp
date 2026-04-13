#include "all_systems.h"
#include "play_session.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/scoring.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../constants.h"

static void enter_game_over(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    if (score.score > score.high_score) {
        score.high_score = score.score;
    }
    audio_push(reg.ctx().get<AudioQueue>(), SFX::Crash);

    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;
}

static void enter_song_complete(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    if (score.score > score.high_score) {
        score.high_score = score.score;
    }

    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::SongComplete;
    gs.phase_timer = 0.0f;
}

void game_state_system(entt::registry& reg, float dt) {
    auto& gs    = reg.ctx().get<GameState>();
    auto& input = reg.ctx().get<InputState>();

    gs.phase_timer += dt;

    if (gs.transition_pending) {
        gs.transition_pending = false;
        switch (gs.next_phase) {
            case GamePhase::Playing:      setup_play_session(reg);  break;
            case GamePhase::GameOver:     enter_game_over(reg);     break;
            case GamePhase::SongComplete: enter_song_complete(reg); break;
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
        }
        return;
    }

    // Title → LevelSelect on any touch (except exit button)
    if (gs.phase == GamePhase::Title && input.touch_up) {
        float tx = input.end_x;
        float ty = input.end_y;
        // Exit button at bottom
        constexpr float EXIT_W = 200.0f;
        constexpr float EXIT_H = 50.0f;
        constexpr float EXIT_Y = 1050.0f;
        float exit_x = (constants::SCREEN_W - EXIT_W) / 2.0f;
        if (tx >= exit_x && tx <= exit_x + EXIT_W && ty >= EXIT_Y && ty <= EXIT_Y + EXIT_H) {
            #ifndef PLATFORM_WEB
            input.quit_requested = true;
            #endif
        } else {
            gs.transition_pending = true;
            gs.next_phase = GamePhase::LevelSelect;
        }
    }

    // LevelSelect input handling
    if (gs.phase == GamePhase::LevelSelect && input.touch_up && gs.phase_timer > 0.2f) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        if (lss.confirmed) {
            lss.confirmed = false;
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Playing;
        }
    }

    // End screen button detection (shared by GameOver and SongComplete)
    if ((gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete)
        && input.touch_up && gs.phase_timer > 0.4f) {
        float tx = input.end_x;
        float ty = input.end_y;
        constexpr float BTN_W = 280.0f;
        constexpr float BTN_H = 55.0f;
        constexpr float BTN_GAP = 20.0f;
        constexpr float BTN_PAD = 15.0f;
        float btn_x = (constants::SCREEN_W - BTN_W) / 2.0f;
        float btn_y1 = 900.0f;
        float btn_y2 = btn_y1 + BTN_H + BTN_GAP;
        if (tx >= btn_x - BTN_PAD && tx <= btn_x + BTN_W + BTN_PAD) {
            if (ty >= btn_y1 - BTN_PAD && ty <= btn_y1 + BTN_H + BTN_PAD)
                gs.end_choice = EndScreenChoice::LevelSelect;
            else if (ty >= btn_y2 - BTN_PAD && ty <= btn_y2 + BTN_H + BTN_PAD)
                gs.end_choice = EndScreenChoice::MainMenu;
        }
    }

    // GameOver → button choice (after brief delay)
    if (gs.phase == GamePhase::GameOver && gs.phase_timer > 0.4f && gs.end_choice != EndScreenChoice::None) {
        gs.transition_pending = true;
        gs.next_phase = (gs.end_choice == EndScreenChoice::LevelSelect) ? GamePhase::LevelSelect : GamePhase::Title;
        gs.end_choice = EndScreenChoice::None;
    }

    // Paused → resume on touch
    if (gs.phase == GamePhase::Paused && input.touch_up) {
        gs.previous_phase = gs.phase;
        gs.phase = GamePhase::Playing;
        gs.phase_timer = 0.0f;
    }

    // Playing → SongComplete when song finishes (all obstacles cleared)
    if (gs.phase == GamePhase::Playing) {
        auto* song = reg.ctx().find<SongState>();
        if (song && song->finished) {
            // Wait until all remaining obstacles have scrolled past
            auto obs_count = reg.view<ObstacleTag>(entt::exclude<ScoredTag>).size_hint();
            if (obs_count == 0) {
                gs.transition_pending = true;
                gs.next_phase = GamePhase::SongComplete;
            }
        }
    }

    // SongComplete → button choice (after brief delay)
    if (gs.phase == GamePhase::SongComplete && gs.phase_timer > 0.5f && gs.end_choice != EndScreenChoice::None) {
        gs.transition_pending = true;
        gs.next_phase = (gs.end_choice == EndScreenChoice::LevelSelect) ? GamePhase::LevelSelect : GamePhase::Title;
        gs.end_choice = EndScreenChoice::None;
    }
}
