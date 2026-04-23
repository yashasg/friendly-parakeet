#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"
#include "systems/input_gesture.h"

namespace {
float shape_button_center_x(int i) {
    const float btn_w = constants::BUTTON_W_N * constants::SCREEN_W;
    const float btn_spacing = constants::BUTTON_SPACING_N * constants::SCREEN_W;
    const float btn_area_x = (constants::SCREEN_W - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
    return btn_area_x + static_cast<float>(i) * (btn_w + btn_spacing) + btn_w / 2.0f;
}

float shape_button_center_y() {
    const float btn_h = constants::BUTTON_H_N * constants::SCREEN_H;
    const float btn_y = constants::BUTTON_Y_N * constants::SCREEN_H;
    return btn_y + btn_h / 2.0f;
}
}  // namespace

TEST_CASE("input_gesture: bottom-zone taps always emit position", "[input][gesture]") {
    ActionQueue aq;
    InputState input;

    input.start_x = shape_button_center_x(2);
    input.start_y = shape_button_center_y();
    input.end_x = input.start_x;
    input.end_y = input.start_y;
    input.duration = 0.05f;

    enqueue_pointer_release_action(aq, input);

    REQUIRE(aq.count == 1);
    CHECK(aq.actions[0].verb == ActionVerb::Click);
    CHECK(aq.actions[0].x == input.end_x);
    CHECK(aq.actions[0].y == input.end_y);
}

TEST_CASE("player_input: position tap on shape button still morphs player", "[player][input]") {
    auto reg = make_registry();
    make_player(reg);

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.click(shape_button_center_x(2), shape_button_center_y());

    player_input_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [entity, ps] : view.each()) {
        static_cast<void>(entity);
        CHECK(ps.current == Shape::Triangle);
        CHECK(ps.previous == Shape::Circle);
        CHECK(ps.morph_t == 0.0f);
    }
}
