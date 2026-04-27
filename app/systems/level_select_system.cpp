#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/input_events.h"
#include "../components/transform.h"

static void update_diff_buttons_pos(entt::registry& reg, const LevelSelectState& lss) {
    constexpr float CARD_START_Y   = 200.0f;
    constexpr float CARD_HEIGHT    = 200.0f;
    constexpr float CARD_GAP       =  40.0f;
    constexpr float DIFF_BTN_Y_OFF = 120.0f;
    constexpr float DIFF_BTN_H     =  50.0f;
    float card_y = CARD_START_Y + static_cast<float>(lss.selected_level) * (CARD_HEIGHT + CARD_GAP);
    float diff_y = card_y + DIFF_BTN_Y_OFF + DIFF_BTN_H / 2.0f;
    auto view = reg.view<MenuButtonTag, MenuAction, Position>();
    for (auto [e, ma, pos] : view.each()) {
        if (ma.kind == MenuActionKind::SelectDiff) pos.y = diff_y;
    }
}

void level_select_handle_go(entt::registry& reg, const GoEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::LevelSelect) return;

    auto& lss = reg.ctx().get<LevelSelectState>();
    if (evt.dir == Direction::Up) {
        lss.selected_level = (lss.selected_level - 1 + LevelSelectState::LEVEL_COUNT) % LevelSelectState::LEVEL_COUNT;
        update_diff_buttons_pos(reg, lss);
    } else if (evt.dir == Direction::Down) {
        lss.selected_level = (lss.selected_level + 1) % LevelSelectState::LEVEL_COUNT;
        update_diff_buttons_pos(reg, lss);
    } else if (evt.dir == Direction::Left) {
        lss.selected_difficulty = (lss.selected_difficulty - 1 + LevelSelectState::DIFFICULTY_COUNT) % LevelSelectState::DIFFICULTY_COUNT;
    } else if (evt.dir == Direction::Right) {
        lss.selected_difficulty = (lss.selected_difficulty + 1) % LevelSelectState::DIFFICULTY_COUNT;
    }
}

void level_select_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::LevelSelect) return;
    if (gs.phase_timer < 0.05f) return;

    if (!reg.valid(evt.entity)) return;
    if (!reg.all_of<MenuButtonTag, MenuAction>(evt.entity)) return;
    auto& ma = reg.get<MenuAction>(evt.entity);
    auto& lss = reg.ctx().get<LevelSelectState>();

    switch (ma.kind) {
        case MenuActionKind::Confirm:
            lss.confirmed = true;
            break;
        case MenuActionKind::SelectLevel:
            lss.selected_level = ma.index;
            update_diff_buttons_pos(reg, lss);
            break;
        case MenuActionKind::SelectDiff:
            lss.selected_difficulty = ma.index;
            break;
        default: break;
    }
}

void level_select_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::LevelSelect) return;

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<GoEvent>();
    disp.update<ButtonPressEvent>();

    // Handle confirmed level start
    if (gs.phase_timer > 0.2f) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        if (lss.confirmed) {
            lss.confirmed = false;
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Playing;
        }
    }

    update_diff_buttons_pos(reg, reg.ctx().get<LevelSelectState>());
}
