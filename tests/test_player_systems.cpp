#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── semantic input drain ─────────────────────────────────────

TEST_CASE("player_action: shape change on button press", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        (void)ps;
        CHECK(current_player_shape(reg, e) == Shape::Triangle);
        CHECK(ps.morph_t  == 1.0f);
    }
    // Should have pushed ShapeShift SFX
    CHECK(drain_sfx_events(reg).count > 0);
}

TEST_CASE("player_action: no shape change when same shape pressed", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    // Player starts as Circle, press Circle
    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        (void)ps;
        CHECK(current_player_shape(reg, e) == Shape::Circle);
        CHECK(ps.morph_t == 1.0f);  // unchanged
    }
    CHECK(drain_sfx_events(reg).count == 0);
}

TEST_CASE("player_action: swipe left changes lane", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoLeftEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    auto view = reg.view<PlayerTag, Lane>();
    for (auto [e, lane] : view.each()) {
        CHECK(lane.target == 0);
        CHECK(lane.lerp_t == 0.0f);
    }
}

TEST_CASE("player_action: swipe right changes lane", "[player]") {
    auto reg = make_registry();
    make_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoRightEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    auto view = reg.view<PlayerTag, Lane>();
    for (auto [e, lane] : view.each()) {
        CHECK(lane.target == 2);
    }
}

TEST_CASE("player_action: repeated lane input during transition does not restart interpolation",
          "[player][issue1043]") {
    auto reg = make_registry();
    auto p = make_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoRightEvent>({});
    run_semantic_input_tick(reg, 0.016f);

    auto& lane = reg.get<Lane>(p);
    lane.lerp_t = 0.4f;

    reg.ctx().get<entt::dispatcher>().enqueue<GoRightEvent>({});
    run_semantic_input_tick(reg, 0.016f);

    CHECK(lane.current == 1);
    CHECK(lane.target == 2);
    CHECK_THAT(lane.lerp_t, Catch::Matchers::WithinAbs(0.4f, 0.0001f));
}

TEST_CASE("player_action: opposite lane input during transition waits for completion",
          "[player][issue1043]") {
    auto reg = make_registry();
    auto p = make_player(reg);

    auto& lane = reg.get<Lane>(p);
    lane.current = 1;
    lane.target = 2;
    lane.lerp_t = 0.4f;

    reg.ctx().get<entt::dispatcher>().enqueue<GoLeftEvent>({});
    run_semantic_input_tick(reg, 0.016f);

    CHECK(lane.current == 1);
    CHECK(lane.target == 2);
    CHECK_THAT(lane.lerp_t, Catch::Matchers::WithinAbs(0.4f, 0.0001f));
}

TEST_CASE("player_action: swipe left at lane 0 is clamped", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<Lane>(p).current = 0;

    reg.ctx().get<entt::dispatcher>().enqueue<GoLeftEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    CHECK(reg.get<Lane>(p).target == -1);  // no transition started
}

TEST_CASE("player_action: swipe right at lane 2 is clamped", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<Lane>(p).current = 2;

    reg.ctx().get<entt::dispatcher>().enqueue<GoRightEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    CHECK(reg.get<Lane>(p).target == -1);
}

TEST_CASE("player_action: swipe up starts jump", "[player]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoUpEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    REQUIRE(reg.all_of<Jumping>(player));
    CHECK(reg.get<Jumping>(player).timer == constants::JUMP_DURATION);
}

TEST_CASE("player_action: swipe down starts slide", "[player]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoDownEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    REQUIRE(reg.all_of<Sliding>(player));
    CHECK(reg.get<Sliding>(player).timer == constants::SLIDE_DURATION);
}

TEST_CASE("player_action: cannot jump while already jumping", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Jumping>(p, Jumping{0.2f, 0.0f});

    reg.ctx().get<entt::dispatcher>().enqueue<GoUpEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    // Timer should not reset
    CHECK(reg.get<Jumping>(p).timer == 0.2f);
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

    float initial_x = reg.get<WorldTransform>(p).position.x;
    player_movement_system(reg, 0.016f);

    CHECK(reg.get<WorldTransform>(p).position.x < initial_x);
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
    CHECK(reg.get<WorldTransform>(p).position.x == constants::LANE_X[2]);
}

TEST_CASE("player_movement: clears stale lane target when target equals current", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& lane = reg.get<Lane>(p);
    auto& transform = reg.get<WorldTransform>(p);
    lane.current = 1;
    lane.target = 1;
    lane.lerp_t = 0.0f;
    transform.position.x = (constants::LANE_X[1] + constants::LANE_X[2]) * 0.5f;

    player_movement_system(reg, 0.016f);

    CHECK(lane.target == -1);
    CHECK(lane.lerp_t == 1.0f);
    CHECK(transform.position.x == constants::LANE_X[1]);
}

TEST_CASE("player_movement: normalizes invalid current lane", "[player][issue900]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& lane = reg.get<Lane>(p);
    auto& transform = reg.get<WorldTransform>(p);
    lane.current = -1;
    lane.target = 0;
    lane.lerp_t = 0.0f;
    transform.position.x = -999.0f;

    player_movement_system(reg, 0.016f);

    CHECK(lane.current == constants::DEFAULT_LANE);
    CHECK(lane.target == -1);
    CHECK(lane.lerp_t == 1.0f);
    CHECK(transform.position.x == constants::LANE_X[constants::DEFAULT_LANE]);
}

