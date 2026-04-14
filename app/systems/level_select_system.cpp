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

    auto& aq  = reg.ctx().get<ActionQueue>();
    auto& lss = reg.ctx().get<LevelSelectState>();

    // Keyboard-style navigation via Go actions
    for (int i = 0; i < aq.count; ++i) {
        auto& a = aq.actions[i];
        if (a.verb == ActionVerb::Go) {
            if (a.dir == Direction::Up) {
                lss.selected_level = (lss.selected_level - 1 + LevelSelectState::LEVEL_COUNT) % LevelSelectState::LEVEL_COUNT;
                return;
            }
            if (a.dir == Direction::Down) {
                lss.selected_level = (lss.selected_level + 1) % LevelSelectState::LEVEL_COUNT;
                return;
            }
            if (a.dir == Direction::Left) {
                lss.selected_difficulty = (lss.selected_difficulty - 1 + LevelSelectState::DIFFICULTY_COUNT) % LevelSelectState::DIFFICULTY_COUNT;
                return;
            }
            if (a.dir == Direction::Right) {
                lss.selected_difficulty = (lss.selected_difficulty + 1) % LevelSelectState::DIFFICULTY_COUNT;
                return;
            }
        }
        if (a.verb == ActionVerb::Tap && a.button == Button::Confirm) {
            lss.confirmed = true;
            return;
        }
    }

    // Ignore touch/click on the transition frame
    if (gs.phase_timer < 0.05f) return;

    // Positional taps (touch/mouse)
    for (int i = 0; i < aq.count; ++i) {
        auto& a = aq.actions[i];
        if (a.verb != ActionVerb::Tap || a.button != Button::Position) continue;

        float tx = a.x;
        float ty = a.y;

        // Check START button
        float start_x = (constants::SCREEN_W - START_BTN_W) / 2.0f;
        constexpr float PAD = 10.0f;
        if (tx >= start_x - PAD && tx <= start_x + START_BTN_W + PAD &&
            ty >= START_BTN_Y - PAD && ty <= START_BTN_Y + START_BTN_H + PAD) {
            lss.confirmed = true;
            return;
        }

        // Check difficulty button taps for the selected level
        float card_y = CARD_START_Y + static_cast<float>(lss.selected_level) * (CARD_HEIGHT + CARD_GAP);
        float diff_y = card_y + DIFF_BTN_Y_OFF;
        for (int d = 0; d < LevelSelectState::DIFFICULTY_COUNT; ++d) {
            float bx = DIFF_BTN_X0 + static_cast<float>(d) * (DIFF_BTN_W + DIFF_BTN_GAP);
            if (tx >= bx - PAD && tx <= bx + DIFF_BTN_W + PAD &&
                ty >= diff_y - PAD && ty <= diff_y + DIFF_BTN_H + PAD) {
                lss.selected_difficulty = d;
                return;
            }
        }

        // Check level card taps
        for (int c = 0; c < LevelSelectState::LEVEL_COUNT; ++c) {
            float cy = CARD_START_Y + static_cast<float>(c) * (CARD_HEIGHT + CARD_GAP);
            if (tx >= CARD_X && tx <= CARD_X + CARD_W && ty >= cy && ty <= cy + CARD_HEIGHT) {
                lss.selected_level = c;
                return;
            }
        }
    }
}
