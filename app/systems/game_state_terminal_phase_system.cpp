#include "all_systems.h"
#include "game_phase_transition.h"
#include "../components/audio_events.h"
#include "../components/haptics.h"
#include "../components/high_score.h"
#include "../components/rhythm.h"
#include "../components/scoring.h"
#include "../util/high_score_persistence.h"

namespace {

void persist_dirty_high_scores(const HighScoreState& high_scores, HighScorePersistence& persistence) {
    if (!persistence.dirty) return;

    if (persistence.path.empty()) {
        persistence.last_save = persistence::Result{persistence::Status::PathUnavailable, {}};
        return;
    }

    persistence.last_save = high_score::save_high_scores(high_scores, persistence.path);
    if (persistence.last_save.ok()) {
        persistence.dirty = false;
    }
}

bool update_and_persist_high_score(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    const int32_t previous_high_score = score.high_score;
    const bool score_exceeds_high_score = (score.score > score.high_score);
    bool recorded_new_high_score = score_exceeds_high_score;

    if (auto* hs = reg.ctx().find<HighScoreState>()) {
        const bool has_active_high_score_key = (hs->current_key_hash != 0);
        if (score_exceeds_high_score && has_active_high_score_key) {
            recorded_new_high_score = high_score::update_if_higher(*hs, score.score);
        }
        if (score_exceeds_high_score && recorded_new_high_score) {
            score.high_score = score.score;
        }
        if (auto* hp = reg.ctx().find<HighScorePersistence>()) {
            if (score_exceeds_high_score && recorded_new_high_score && has_active_high_score_key) {
                hp->dirty = true;
            }
            persist_dirty_high_scores(*hs, *hp);
        }
    } else if (score_exceeds_high_score) {
        score.high_score = score.score;
    }

    if (auto* result = reg.ctx().find<TerminalResultState>()) {
        *result = TerminalResultState{recorded_new_high_score, previous_high_score};
    } else {
        reg.ctx().emplace<TerminalResultState>(TerminalResultState{recorded_new_high_score, previous_high_score});
    }

    return recorded_new_high_score;
}

void emit_terminal_feedback(entt::registry& reg, GamePhase phase, bool is_new_high_score) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;

    if (phase == GamePhase::GameOver) {
        disp->enqueue<PlaySfxEvent>({SFX::Crash});
        disp->enqueue<PlayHapticEvent>({HapticEvent::DeathCrash});
    }
    if (is_new_high_score) {
        disp->enqueue<PlayHapticEvent>({HapticEvent::NewHighScore});
    }
}

}  // namespace

void game_state_enter_terminal_phase(entt::registry& reg, GamePhase phase) {
    const bool is_new_high_score = update_and_persist_high_score(reg);
    emit_terminal_feedback(reg, phase, is_new_high_score);

    auto& gs = reg.ctx().get<GameState>();
    enter_phase(gs, phase);

    if (phase != GamePhase::GameOver) return;

    if (auto* song = reg.ctx().find<SongState>()) {
        song->finished = true;
        song->playing = false;
    }
}