TEST_CASE("player_movement: normalizes invalid target lane", "[player][issue900]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& lane = reg.get<Lane>(p);
    auto& transform = reg.get<WorldTransform>(p);
    lane.current = 1;
    lane.target = constants::LANE_COUNT;
    lane.lerp_t = 0.0f;
    transform.position.x = (constants::LANE_X[1] + constants::LANE_X[2]) * 0.5f;

    player_movement_system(reg, 0.016f);

    CHECK(lane.current == 1);
    CHECK(lane.target == -1);
    CHECK(lane.lerp_t == 1.0f);
    CHECK(transform.position.x == constants::LANE_X[1]);
}

TEST_CASE("player_movement: jump creates negative y_offset", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Jumping>(p, Jumping{constants::JUMP_DURATION, 0.0f});

    // Advance halfway through jump
    float half_jump = constants::JUMP_DURATION / 2.0f;
    player_movement_system(reg, half_jump);

    REQUIRE(reg.all_of<Jumping>(p));
    CHECK(reg.get<Jumping>(p).y_offset < 0.0f);
}

TEST_CASE("player_movement: jump returns to grounded", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Jumping>(p, Jumping{constants::JUMP_DURATION, 0.0f});

    // Advance past jump duration
    for (int i = 0; i < 60; ++i) {
        player_movement_system(reg, 0.016f);
    }

    // Grounded = no Jumping/Sliding components.
    CHECK_FALSE(reg.any_of<Jumping, Sliding>(p));
}

TEST_CASE("player_movement: slide returns to grounded", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Sliding>(p, Sliding{constants::SLIDE_DURATION});

    for (int i = 0; i < 60; ++i) {
        player_movement_system(reg, 0.016f);
    }

    CHECK_FALSE(reg.any_of<Jumping, Sliding>(p));
}

TEST_CASE("player_action: not in Playing phase skips processing", "[player]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseTitleTag>(reg);
    make_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoUpEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    auto view = reg.view<PlayerTag>();
    for (auto p : view) {
        CHECK_FALSE(reg.any_of<Jumping, Sliding>(p));
    }
}

TEST_CASE("player_movement: not in Playing phase skips processing", "[player]") {
    auto reg = make_registry();
    set_test_phase<GamePhasePausedTag>(reg);
    auto p = make_player(reg);
    reg.get<PlayerShape>(p).morph_t = 0.0f;

    tick_playing_systems(reg, 0.016f);

    CHECK(reg.get<PlayerShape>(p).morph_t == 0.0f);
}

TEST_CASE("player_action: cannot slide while already sliding", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Sliding>(p, Sliding{0.3f});

    reg.ctx().get<entt::dispatcher>().enqueue<GoDownEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    // Timer should not reset
    CHECK(reg.get<Sliding>(p).timer == 0.3f);
}

TEST_CASE("player_action: cannot slide while jumping", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Jumping>(p, Jumping{0.2f, 0.0f});

    reg.ctx().get<entt::dispatcher>().enqueue<GoDownEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    REQUIRE(reg.all_of<Jumping>(p));
    CHECK_FALSE(reg.all_of<Sliding>(p));
    CHECK(reg.get<Jumping>(p).timer == 0.2f);
}

TEST_CASE("player_action: cannot jump while sliding", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Sliding>(p, Sliding{0.3f});

    reg.ctx().get<entt::dispatcher>().enqueue<GoUpEvent>({});

    run_semantic_input_tick(reg, 0.016f);

    REQUIRE(reg.all_of<Sliding>(p));
    CHECK_FALSE(reg.all_of<Jumping>(p));
    CHECK(reg.get<Sliding>(p).timer == 0.3f);
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
    reg.emplace<Sliding>(p, Sliding{constants::SLIDE_DURATION});

    player_movement_system(reg, 0.1f);

    // Slide has no y_offset field — semantic check: player still on ground.
    REQUIRE(reg.all_of<Sliding>(p));
    CHECK_FALSE(reg.all_of<Jumping>(p));
}

TEST_CASE("player_movement: grounded state has no timer change", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Default = no Jumping/Sliding tags = grounded.
    REQUIRE_FALSE(reg.any_of<Jumping, Sliding>(p));

    player_movement_system(reg, 0.016f);

    // Grounded entities don't gain any vertical-motion tags.
    CHECK_FALSE(reg.any_of<Jumping, Sliding>(p));
}

TEST_CASE("player_movement: jump arc peaks at half duration", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.emplace<Jumping>(p, Jumping{constants::JUMP_DURATION, 0.0f});

    // Advance exactly to the peak (half duration)
    float half = constants::JUMP_DURATION / 2.0f;
    player_movement_system(reg, half);

    // At peak, y_offset should be at max negative (JUMP_HEIGHT)
    REQUIRE(reg.all_of<Jumping>(p));
    float y_off = reg.get<Jumping>(p).y_offset;
    CHECK(y_off < 0.0f);
    CHECK_THAT(y_off, Catch::Matchers::WithinAbs(-constants::JUMP_HEIGHT, 1.0f));
}
