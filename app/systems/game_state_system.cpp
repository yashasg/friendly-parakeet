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
        }
        return;
    }

    // Title → Playing on any touch
    if (gs.phase == GamePhase::Title && input.touch_up) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }

    // GameOver → Playing on any touch (after brief delay)
    if (gs.phase == GamePhase::GameOver && input.touch_up && gs.phase_timer > 0.4f) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
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

    // SongComplete → replay on any touch (after brief delay)
    if (gs.phase == GamePhase::SongComplete && input.touch_up && gs.phase_timer > 0.5f) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }
}
