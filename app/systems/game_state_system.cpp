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
    auto& aq    = reg.ctx().get<ActionQueue>();

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

    // Title → LevelSelect on any tap
    if (gs.phase == GamePhase::Title) {
        for (int i = 0; i < aq.count; ++i) {
            auto& a = aq.actions[i];
            if (a.verb != ActionVerb::Tap) continue;

            if (a.button == Button::Position) {
                float tx = a.x;
                float ty = a.y;
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
            } else if (a.button == Button::Confirm) {
                gs.transition_pending = true;
                gs.next_phase = GamePhase::LevelSelect;
            }
            break;
        }
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

    // End screen button detection (shared by GameOver and SongComplete)
    if ((gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete)
        && gs.phase_timer > 0.4f) {
        for (int i = 0; i < aq.count; ++i) {
            auto& a = aq.actions[i];
            if (a.verb != ActionVerb::Tap) continue;
            if (a.button == Button::Position) {
                float tx = a.x;
                float ty = a.y;
                constexpr float BTN_W = 280.0f;
                constexpr float BTN_H = 50.0f;
                constexpr float BTN_GAP = 15.0f;
                constexpr float BTN_PAD = 10.0f;
                float btn_x = (constants::SCREEN_W - BTN_W) / 2.0f;
                float btn_y1 = 870.0f;
                float btn_y2 = btn_y1 + BTN_H + BTN_GAP;
                float btn_y3 = btn_y2 + BTN_H + BTN_GAP;
                if (tx >= btn_x - BTN_PAD && tx <= btn_x + BTN_W + BTN_PAD) {
                    if (ty >= btn_y1 - BTN_PAD && ty <= btn_y1 + BTN_H + BTN_PAD)
                        gs.end_choice = EndScreenChoice::Restart;
                    else if (ty >= btn_y2 - BTN_PAD && ty <= btn_y2 + BTN_H + BTN_PAD)
                        gs.end_choice = EndScreenChoice::LevelSelect;
                    else if (ty >= btn_y3 - BTN_PAD && ty <= btn_y3 + BTN_H + BTN_PAD)
                        gs.end_choice = EndScreenChoice::MainMenu;
                }
            } else if (a.button == Button::Confirm) {
                gs.end_choice = EndScreenChoice::Restart;
            }
            break;
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

    // Paused → resume on any Tap action
    bool pause_resume = false;
    for (int i = 0; i < aq.count; ++i) {
        if (aq.actions[i].verb == ActionVerb::Tap) { pause_resume = true; break; }
    }
    if (gs.phase == GamePhase::Paused && pause_resume) {
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
        if (gs.end_choice == EndScreenChoice::Restart)
            gs.next_phase = GamePhase::Playing;
        else if (gs.end_choice == EndScreenChoice::LevelSelect)
            gs.next_phase = GamePhase::LevelSelect;
        else
            gs.next_phase = GamePhase::Title;
        gs.end_choice = EndScreenChoice::None;
    }
}
