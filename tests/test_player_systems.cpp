#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── player_action_system ─────────────────────────────────────

TEST_CASE("player_action: shape change on button press", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape   = Shape::Triangle;

    player_action_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        CHECK(ps.current  == Shape::Triangle);
        CHECK(ps.previous == Shape::Circle);
        CHECK(ps.morph_t  == 0.0f);
    }
    // Should have pushed ShapeShift SFX
    CHECK(reg.ctx().get<AudioQueue>().count > 0);
}

TEST_CASE("player_action: no shape change when same shape pressed", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    // Player starts as Circle, press Circle
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape   = Shape::Circle;

    player_action_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        CHECK(ps.current == Shape::Circle);
        CHECK(ps.morph_t == 1.0f);  // unchanged
    }
    CHECK(reg.ctx().get<AudioQueue>().count == 0);
}

TEST_CASE("player_action: swipe left changes lane", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeLeft;

    player_action_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, Lane>();
    for (auto [e, lane] : view.each()) {
        CHECK(lane.target == 0);
        CHECK(lane.lerp_t == 0.0f);
    }
}

TEST_CASE("player_action: swipe right changes lane", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeRight;

    player_action_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, Lane>();
    for (auto [e, lane] : view.each()) {
        CHECK(lane.target == 2);
    }
}

TEST_CASE("player_action: swipe left at lane 0 is clamped", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<Lane>(p).current = 0;

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeLeft;

    player_action_system(reg, 0.016f);

    CHECK(reg.get<Lane>(p).target == -1);  // no transition started
}

TEST_CASE("player_action: swipe right at lane 2 is clamped", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<Lane>(p).current = 2;

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeRight;

    player_action_system(reg, 0.016f);

    CHECK(reg.get<Lane>(p).target == -1);
}

TEST_CASE("player_action: swipe up initiates jump", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeUp;

    player_action_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, VerticalState>();
    for (auto [e, vs] : view.each()) {
        CHECK(vs.mode == VMode::Jumping);
        CHECK(vs.timer == constants::JUMP_DURATION);
    }
}

TEST_CASE("player_action: swipe down initiates slide", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeDown;

    player_action_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, VerticalState>();
    for (auto [e, vs] : view.each()) {
        CHECK(vs.mode == VMode::Sliding);
        CHECK(vs.timer == constants::SLIDE_DURATION);
    }
}

TEST_CASE("player_action: cannot jump while already jumping", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Jumping;
    reg.get<VerticalState>(p).timer = 0.2f;

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeUp;

    player_action_system(reg, 0.016f);

    // Timer should not reset
    CHECK(reg.get<VerticalState>(p).timer == 0.2f);
}

// ── player_movement_system ───────────────────────────────────

TEST_CASE("player_movement: morph_t advances toward 1.0", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<PlayerShape>(p).morph_t = 0.0f;

    player_movement_system(reg, 0.016f);

    CHECK(reg.get<PlayerShape>(p).morph_t > 0.0f);
    CHECK(reg.get<PlayerShape>(p).morph_t <= 1.0f);
}

TEST_CASE("player_movement: lane transition moves position", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<Lane>(p).target = 0;
    reg.get<Lane>(p).lerp_t = 0.0f;

    float initial_x = reg.get<Position>(p).x;
    player_movement_system(reg, 0.016f);

    CHECK(reg.get<Position>(p).x < initial_x);
}

TEST_CASE("player_movement: lane transition completes", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<Lane>(p).target = 2;
    reg.get<Lane>(p).lerp_t = 0.0f;

    // Run enough frames for transition to complete
    for (int i = 0; i < 120; ++i) {
        player_movement_system(reg, 0.016f);
    }

    CHECK(reg.get<Lane>(p).current == 2);
    CHECK(reg.get<Lane>(p).target == -1);
    CHECK(reg.get<Position>(p).x == constants::LANE_X[2]);
}

TEST_CASE("player_movement: jump creates negative y_offset", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Jumping;
    reg.get<VerticalState>(p).timer = constants::JUMP_DURATION;

    // Advance halfway through jump
    float half_jump = constants::JUMP_DURATION / 2.0f;
    player_movement_system(reg, half_jump);

    CHECK(reg.get<VerticalState>(p).y_offset < 0.0f);
}

