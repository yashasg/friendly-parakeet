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

    auto cap = drain_press_events(reg);
    CHECK(cap.count == 1);
    CHECK(cap.buf[0].entity == btn);
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

    CHECK(drain_press_events(reg).count == 0);
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

    auto cap = drain_press_events(reg);
    CHECK(cap.count == 1);
    CHECK(cap.buf[0].entity == btn);
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

    CHECK(drain_press_events(reg).count == 0);  // skipped: wrong phase
}

TEST_CASE("hit_test: swipe emits go not press", "[hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    // Create a button at origin
    auto btn = make_shape_button(reg, Shape::Circle);
    reg.get<Position>(btn) = {0.0f, 0.0f};

    eq.push_input(InputType::Swipe, 0.0f, 0.0f, Direction::Right);
    gesture_routing_system(reg);
    hit_test_system(reg);

    auto go_cap = drain_go_events(reg);
    CHECK(go_cap.count == 1);
    CHECK(go_cap.buf[0].dir == Direction::Right);
    CHECK(drain_press_events(reg).count == 0);  // swipe → go, not press
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

    CHECK(drain_press_events(reg).count == 2);
}

TEST_CASE("hit_test: no input events no output", "[hit_test]") {
    auto reg = make_registry();

    make_shape_button(reg, Shape::Circle);

    // No input events pushed
    hit_test_system(reg);

    CHECK(drain_press_events(reg).count == 0);
    CHECK(drain_go_events(reg).count == 0);
}

// ── Structural ActiveTag sync (#249) ──────────────────────────────
// hit_test_system filters via the structural ActiveTag component, which is
// (re-)synced from ActiveInPhase masks when the GamePhase changes. These
// tests pin that contract so the per-event hot loops stay predicate-free.

TEST_CASE("hit_test: ActiveTag emplaced on entities active in current phase",
          "[hit_test][active_tag]") {
    auto reg = make_registry();  // default phase = Playing
    auto btn = make_shape_button(reg, Shape::Circle);  // mask = Playing

    // Tag is not present on creation; sync happens lazily inside hit_test_system.
    CHECK_FALSE(reg.all_of<ActiveTag>(btn));

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_input(InputType::Tap, 9999.0f, 9999.0f);  // far from button
    hit_test_system(reg);

    // After the system runs, ActiveTag should be present on the Playing-phase button.
    CHECK(reg.all_of<ActiveTag>(btn));
}

TEST_CASE("hit_test: ActiveTag removed when phase no longer covers entity",
          "[hit_test][active_tag]") {
    auto reg = make_registry();
    auto btn = make_shape_button(reg, Shape::Circle);  // mask = Playing
    reg.get<Position>(btn) = {100.0f, 100.0f};
    auto& eq = reg.ctx().get<EventQueue>();

    // First pass in Playing: tag synced on, tap registers a press.
    eq.push_input(InputType::Tap, 100.0f, 100.0f);
    hit_test_system(reg);
    CHECK(reg.all_of<ActiveTag>(btn));
    CHECK(drain_press_events(reg).count == 1);

    // Move to Title; the Playing-only button must lose its ActiveTag and
    // stop receiving presses without any per-event predicate.
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    eq.clear();
    eq.push_input(InputType::Tap, 100.0f, 100.0f);
    hit_test_system(reg);

    CHECK_FALSE(reg.all_of<ActiveTag>(btn));
    CHECK(drain_press_events(reg).count == 0);
}

TEST_CASE("hit_test: ActiveTag re-emplaced when phase returns to coverage",
          "[hit_test][active_tag]") {
    auto reg = make_registry();
    auto btn = make_shape_button(reg, Shape::Circle);  // mask = Playing
    reg.get<Position>(btn) = {50.0f, 50.0f};
    auto& eq = reg.ctx().get<EventQueue>();

    // Title: button inactive.
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    hit_test_system(reg);
    CHECK_FALSE(reg.all_of<ActiveTag>(btn));

    // Back to Playing: button must become active again on next system run.
    reg.ctx().get<GameState>().phase = GamePhase::Playing;
    eq.push_input(InputType::Tap, 50.0f, 50.0f);
    hit_test_system(reg);

    CHECK(reg.all_of<ActiveTag>(btn));
    CHECK(drain_press_events(reg).count == 1);
}

TEST_CASE("hit_test: invalidate_active_tag_cache forces resync on same phase",
          "[hit_test][active_tag]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();

    // Initial sync with no buttons (warms the cache for Playing).
    hit_test_system(reg);

    // Spawn a button after the cache was warmed; without invalidation
    // ensure_active_tags_synced would skip it. Invalidating the cache
    // mirrors what spawners must do when they create entities outside
    // a phase transition.
    auto btn = make_shape_button(reg, Shape::Circle);
    reg.get<Position>(btn) = {200.0f, 200.0f};
    invalidate_active_tag_cache(reg);

    eq.push_input(InputType::Tap, 200.0f, 200.0f);
    hit_test_system(reg);

    CHECK(reg.all_of<ActiveTag>(btn));
    CHECK(drain_press_events(reg).count == 1);
}
