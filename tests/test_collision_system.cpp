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
    auto obs = make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);
    CHECK(reg.all_of<MissTag>(obs));

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: shape gate hitbox edge handling", "[collision][edge]") {
    constexpr struct {
        float offset;
        bool missed;
    } cases[] = {{constants::PLAYER_SIZE, false}, {constants::PLAYER_SIZE + 0.01f, true}};

    for (const auto& tc : cases) {
        auto reg = make_registry();
        auto player = make_player(reg);
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

        reg.get<WorldTransform>(obs).position.x = reg.get<WorldTransform>(player).position.x + tc.offset;
        collision_system(reg, 0.016f);

        CHECK(reg.all_of<ScoredTag>(obs));
        CHECK(reg.all_of<MissTag>(obs) == tc.missed);
    }
}

TEST_CASE("collision: shape gate does not clear from target lane before strafe arrives",
          "[collision][issue892]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    reg.get<WorldTransform>(obs).position.x = constants::LANE_X[0];
    lane.target = 0;
    lane.lerp_t = 0.0f;

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK(reg.all_of<MissTag>(obs));
}

TEST_CASE("collision: shape gate ignores shape-derived lane without explicit target",
          "[collision][issue874]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    reg.get<WorldTransform>(obs).position.x = constants::LANE_X[0];

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK(reg.all_of<MissTag>(obs));
}

TEST_CASE("collision: shape gate clears when interpolated player hitbox reaches lane",
          "[collision][issue892]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    reg.get<WorldTransform>(obs).position.x = constants::LANE_X[0];
    reg.get<WorldTransform>(player).position.x = constants::LANE_X[0];
    lane.target = 0;
    lane.lerp_t = 1.0f;

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: invalid player lane is normalized before lane checks",
          "[collision][issue900]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& transform = reg.get<WorldTransform>(player);
    lane.current = -1;
    lane.target = constants::LANE_COUNT;
    lane.lerp_t = 0.0f;
    transform.position.x = -999.0f;
    auto obs = make_lane_block(reg, 0b010, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(lane.current == constants::DEFAULT_LANE);
    CHECK(lane.target == -1);
    CHECK(lane.lerp_t == 1.0f);
    CHECK(transform.position.x == constants::LANE_X[constants::DEFAULT_LANE]);
    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK(reg.all_of<MissTag>(obs));
}





TEST_CASE("collision: obstacle too far away is ignored", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Place obstacle far above player
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 200.0f);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: beat line is collision point",
          "[collision][issue-305]") {
    auto line_reg = make_registry();
    make_player(line_reg);
    auto line_obs = make_shape_gate(line_reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(line_reg, 0.016f);

    CHECK(line_reg.all_of<ScoredTag>(line_obs));

    auto preline_reg = make_registry();
    make_player(preline_reg);
    auto preline_obs = make_shape_gate(preline_reg, Shape::Circle, constants::PLAYER_Y - 0.1f);

    collision_system(preline_reg, 0.016f);

    CHECK_FALSE(preline_reg.all_of<ScoredTag>(preline_obs));
}

TEST_CASE("collision: already scored obstacles are skipped", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: resolved hit obstacle is not re-tagged after scoring cleanup",
          "[collision][regression][issue860]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<ScoredTag>(obs));

    scoring_system(reg, 0.0f);
    REQUIRE(reg.all_of<ResolvedObstacleTag>(obs));
    REQUIRE_FALSE(reg.all_of<Obstacle>(obs));
    REQUIRE_FALSE(reg.all_of<ScoredTag>(obs));
    REQUIRE_FALSE(reg.all_of<MissTag>(obs));

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

TEST_CASE("collision: visual obstacle leftovers without Obstacle payload are ignored",
          "[collision][regression][issue865]") {
    auto reg = make_registry();
    make_player(reg);

    auto visual_leftover = reg.create();
    reg.emplace<ObstacleTag>(visual_leftover);
    reg.emplace<WorldTransform>(visual_leftover, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<RequiredShape>(visual_leftover, Shape::Triangle);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<ScoredTag>(visual_leftover));
    CHECK_FALSE(reg.all_of<MissTag>(visual_leftover));
}

TEST_CASE("collision: combo gate requires shape AND lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle in lane 1
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<MotionVelocity>(obs, MotionVelocity{{0.0f, song.scroll_speed}});
    reg.emplace<Obstacle>(obs, ObstacleKind::ComboGate, int16_t{200});
    reg.emplace<RequiredShape>(obs, Shape::Circle);
    // Block lanes 0 and 2, leave lane 1 open
    reg.emplace<BlockedLanes>(obs, uint8_t{0b101});
    reg.emplace<DrawSize>(obs, 720.0f, 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{200, 100, 255, 255});

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: combo gate fails with wrong shape", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<MotionVelocity>(obs, MotionVelocity{{0.0f, song.scroll_speed}});
    reg.emplace<Obstacle>(obs, ObstacleKind::ComboGate, int16_t{200});
    reg.emplace<RequiredShape>(obs, Shape::Triangle);  // wrong shape
    reg.emplace<BlockedLanes>(obs, uint8_t{0b101});    // lane 1 open
    reg.emplace<DrawSize>(obs, 720.0f, 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{200, 100, 255, 255});

    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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



TEST_CASE("collision: BAD timing does not adjust window_start", "[collision][rhythm]") {
    // Post-#223: Bad scale = 1.0 → collision_system does NOT adjust window_start.
    // The window_start shortening path (scale < 1.0) only fires for Perfect and Good.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Put player in Active phase
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.graded = false;
    sw.window_timer = 0.0f;
    sw.window_start = song.song_time;

    // press_time anchors grading. Set arrival_time far enough so it's BAD.
    sw.press_time = song.song_time;
    float bad_arrival = song.song_time - song.half_window * 1.2f;

    // Spawn an obstacle at the player's position
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, bad_arrival, bad_arrival - song.lead_time);

    float original_window_start = sw.window_start;
    collision_system(reg, 0.016f);

    // window_start must NOT be adjusted for Bad (scale == 1.0)
    CHECK_THAT(sw.window_start, Catch::Matchers::WithinAbs(original_window_start, 0.0001f));
    // window_timer should remain unchanged by collision_system
    CHECK(sw.window_timer == 0.0f);
    // graded flag must be set
    CHECK(sw.graded);
}

TEST_CASE("collision: Perfect timing shrinks window via window_start adjustment", "[collision][rhythm]") {
    // Post-#223: Perfect scale = 0.50 (< 1.0); collision_system adjusts
    // window_start backward to collapse the remaining Active window to 50%.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.graded = false;
    sw.window_timer = 0.0f;
    sw.window_start = song.song_time;

    // Set press_time to right now so the hit is perfect.
    sw.press_time = song.song_time;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    float original_window_start = sw.window_start;
    collision_system(reg, 0.016f);

    // Perfect scale = 0.50; remaining = window_duration - 0 = window_duration
    // window_start is shifted backward by remaining * (1 - 0.50) = remaining * 0.50
    float remaining = song.window_duration - 0.0f;
    float expected_shift = remaining * 0.50f;
    CHECK_THAT(sw.window_start, Catch::Matchers::WithinAbs(original_window_start - expected_shift, 0.0001f));
    CHECK(sw.window_scale == 0.50f);
    CHECK(sw.graded);
}
