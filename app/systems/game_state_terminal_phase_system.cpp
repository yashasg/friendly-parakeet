#include "all_systems.h"
#include "../audio/audio_types.h"
#include "../components/haptics.h"
#include "../components/high_score.h"
#include "../components/rhythm.h"
#include "../components/scoring.h"
#include "../util/high_score_persistence.h"
#include "../util/settings.h"

namespace {

bool update_and_persist_high_score(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    const bool is_new_high_score = (score.score > score.high_score);
    if (!is_new_high_score) return false;

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
    return true;
}

void emit_terminal_feedback(entt::registry& reg, GamePhase phase, bool is_new_high_score) {
    if (phase == GamePhase::GameOver) {
        auto& audio = reg.ctx().get<AudioQueue>();
        if (audio.count < AudioQueue::MAX_QUEUED) {
            audio.queue[audio.count++] = SFX::Crash;
        }
    }

    auto* haptics = reg.ctx().find<HapticQueue>();
    auto* settings = reg.ctx().find<SettingsState>();
    if (!haptics || (settings && !settings->haptics_enabled)) {
        return;
    }
    if (phase == GamePhase::GameOver && haptics->count < HapticQueue::MAX_QUEUED) {
        haptics->queue[haptics->count++] = HapticEvent::DeathCrash;
    }
    if (is_new_high_score && haptics->count < HapticQueue::MAX_QUEUED) {
        haptics->queue[haptics->count++] = HapticEvent::NewHighScore;
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
