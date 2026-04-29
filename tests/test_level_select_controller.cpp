#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "ui/level_select_controller.h"

TEST_CASE("level_select_controller: select difficulty press updates state", "[level_select][ui]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 0.2f;

    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 0;

    level_select_handle_press(reg, ButtonPressEvent{
        ButtonPressKind::Menu, Shape::Circle, MenuActionKind::SelectDiff, 2
    });

    CHECK(lss.selected_difficulty == 2);
}

TEST_CASE("level_select_controller: select level press updates state", "[level_select][ui]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 0.2f;

    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    level_select_handle_press(reg, ButtonPressEvent{
        ButtonPressKind::Menu, Shape::Circle, MenuActionKind::SelectLevel, 2
    });

    CHECK(lss.selected_level == 2);
}

TEST_CASE("level_select_controller: ignores menu presses during entry delay", "[level_select][ui]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 0.01f;

    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 1;

    level_select_handle_press(reg, ButtonPressEvent{
        ButtonPressKind::Menu, Shape::Circle, MenuActionKind::SelectDiff, 0
    });

    CHECK(lss.selected_difficulty == 1);
}
