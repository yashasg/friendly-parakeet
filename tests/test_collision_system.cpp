#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
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

TEST_CASE("collision: shape gate drains energy with wrong shape", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle, gate requires Triangle
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: lane block cleared when player in unblocked lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player in lane 1 (center), block lane 0
    auto obs = make_lane_block(reg, 0b001, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: lane block drains energy when player in blocked lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player in lane 1 (center), block lane 1
    make_lane_block(reg, 0b010, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: low bar cleared when jumping", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Jumping;
    auto obs = make_vertical_bar(reg, ObstacleKind::LowBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: low bar drains energy when grounded", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    make_vertical_bar(reg, ObstacleKind::LowBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: high bar cleared when sliding", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Sliding;
    auto obs = make_vertical_bar(reg, ObstacleKind::HighBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: high bar drains energy when grounded", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    make_vertical_bar(reg, ObstacleKind::HighBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
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
    reg.emplace<DrawColor>(obs, uint8_t{200}, uint8_t{100}, uint8_t{255}, uint8_t{255});

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
    reg.emplace<DrawColor>(obs, uint8_t{200}, uint8_t{100}, uint8_t{255}, uint8_t{255});

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: combo gate fails when lane is blocked", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle in lane 1, shape matches but lane is blocked
    make_combo_gate(reg, Shape::Circle, 0b010, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: split path cleared with matching shape and lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle in lane 1
    auto obs = make_split_path(reg, Shape::Circle, 1, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: split path fails with wrong shape", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle, requires Triangle
    make_split_path(reg, Shape::Triangle, 1, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: split path fails with wrong lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle in lane 1, requires lane 0
    make_split_path(reg, Shape::Circle, 0, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: no player means no collision processing", "[collision]") {
    auto reg = make_registry();
    // No player, just an obstacle at collision range
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: not in Playing phase skips processing", "[collision]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: low bar drains energy when sliding", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Sliding;
    make_vertical_bar(reg, ObstacleKind::LowBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: high bar drains energy when jumping", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Jumping;
    make_vertical_bar(reg, ObstacleKind::HighBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: BAD timing adjusts window_start, not window_timer", "[collision][rhythm]") {
    // Verify that for a BAD hit (scale < 1), collision_system adjusts window_start
    // backward instead of advancing window_timer.  shape_window_system derives
    // window_timer from song_time - window_start each frame, so only window_start
    // changes survive across ticks.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Put player in Active phase
    ps.current = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    sw.graded = false;
    sw.window_timer = 0.0f;
    sw.window_start = song.song_time;

    // peak_time doesn't affect grading anymore — timing is based on
    // BeatInfo.arrival_time.  Set arrival_time far from song_time so
    // pct_from_peak > 0.75 → BAD (scale = 0.5).
    sw.peak_time = song.song_time;
    float bad_arrival = song.song_time - song.half_window * 2.0f;

    // Spawn an obstacle at the player's position
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, bad_arrival, bad_arrival - song.lead_time);

    float original_window_start = sw.window_start;
    collision_system(reg, 0.016f);

    // window_start must have moved backward (earlier) to shorten the window
    CHECK(sw.window_start < original_window_start);
    // window_timer should remain unchanged by collision_system
    CHECK(sw.window_timer == 0.0f);
    // graded flag must be set
    CHECK(sw.graded);
}

TEST_CASE("collision: Perfect timing extends window via window_scale only", "[collision][rhythm]") {
    // For a Perfect hit (scale > 1), only window_scale is updated; window_start
    // must not be changed (no shortening needed).
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    sw.graded = false;
    sw.window_timer = 0.0f;
    sw.window_start = song.song_time;

    // Set peak_time to right now so the hit is perfect (pct_from_peak = 0)
    sw.peak_time = song.song_time;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    float original_window_start = sw.window_start;
    collision_system(reg, 0.016f);

    // window_start must NOT be adjusted for Perfect (scale >= 1)
    CHECK_THAT(sw.window_start, Catch::Matchers::WithinAbs(original_window_start, 0.0001f));
    CHECK(sw.window_scale > 1.0f);
    CHECK(sw.graded);
}
