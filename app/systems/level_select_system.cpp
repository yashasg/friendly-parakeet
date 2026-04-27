#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/input_events.h"
#include "../components/transform.h"

void level_select_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::LevelSelect) return;

    auto& eq  = reg.ctx().get<EventQueue>();
    auto& lss = reg.ctx().get<LevelSelectState>();

    // Reposition difficulty buttons to track the currently selected level card.
    auto update_diff_buttons = [&]() {
        constexpr float CARD_START_Y   = 200.0f;
        constexpr float CARD_HEIGHT    = 200.0f;
        constexpr float CARD_GAP       =  40.0f;
        constexpr float DIFF_BTN_Y_OFF = 120.0f;
        constexpr float DIFF_BTN_H     =  50.0f;
        float card_y = CARD_START_Y + static_cast<float>(lss.selected_level) * (CARD_HEIGHT + CARD_GAP);
        float diff_y = card_y + DIFF_BTN_Y_OFF + DIFF_BTN_H / 2.0f;
        auto diff_view = reg.view<MenuButtonTag, MenuAction, Position>();
        for (auto [e, ma, pos] : diff_view.each()) {
            if (ma.kind == MenuActionKind::SelectDiff) {
                pos.y = diff_y;
            }
        }
    };

    // Keyboard-style navigation via GoEvents
    for (int i = 0; i < eq.go_count; ++i) {
        auto dir = eq.goes[i].dir;
        if (dir == Direction::Up) {
            lss.selected_level = (lss.selected_level - 1 + LevelSelectState::LEVEL_COUNT) % LevelSelectState::LEVEL_COUNT;
            update_diff_buttons();
            return;
        }
        if (dir == Direction::Down) {
            lss.selected_level = (lss.selected_level + 1) % LevelSelectState::LEVEL_COUNT;
            update_diff_buttons();
            return;
        }
        if (dir == Direction::Left) {
            lss.selected_difficulty = (lss.selected_difficulty - 1 + LevelSelectState::DIFFICULTY_COUNT) % LevelSelectState::DIFFICULTY_COUNT;
            return;
        }
        if (dir == Direction::Right) {
            lss.selected_difficulty = (lss.selected_difficulty + 1) % LevelSelectState::DIFFICULTY_COUNT;
            return;
        }
    }

    // Ignore touch/click on the transition frame
    if (gs.phase_timer < 0.05f) return;

    // Button presses (resolved by hit_test_system)
    for (int i = 0; i < eq.press_count; ++i) {
        auto entity = eq.presses[i].entity;
        if (!reg.valid(entity)) continue;
        if (!reg.all_of<MenuButtonTag, MenuAction>(entity)) continue;
        auto& ma = reg.get<MenuAction>(entity);
        switch (ma.kind) {
            case MenuActionKind::Confirm:
                lss.confirmed = true;
                return;
            case MenuActionKind::SelectLevel:
                lss.selected_level = ma.index;
                update_diff_buttons();
                return;
            case MenuActionKind::SelectDiff:
                lss.selected_difficulty = ma.index;
                return;
            default: break;
        }
    }

    update_diff_buttons();
}
