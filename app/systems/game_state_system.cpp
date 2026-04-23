#include "all_systems.h"
#include "play_session.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/scoring.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include "ui_hit.h"

#include <iterator>

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

    // Title → LevelSelect on click/tap
    if (gs.phase == GamePhase::Title) {
        for (int i = 0; i < aq.count; ++i) {
            auto& a = aq.actions[i];
            if (a.verb == ActionVerb::Click) {
                constexpr float EXIT_W = 200.0f;
                constexpr float EXIT_H = 50.0f;
                constexpr float EXIT_Y = 1050.0f;
                const float exit_x = (constants::SCREEN_W - EXIT_W) / 2.0f;
                if (point_in_padded_rect(a.x, a.y, exit_x, EXIT_Y, EXIT_W, EXIT_H)) {
                    #ifndef PLATFORM_WEB
                    reg.ctx().get<InputState>().quit_requested = true;
                    #endif
                } else {
                    gs.transition_pending = true;
                    gs.next_phase = GamePhase::LevelSelect;
                }
                break;
            } else if (a.verb == ActionVerb::Tap && a.button == Button::Confirm) {
                gs.transition_pending = true;
                gs.next_phase = GamePhase::LevelSelect;
                break;
            }
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

    // End screen button detection (shared by GameOver and SongComplete).
    // Three stacked buttons: Restart → LevelSelect → MainMenu.
    if ((gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete)
        && gs.phase_timer > 0.4f) {
        constexpr float BTN_W   = 280.0f;
        constexpr float BTN_H   = 50.0f;
        constexpr float BTN_GAP = 15.0f;
        constexpr float BTN_PAD = 10.0f;
        constexpr float BTN_Y0  = 870.0f;
        constexpr EndScreenChoice CHOICES[] = {
            EndScreenChoice::Restart,
            EndScreenChoice::LevelSelect,
            EndScreenChoice::MainMenu,
        };
        constexpr int N_CHOICES = static_cast<int>(std::size(CHOICES));

        for (int i = 0; i < aq.count; ++i) {
            auto& a = aq.actions[i];
            if (a.verb == ActionVerb::Click) {
                const float btn_x = (constants::SCREEN_W - BTN_W) / 2.0f;
                for (int b = 0; b < N_CHOICES; ++b) {
                    const float by = BTN_Y0 + static_cast<float>(b) * (BTN_H + BTN_GAP);
                    if (point_in_padded_rect(a.x, a.y, btn_x, by, BTN_W, BTN_H, BTN_PAD)) {
                        gs.end_choice = CHOICES[b];
                        break;
                    }
                }
                break;
            } else if (a.verb == ActionVerb::Tap && a.button == Button::Confirm) {
                gs.end_choice = EndScreenChoice::Restart;
                break;
            }
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

    // Paused → resume on any Tap/Click action
    bool pause_resume = false;
    for (int i = 0; i < aq.count; ++i) {
        if (aq.actions[i].verb == ActionVerb::Tap || aq.actions[i].verb == ActionVerb::Click) {
            pause_resume = true;
            break;
        }
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
            auto unscored = reg.view<ObstacleTag>(entt::exclude<ScoredTag>);
            if (unscored.begin() == unscored.end()) {
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
