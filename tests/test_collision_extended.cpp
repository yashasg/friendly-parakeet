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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
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
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<MotionVelocity>(obs, MotionVelocity{{0.0f, 400.0f}});
    reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate, int16_t{200});
    reg.emplace<RequiredShape>(obs, Shape::Hexagon);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{255, 255, 255, 255});

    collision_system(reg, 0.016f);
    // Hexagon should NEVER clear shape gates
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    // Hexagon should NEVER clear shape gates
    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

// ── collision_system: rhythm mode timing grades ──────────────

TEST_CASE("collision: rhythm mode assigns Perfect for on-time hit", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.graded = false;
    sw.window_start = song.song_time;
    sw.press_time = song.song_time;

    // obstacle arrives right at press_time (perfect)
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
    sw.phase = WindowPhase::Active;
    sw.graded = false;
    sw.window_start = song.song_time;
    sw.press_time = song.song_time;

    // obstacle arrival time far from press_time -> Bad
    float bad_arrival = song.song_time + song.half_window * 1.2f;
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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    sw.phase = WindowPhase::Active;
    sw.graded = false;
    sw.press_time = song.song_time;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.perfect_count == 1);
}

TEST_CASE("collision: multiple misses accumulate in miss_count", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);

    // First miss
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    // Reset game over for second collision
    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = false;
    gs.phase = GamePhase::Playing;

    // Second miss
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
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

TEST_CASE("collision: Hexagon fails combo gate even when shape and lane match", "[collision][rhythm][issue219]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);

    auto obs = make_combo_gate(reg, Shape::Hexagon, 0b101, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<TimingGrade>(obs));

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    const auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 1);
    CHECK(results.perfect_count == 0);
    CHECK(results.good_count == 0);
    CHECK(results.ok_count == 0);
    CHECK(results.bad_count == 0);

    const auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

// ── collision_system: split path ─────────────────────────────

TEST_CASE("collision: split path succeeds with correct shape and lane", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.current = Shape::Triangle;
    auto& lane = reg.get<Lane>(p);
    lane.current = 2;
    reg.get<WorldTransform>(p).position.x = constants::LANE_X[2];

    auto obs = make_split_path(reg, Shape::Triangle, 2, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: Hexagon fails split path even when shape and lane match", "[collision][rhythm][issue219]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);

    auto obs = make_split_path(reg, Shape::Hexagon, 1, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<TimingGrade>(obs));

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    const auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 1);
    CHECK(results.perfect_count == 0);
    CHECK(results.good_count == 0);
    CHECK(results.ok_count == 0);
    CHECK(results.bad_count == 0);

    const auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

// ── SRP observation: collision_system must NOT mutate SongResults ──────────
// After this move, perfect_count is owned by scoring_system.
// Running collision but withholding scoring must leave all counts at zero.
TEST_CASE("scoring: collision_system alone does not mutate SongResults counts", "[scoring][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.graded = false;
    sw.press_time = song.song_time;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    // Run collision only — scoring_system intentionally NOT called.
    collision_system(reg, 0.016f);

    // TimingGrade IS emplaced by collision (event component), but SongResults
    // counters must remain zero until scoring_system processes it.
    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.perfect_count == 0);
    CHECK(results.good_count    == 0);
    CHECK(results.ok_count      == 0);
    CHECK(results.bad_count     == 0);
}
