#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── collision_system: Hexagon rejection ──────────────────────

TEST_CASE("collision: Hexagon shape never matches shape gate", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.current = Shape::Hexagon;
    // Gate requires Circle, player is Hexagon
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
}

TEST_CASE("collision: Hexagon fails even when matching gate shape", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.current = Shape::Hexagon;
    // This should NOT pass — Hexagon is the neutral shape
    // Note: make_shape_gate with Hexagon isn't typical, but tests the guard
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<Velocity>(obs, 0.0f, 400.0f);
    reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate, int16_t{200});
    reg.emplace<RequiredShape>(obs, Shape::Hexagon);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<DrawColor>(obs, uint8_t{255}, uint8_t{255}, uint8_t{255}, uint8_t{255});

    collision_system(reg, 0.016f);

    // Hexagon should NEVER clear shape gates
    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
}

// ── collision_system: rhythm mode timing grades ──────────────

TEST_CASE("collision: rhythm mode assigns Perfect for on-time hit", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    sw.graded = false;
    sw.window_start = song.song_time;
    sw.peak_time = song.song_time;

    // obstacle arrives right at peak_time (perfect)
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Perfect);
}

TEST_CASE("collision: rhythm mode assigns Bad for far-off hit", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    sw.graded = false;
    sw.window_start = song.song_time;
    sw.peak_time = song.song_time + song.half_window;

    // obstacle arrival time far from peak_time → Bad
    float bad_arrival = song.song_time + song.half_window * 2.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, bad_arrival, bad_arrival - song.lead_time);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Bad);
}

TEST_CASE("collision: rhythm miss increments miss_count in SongResults", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    // Player is Hexagon, gate requires Circle → miss
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 1);
}

TEST_CASE("collision: rhythm perfect increments perfect_count in SongResults", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    sw.graded = false;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    collision_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.perfect_count == 1);
}

TEST_CASE("collision: multiple misses accumulate in miss_count", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);

    // First miss
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    // Reset game over for second collision
    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = false;
    gs.phase = GamePhase::Playing;

    // Second miss
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 2);
}

// ── collision_system: combo gate edge cases ──────────────────

TEST_CASE("collision: combo gate requires both shape AND open lane", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.current = Shape::Triangle;

    // Shape matches but player's lane (1) is blocked
    make_combo_gate(reg, Shape::Triangle, 0b010, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: combo gate succeeds when both match", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.current = Shape::Square;

    // Shape matches, lane 1 is NOT blocked (mask blocks 0 and 2)
    auto obs = make_combo_gate(reg, Shape::Square, 0b101, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

// ── collision_system: split path ─────────────────────────────

TEST_CASE("collision: split path succeeds with correct shape and lane", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.current = Shape::Triangle;
    auto& lane = reg.get<Lane>(p);
    lane.current = 2;
    reg.get<Position>(p).x = constants::LANE_X[2];

    auto obs = make_split_path(reg, Shape::Triangle, 2, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

// ── collision_system: vertical bar combinations ──────────────

TEST_CASE("collision: high bar cleared only when sliding", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Sliding;
    auto obs = make_vertical_bar(reg, ObstacleKind::HighBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: low bar fails when sliding", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Sliding;
    make_vertical_bar(reg, ObstacleKind::LowBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: high bar fails when jumping", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Jumping;
    make_vertical_bar(reg, ObstacleKind::HighBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
}

