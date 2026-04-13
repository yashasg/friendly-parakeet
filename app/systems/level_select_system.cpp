#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../constants.h"

// Layout constants (shared with render_system)
static constexpr float CARD_START_Y = 200.0f;
static constexpr float CARD_HEIGHT  = 200.0f;
static constexpr float CARD_GAP     = 40.0f;
static constexpr float CARD_X       = 60.0f;
static constexpr float CARD_W       = 600.0f;

static constexpr float DIFF_BTN_W   = 160.0f;
static constexpr float DIFF_BTN_H   = 50.0f;
static constexpr float DIFF_BTN_Y_OFF = 120.0f;
static constexpr float DIFF_BTN_X0  = 100.0f;
static constexpr float DIFF_BTN_GAP = 20.0f;

static constexpr float START_BTN_W  = 300.0f;
static constexpr float START_BTN_H  = 60.0f;
static constexpr float START_BTN_Y  = 1050.0f;

void level_select_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::LevelSelect) return;

    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    if (!input.touch_up) return;

    float tx = input.end_x;
    float ty = input.end_y;

    // Check START button
    float start_x = (constants::SCREEN_W - START_BTN_W) / 2.0f;
    if (tx >= start_x && tx <= start_x + START_BTN_W &&
        ty >= START_BTN_Y && ty <= START_BTN_Y + START_BTN_H) {
        lss.confirmed = true;
        return;
    }

    // Check difficulty button taps for the selected level
    float card_y = CARD_START_Y + static_cast<float>(lss.selected_level) * (CARD_HEIGHT + CARD_GAP);
    float diff_y = card_y + DIFF_BTN_Y_OFF;
    for (int d = 0; d < 3; ++d) {
        float bx = DIFF_BTN_X0 + static_cast<float>(d) * (DIFF_BTN_W + DIFF_BTN_GAP);
        if (tx >= bx && tx <= bx + DIFF_BTN_W && ty >= diff_y && ty <= diff_y + DIFF_BTN_H) {
            lss.selected_difficulty = d;
            return;
        }
    }

    // Check level card taps
    for (int i = 0; i < LevelSelectState::LEVEL_COUNT; ++i) {
        float cy = CARD_START_Y + static_cast<float>(i) * (CARD_HEIGHT + CARD_GAP);
        if (tx >= CARD_X && tx <= CARD_X + CARD_W && ty >= cy && ty <= cy + CARD_HEIGHT) {
            lss.selected_level = i;
            return;
        }
    }
}
