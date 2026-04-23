#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── hit_test_system unit tests ─────────────────────────────────────

TEST_CASE("hit_test: tap inside hitbox emits press", "[hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);  // half extents: ±50
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));

    eq.push_input(InputType::Tap, 120.0f, 110.0f);  // inside
    hit_test_system(reg);

    CHECK(eq.press_count == 1);
    CHECK(eq.presses[0].entity == btn);
}

TEST_CASE("hit_test: tap outside hitbox no event", "[hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));

    eq.push_input(InputType::Tap, 300.0f, 300.0f);  // outside
    hit_test_system(reg);

    CHECK(eq.press_count == 0);
}

TEST_CASE("hit_test: tap inside hitcircle emits press", "[hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    auto btn = make_shape_button(reg, Shape::Circle);
    // Place button at known position
    reg.get<Position>(btn) = {200.0f, 200.0f};
    reg.get<HitCircle>(btn).radius = 40.0f;

    eq.push_input(InputType::Tap, 210.0f, 210.0f);  // inside circle
    hit_test_system(reg);

    CHECK(eq.press_count == 1);
    CHECK(eq.presses[0].entity == btn);
}

TEST_CASE("hit_test: wrong phase skips entity", "[hit_test]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& eq = reg.ctx().get<EventQueue>();

    // Button is active only in Playing phase
    auto btn = make_shape_button(reg, Shape::Circle);
    reg.get<Position>(btn) = {100.0f, 100.0f};

    eq.push_input(InputType::Tap, 100.0f, 100.0f);  // exact center
    hit_test_system(reg);

    CHECK(eq.press_count == 0);  // skipped: wrong phase
}

TEST_CASE("hit_test: swipe emits go not press", "[hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    // Create a button at origin
    auto btn = make_shape_button(reg, Shape::Circle);
    reg.get<Position>(btn) = {0.0f, 0.0f};

    eq.push_input(InputType::Swipe, 0.0f, 0.0f, Direction::Right);
    hit_test_system(reg);

    CHECK(eq.go_count == 1);
    CHECK(eq.goes[0].dir == Direction::Right);
    CHECK(eq.press_count == 0);  // swipe → go, not press
}

TEST_CASE("hit_test: penetrating hits multiple entities", "[hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    // Two overlapping buttons at same position
    auto btn1 = make_shape_button(reg, Shape::Circle);
    reg.get<Position>(btn1) = {100.0f, 100.0f};
    reg.get<HitCircle>(btn1).radius = 50.0f;

    auto btn2 = make_shape_button(reg, Shape::Square);
    reg.get<Position>(btn2) = {100.0f, 100.0f};
    reg.get<HitCircle>(btn2).radius = 50.0f;

    eq.push_input(InputType::Tap, 100.0f, 100.0f);
    hit_test_system(reg);

    CHECK(eq.press_count == 2);
}

TEST_CASE("hit_test: no input events no output", "[hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    make_shape_button(reg, Shape::Circle);

    // No input events pushed
    hit_test_system(reg);

    CHECK(eq.press_count == 0);
    CHECK(eq.go_count == 0);
}
