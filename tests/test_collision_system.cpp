#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("collision: shape gate cleared with matching shape", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle by default
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: shape gate kills with wrong shape", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle, gate requires Triangle
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
}

TEST_CASE("collision: lane block cleared when player in unblocked lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player in lane 1 (center), block lane 0
    auto obs = make_lane_block(reg, 0b001, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: lane block kills when player in blocked lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player in lane 1 (center), block lane 1
    make_lane_block(reg, 0b010, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: low bar cleared when jumping", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Jumping;
    auto obs = make_vertical_bar(reg, ObstacleKind::LowBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: low bar kills when grounded", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    make_vertical_bar(reg, ObstacleKind::LowBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: high bar cleared when sliding", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Sliding;
    auto obs = make_vertical_bar(reg, ObstacleKind::HighBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: high bar kills when grounded", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    make_vertical_bar(reg, ObstacleKind::HighBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: obstacle too far away is ignored", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Place obstacle far above player
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 200.0f);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: already scored obstacles are skipped", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: combo gate requires shape AND lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle in lane 1
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, ObstacleKind::ComboGate, int16_t{200});
    reg.emplace<RequiredShape>(obs, Shape::Circle);
    // Block lanes 0 and 2, leave lane 1 open
    reg.emplace<BlockedLanes>(obs, uint8_t{0b101});
    reg.emplace<DrawSize>(obs, 720.0f, 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, uint8_t{200}, uint8_t{100}, uint8_t{255}, uint8_t{255});

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: combo gate fails with wrong shape", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, ObstacleKind::ComboGate, int16_t{200});
    reg.emplace<RequiredShape>(obs, Shape::Triangle);  // wrong shape
    reg.emplace<BlockedLanes>(obs, uint8_t{0b101});    // lane 1 open
    reg.emplace<DrawSize>(obs, 720.0f, 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, uint8_t{200}, uint8_t{100}, uint8_t{255}, uint8_t{255});

    collision_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
}
