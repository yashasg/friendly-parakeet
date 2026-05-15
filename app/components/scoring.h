#pragma once

#include <cstdint>

// TerminalResultState relocated to game_state.h (it pairs with GameOverState /
// terminal phase). ScorePopup / PopupDisplay relocated to popup.h (popup-entity
// data, distinct from score-state singletons). Re-included here for source
// back-compat with consumers that previously got them via scoring.h; new code
// should include the canonical headers directly.
#include "game_state.h"
#include "popup.h"

struct ScoreState {
    int32_t score             = 0;
    int32_t displayed_score   = 0;
    int32_t high_score        = 0;
    int32_t chain_count       = 0;
    float   passive_score_remainder = 0.0f;
};
