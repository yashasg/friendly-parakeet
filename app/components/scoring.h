#pragma once

#include <cstdint>

// NewBestRecord (formerly TerminalResultState) relocated to game_state.h
// (it pairs with the EnergyDepletedDeath ctx tag and the rest of the
// terminal-phase singletons). ScorePopup / PopupDisplay relocated to
// popup.h (popup-entity data, distinct from score-state singletons).
// Re-included here for source back-compat with consumers that previously
// got them via scoring.h; new code should include the canonical headers
// directly.
#include "game_state.h"
#include "popup.h"

// Live scoring accumulators owned by scoring_system. All three fields are
// written every frame the system runs:
//   - score / chain_count are advanced on each scored obstacle
//   - passive_score_remainder is the fractional carry from time-based scoring
// Read-only consumers (HUD, end screens, telemetry) take a const reference.
struct ScoreState {
    int32_t score                   = 0;
    int32_t chain_count             = 0;
    float   passive_score_remainder = 0.0f;
};

// HUD-side animated readout that lags toward ScoreState::score for a "ticking
// up" feel. Owned by scoring_system's tween block; read by the gameplay HUD
// controller. Lifecycle: ctx-bound singleton, reset alongside ScoreState.
struct ScoreDisplay {
    int32_t displayed = 0;
};

// Best score for the currently loaded song+difficulty. Snapshotted from the
// durable HighScoreState table at session load (in play_session) and bumped
// at terminal phase if the current run exceeded it. Distinct from
// HighScoreState (which spans every song+difficulty combo and persists to
// disk) — this singleton is the single per-run best the HUD/end-screens read.
struct CurrentSongHighScore {
    int32_t value = 0;
};
