#include "all_systems.h"
#include "game_phase_transition.h"
#include "audio_events.h"
#include "haptics.h"
#include "../components/high_score.h"
#include "../components/rhythm.h"
#include "../components/scoring.h"
#include "high_score_system.h"

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
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    const int32_t previous_high_score = current.value;
    const bool score_exceeds_high_score = (score.score > current.value);
    bool recorded_new_high_score = score_exceeds_high_score;

    if (auto* hs = reg.ctx().find<HighScoreState>()) {
        const auto* session = reg.ctx().find<HighScoreSession>();
        const bool has_active_high_score_key = session && (session->key_hash != 0);
        if (score_exceeds_high_score && has_active_high_score_key) {
            recorded_new_high_score = high_score::update_if_higher(*hs, *session, score.score);
        }
        if (score_exceeds_high_score && recorded_new_high_score) {
            current.value = score.score;
        }
        if (auto* hp = reg.ctx().find<HighScorePersistence>()) {
            if (score_exceeds_high_score && recorded_new_high_score && has_active_high_score_key) {
                hp->dirty = true;
            }
            persist_dirty_high_scores(*hs, *hp);
        }
    } else if (score_exceeds_high_score) {
        current.value = score.score;
    }

    if (auto* result = reg.ctx().find<TerminalResultState>()) {
        *result = TerminalResultState{recorded_new_high_score, previous_high_score};
    } else {
        reg.ctx().emplace<TerminalResultState>(TerminalResultState{recorded_new_high_score, previous_high_score});
    }

    return recorded_new_high_score;
}

void emit_terminal_feedback_game_over(entt::registry& reg, bool is_new_high_score) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;

    disp->enqueue<PlaySfxEvent>({SFX::Crash});
    disp->enqueue<PlayHapticEvent>({HapticEvent::DeathCrash});
    if (is_new_high_score) {
        disp->enqueue<PlayHapticEvent>({HapticEvent::NewHighScore});
    }
}

void emit_terminal_feedback_song_complete(entt::registry& reg, bool is_new_high_score) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;

    if (is_new_high_score) {
        disp->enqueue<PlayHapticEvent>({HapticEvent::NewHighScore});
    }
}

}  // namespace

void game_state_enter_terminal_phase_game_over(entt::registry& reg) {
    const bool is_new_high_score = update_and_persist_high_score(reg);
    emit_terminal_feedback_game_over(reg, is_new_high_score);

    enter_phase<GamePhaseGameOverTag>(reg);

    if (auto* song = reg.ctx().find<SongState>()) {
        song->finished = true;
        song->playing = false;
    }
}

void game_state_enter_terminal_phase_song_complete(entt::registry& reg) {
    const bool is_new_high_score = update_and_persist_high_score(reg);
    emit_terminal_feedback_song_complete(reg, is_new_high_score);

    enter_phase<GamePhaseSongCompleteTag>(reg);
}