TEST_CASE("player_movement: jump returns to grounded", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Jumping;
    reg.get<VerticalState>(p).timer = constants::JUMP_DURATION;

    // Advance past jump duration
    for (int i = 0; i < 60; ++i) {
        player_movement_system(reg, 0.016f);
    }

    CHECK(reg.get<VerticalState>(p).mode == VMode::Grounded);
    CHECK(reg.get<VerticalState>(p).y_offset == 0.0f);
}

TEST_CASE("player_movement: slide returns to grounded", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Sliding;
    reg.get<VerticalState>(p).timer = constants::SLIDE_DURATION;

    for (int i = 0; i < 60; ++i) {
        player_movement_system(reg, 0.016f);
    }

    CHECK(reg.get<VerticalState>(p).mode == VMode::Grounded);
}

TEST_CASE("player_action: not in Playing phase skips processing", "[player]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    make_player(reg);

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeUp;

    player_action_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, VerticalState>();
    for (auto [e, vs] : view.each()) {
        CHECK(vs.mode == VMode::Grounded);
    }
}

TEST_CASE("player_movement: not in Playing phase skips processing", "[player]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Paused;
    auto p = make_player(reg);
    reg.get<PlayerShape>(p).morph_t = 0.0f;

    player_movement_system(reg, 0.016f);

    CHECK(reg.get<PlayerShape>(p).morph_t == 0.0f);
}

TEST_CASE("player_action: cannot slide while already sliding", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Sliding;
    reg.get<VerticalState>(p).timer = 0.3f;

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeDown;

    player_action_system(reg, 0.016f);

    // Timer should not reset
    CHECK(reg.get<VerticalState>(p).timer == 0.3f);
}

TEST_CASE("player_action: cannot slide while jumping", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Jumping;
    reg.get<VerticalState>(p).timer = 0.2f;

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeDown;

    player_action_system(reg, 0.016f);

    CHECK(reg.get<VerticalState>(p).mode == VMode::Jumping);
    CHECK(reg.get<VerticalState>(p).timer == 0.2f);
}

TEST_CASE("player_action: cannot jump while sliding", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Sliding;
    reg.get<VerticalState>(p).timer = 0.3f;

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = Gesture::SwipeUp;

    player_action_system(reg, 0.016f);

    CHECK(reg.get<VerticalState>(p).mode == VMode::Sliding);
    CHECK(reg.get<VerticalState>(p).timer == 0.3f);
}

TEST_CASE("player_movement: morph_t clamps at 1.0", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<PlayerShape>(p).morph_t = 0.95f;

    // Large dt to overshoot
    player_movement_system(reg, 1.0f);

    CHECK(reg.get<PlayerShape>(p).morph_t == 1.0f);
}

TEST_CASE("player_movement: slide keeps y_offset at 0", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Sliding;
    reg.get<VerticalState>(p).timer = constants::SLIDE_DURATION;

    player_movement_system(reg, 0.1f);

    CHECK(reg.get<VerticalState>(p).y_offset == 0.0f);
}

TEST_CASE("player_movement: grounded state has no timer change", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Default is Grounded
    CHECK(reg.get<VerticalState>(p).mode == VMode::Grounded);

    player_movement_system(reg, 0.016f);

    CHECK(reg.get<VerticalState>(p).timer == 0.0f);
    CHECK(reg.get<VerticalState>(p).y_offset == 0.0f);
}

TEST_CASE("player_movement: jump arc peaks at half duration", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode  = VMode::Jumping;
    reg.get<VerticalState>(p).timer = constants::JUMP_DURATION;

    // Advance exactly to the peak (half duration)
    float half = constants::JUMP_DURATION / 2.0f;
    player_movement_system(reg, half);

    // At peak, y_offset should be at max negative (JUMP_HEIGHT)
    float y_off = reg.get<VerticalState>(p).y_offset;
    CHECK(y_off < 0.0f);
    CHECK_THAT(y_off, Catch::Matchers::WithinAbs(-constants::JUMP_HEIGHT, 1.0f));
}
