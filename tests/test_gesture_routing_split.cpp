#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// Tests the #272 split: gesture_routing_system handles swipes only,
// hit_test_system handles taps only. Together they preserve previous behavior.

TEST_CASE("gesture_routing: swipe input emits GoEvent", "[gesture_routing][issue272]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    eq.push_input(InputType::Swipe, 0.0f, 0.0f, Direction::Up);
    gesture_routing_system(reg);

    auto cap = drain_go_events(reg);
    CHECK(cap.count == 1);
    CHECK(cap.buf[0].dir == Direction::Up);
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("gesture_routing: tap input does not emit GoEvent", "[gesture_routing][issue272]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    // A button exists at (100, 100) but gesture_routing must not see it.
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));

    eq.push_input(InputType::Tap, 100.0f, 100.0f);
    gesture_routing_system(reg);

    // Gesture system must not perform spatial hit-tests.
    CHECK(drain_go_events(reg).count == 0);
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("gesture_routing: multiple swipes preserve order", "[gesture_routing][issue272]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    eq.push_input(InputType::Swipe, 0.0f, 0.0f, Direction::Left);
    eq.push_input(InputType::Tap,   10.0f, 10.0f);
    eq.push_input(InputType::Swipe, 0.0f, 0.0f, Direction::Right);
    gesture_routing_system(reg);

    auto cap = drain_go_events(reg);
    REQUIRE(cap.count == 2);
    CHECK(cap.buf[0].dir == Direction::Left);
    CHECK(cap.buf[1].dir == Direction::Right);
    // Tap is intentionally untouched by gesture routing.
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("hit_test: swipe input is ignored (no GoEvent emitted)", "[hit_test][issue272]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));

    eq.push_input(InputType::Swipe, 100.0f, 100.0f, Direction::Down);
    hit_test_system(reg);

    // hit_test must no longer route swipes — that is gesture_routing's job.
    CHECK(drain_go_events(reg).count == 0);
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("hit_test: tap inside hitbox emits press without gesture routing",
          "[hit_test][issue272]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));

    eq.push_input(InputType::Tap, 110.0f, 110.0f);
    hit_test_system(reg);

    auto cap = drain_press_events(reg);
    REQUIRE(cap.count == 1);
    CHECK(cap.buf[0].entity == btn);
    CHECK(drain_go_events(reg).count == 0);
}

TEST_CASE("split systems: mixed inputs preserve event ordering and counts",
          "[hit_test][gesture_routing][issue272]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 200.0f, 200.0f);
    reg.emplace<HitBox>(btn, 30.0f, 30.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));

    eq.push_input(InputType::Swipe, 0.0f, 0.0f, Direction::Left);
    eq.push_input(InputType::Tap,   210.0f, 210.0f);
    eq.push_input(InputType::Swipe, 0.0f, 0.0f, Direction::Right);
    eq.push_input(InputType::Tap,   500.0f, 500.0f);  // outside btn

    gesture_routing_system(reg);
    hit_test_system(reg);

    auto go_cap = drain_go_events(reg);
    REQUIRE(go_cap.count == 2);
    CHECK(go_cap.buf[0].dir == Direction::Left);
    CHECK(go_cap.buf[1].dir == Direction::Right);

    auto press_cap = drain_press_events(reg);
    REQUIRE(press_cap.count == 1);
    CHECK(press_cap.buf[0].entity == btn);
}

TEST_CASE("hit_test: ActiveTag phase filtering preserved after split",
          "[hit_test][active_tag][issue272]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& eq = reg.ctx().get<EventQueue>();

    // Button only active in Playing — current phase is Title.
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<Position>(btn, 100.0f, 100.0f);
    reg.emplace<HitBox>(btn, 50.0f, 50.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));

    eq.push_input(InputType::Tap, 110.0f, 110.0f);
    hit_test_system(reg);
    CHECK(drain_press_events(reg).count == 0);  // not active in Title

    // Switch to Playing — now active.
    eq.clear();
    reg.ctx().get<GameState>().phase = GamePhase::Playing;
    eq.push_input(InputType::Tap, 110.0f, 110.0f);
    hit_test_system(reg);
    CHECK(drain_press_events(reg).count == 1);
}
