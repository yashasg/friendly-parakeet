#include "all_systems.h"
#include "play_session.h"
#include "ui_button_spawner.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/scoring.h"
#include "../components/audio.h"
#include "../components/haptics.h"
#include "../components/settings.h"
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
            hs->update_current_high_score(score.score);
            if (auto* hp = reg.ctx().find<HighScorePersistence>()) {
                if (!hp->path.empty()) high_score::save_high_scores(*hs, hp->path);
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
}

static void enter_song_complete(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    bool is_new_high_score = (score.score > score.high_score);
    if (is_new_high_score) {
        score.high_score = score.score;
        if (auto* hs = reg.ctx().find<HighScoreState>()) {
            hs->update_current_high_score(score.score);
            if (auto* hp = reg.ctx().find<HighScorePersistence>()) {
                if (!hp->path.empty()) high_score::save_high_scores(*hs, hp->path);
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
    auto& gs    = reg.ctx().get<GameState>();
    auto& eq    = reg.ctx().get<EventQueue>();

    gs.phase_timer += dt;

    if (gs.transition_pending) {
        gs.transition_pending = false;
        switch (gs.next_phase) {
            case GamePhase::Playing:      setup_play_session(reg);  break;
            case GamePhase::GameOver:
                enter_game_over(reg);
                spawn_end_screen_buttons(reg);
                break;
            case GamePhase::SongComplete:
                enter_song_complete(reg);
                spawn_end_screen_buttons(reg);
                break;
            case GamePhase::Paused:
                spawn_pause_button(reg);
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Paused;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::Title:
                destroy_ui_buttons(reg);
                spawn_title_buttons(reg);
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Title;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::LevelSelect:
                destroy_ui_buttons(reg);
                spawn_level_select_buttons(reg);
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

    // Title → LevelSelect on button press
    if (gs.phase == GamePhase::Title) {
        for (int i = 0; i < eq.press_count; ++i) {
            auto entity = eq.presses[i].entity;
            if (!reg.valid(entity)) continue;
            if (!reg.all_of<MenuButtonTag, MenuAction>(entity)) continue;
            auto& ma = reg.get<MenuAction>(entity);
            {
                auto* hq = reg.ctx().find<HapticQueue>();
                auto* st = reg.ctx().find<SettingsState>();
                if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::UIButtonTap);
            }
            if (ma.kind == MenuActionKind::Exit) {
                #ifndef PLATFORM_WEB
                reg.ctx().get<InputState>().quit_requested = true;
                #endif
            } else if (ma.kind == MenuActionKind::Confirm) {
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
        for (int i = 0; i < eq.press_count; ++i) {
            auto entity = eq.presses[i].entity;
            if (!reg.valid(entity)) continue;
            if (!reg.all_of<MenuButtonTag, MenuAction>(entity)) continue;
            auto& ma = reg.get<MenuAction>(entity);
            {
                auto* hq = reg.ctx().find<HapticQueue>();
                auto* st = reg.ctx().find<SettingsState>();
                if (hq) {
                    bool haptics_on = !st || st->haptics_enabled;
                    // RetryTap is distinct (crisp) per spec; other buttons use generic UIButtonTap
                    if (ma.kind == MenuActionKind::Restart)
                        haptic_push(*hq, haptics_on, HapticEvent::RetryTap);
                    else
                        haptic_push(*hq, haptics_on, HapticEvent::UIButtonTap);
                }
            }
            if (ma.kind == MenuActionKind::Restart)
                gs.end_choice = EndScreenChoice::Restart;
            else if (ma.kind == MenuActionKind::GoLevelSelect)
                gs.end_choice = EndScreenChoice::LevelSelect;
            else if (ma.kind == MenuActionKind::GoMainMenu)
                gs.end_choice = EndScreenChoice::MainMenu;
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

    // Paused → resume on any press or go event
    bool pause_resume = eq.press_count > 0 || eq.go_count > 0;
    if (gs.phase == GamePhase::Paused && pause_resume) {
        // Only destroy menu buttons (the pause overlay); preserve shape buttons
        auto mv = reg.view<MenuButtonTag>();
        reg.destroy(mv.begin(), mv.end());
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
