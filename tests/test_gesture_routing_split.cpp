#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// Tests the #272 split: gesture_routing_handle_input handles swipes only,
// hit_test_handle_input handles taps only. Together they preserve previous
// behavior via the Tier-1 InputEvent dispatcher (#333).

TEST_CASE("gesture_routing: swipe input emits GoEvent", "[gesture_routing][issue272]") {
    auto reg = make_registry();

    push_input(reg, InputType::Swipe, 0.0f, 0.0f, Direction::Up);
    run_input_tier1(reg);

    auto cap = drain_go_events(reg);
    CHECK(cap.count == 1);
    CHECK(cap.buf[0].dir == Direction::Up);
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("gesture_routing: tap input does not emit GoEvent", "[gesture_routing][issue272]") {
    auto reg = make_registry();

    // A button exists at (100, 100) but gesture_routing must not see it.
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Playing);

    push_input(reg, InputType::Tap, 100.0f, 100.0f);
    // gesture_routing_handle_input fires but ignores taps — only hit_test emits presses
    run_input_tier1(reg);

    // Gesture listener must not perform spatial hit-tests.
    CHECK(drain_go_events(reg).count == 0);
    // hit_test_handle_input did produce a press — that's correct and expected here.
    // We only care that gesture_routing did NOT emit a GoEvent.
    drain_press_events(reg);  // drain without asserting — not this test's concern
}

TEST_CASE("gesture_routing: multiple swipes preserve order", "[gesture_routing][issue272]") {
    auto reg = make_registry();

    push_input(reg, InputType::Swipe, 0.0f, 0.0f, Direction::Left);
    push_input(reg, InputType::Tap,   10.0f, 10.0f);
    push_input(reg, InputType::Swipe, 0.0f, 0.0f, Direction::Right);
    run_input_tier1(reg);

    auto cap = drain_go_events(reg);
    REQUIRE(cap.count == 2);
    CHECK(cap.buf[0].dir == Direction::Left);
    CHECK(cap.buf[1].dir == Direction::Right);
    // Tap is intentionally untouched by gesture routing.
    // (hit_test_handle_input may have emitted a press if a button was hit,
    // but none exists here so the queue is empty.)
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("hit_test: swipe input is ignored (no GoEvent emitted)", "[hit_test][issue272]") {
    auto reg = make_registry();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Playing);

    push_input(reg, InputType::Swipe, 100.0f, 100.0f, Direction::Down);
    run_input_tier1(reg);

    // hit_test must not route swipes — that is gesture_routing's job.
    // gesture_routing DID emit a GoEvent for the swipe.
    auto go_cap = drain_go_events(reg);
    CHECK(go_cap.count == 1);
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("hit_test: tap inside hitbox emits press without gesture routing",
          "[hit_test][issue272]") {
    auto reg = make_registry();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Playing);

    push_input(reg, InputType::Tap, 110.0f, 110.0f);
    run_input_tier1(reg);

    auto cap = drain_press_events(reg);
    REQUIRE(cap.count == 1);
    CHECK(cap.buf[0].kind        == ButtonPressKind::Menu);
    CHECK(cap.buf[0].menu_action == MenuActionKind::Confirm);
    CHECK(drain_go_events(reg).count == 0);
}

TEST_CASE("split systems: mixed inputs preserve event ordering and counts",
          "[hit_test][gesture_routing][issue272]") {
    auto reg = make_registry();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 200.0f, 200.0f);
    reg.emplace<HitBox>(btn, 30.0f, 30.0f);
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Playing);

    push_input(reg, InputType::Swipe, 0.0f,   0.0f,   Direction::Left);
    push_input(reg, InputType::Tap,   210.0f, 210.0f);            // inside btn
    push_input(reg, InputType::Swipe, 0.0f,   0.0f,   Direction::Right);
    push_input(reg, InputType::Tap,   500.0f, 500.0f);            // outside btn
    run_input_tier1(reg);

    auto go_cap = drain_go_events(reg);
    REQUIRE(go_cap.count == 2);
    CHECK(go_cap.buf[0].dir == Direction::Left);
    CHECK(go_cap.buf[1].dir == Direction::Right);

    auto press_cap = drain_press_events(reg);
    REQUIRE(press_cap.count == 1);
    CHECK(press_cap.buf[0].kind        == ButtonPressKind::Menu);
    CHECK(press_cap.buf[0].menu_action == MenuActionKind::Confirm);
}

TEST_CASE("hit_test: ActiveTag phase filtering preserved after split",
          "[hit_test][active_tag][issue272]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;

    // Button only active in Playing — current phase is Title.
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Playing);

    push_input(reg, InputType::Tap, 110.0f, 110.0f);
    run_input_tier1(reg);
    CHECK(drain_press_events(reg).count == 0);  // not active in Title

    // Switch to Playing — now active.
    reg.ctx().get<GameState>().phase = GamePhase::Playing;
    push_input(reg, InputType::Tap, 110.0f, 110.0f);
    run_input_tier1(reg);
    CHECK(drain_press_events(reg).count == 1);
}
