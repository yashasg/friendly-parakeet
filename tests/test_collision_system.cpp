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
    CHECK_FALSE(is_phase_transition_pending(reg));
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

    CHECK_FALSE(is_phase_transition_pending(reg));
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: shape gate collision uses authored lane, not visual hitbox edge",
          "[collision][edge][issue1036]") {
    constexpr struct {
        float offset;
        bool missed;
    } cases[] = {{constants::PLAYER_SIZE, false}, {constants::PLAYER_SIZE + 0.01f, false}};

    for (const auto& tc : cases) {
        auto reg = make_registry();
        auto player = make_player(reg);
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

        reg.get<WorldPosition>(obs).position.x = reg.get<WorldPosition>(player).position.x + tc.offset;
        collision_system(reg, 0.016f);

        CHECK(reg.all_of<ScoredTag>(obs));
        CHECK(reg.all_of<MissTag>(obs) == tc.missed);
    }
}

TEST_CASE("collision: shape gate does not clear from target lane before strafe arrives",
          "[collision][issue892]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    reg.get<WorldPosition>(obs).position.x = constants::LANE_X[0];
    reg.get<int8_t>(obs) = int8_t{0};
    reg.emplace<LaneTransition>(player, LaneTransition{0, 0.0f});

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK(reg.all_of<MissTag>(obs));
}

TEST_CASE("collision: shape gate ignores visual lane drift from authored lane",
          "[collision][issue874]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    reg.get<WorldPosition>(obs).position.x = constants::LANE_X[0];

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

TEST_CASE("collision: shape gate clears when interpolated player hitbox reaches lane",
          "[collision][issue892]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    reg.get<WorldPosition>(obs).position.x = constants::LANE_X[0];
    reg.get<int8_t>(obs) = int8_t{0};
    reg.get<WorldPosition>(player).position.x = constants::LANE_X[0];
    reg.emplace<LaneTransition>(player, LaneTransition{0, 1.0f});

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

TEST_CASE("collision: split path uses interpolated player lane during transitions",
          "[collision][issue949]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& transform = reg.get<WorldPosition>(player);
    lane.current = 1;
    auto& transition =
        reg.emplace<LaneTransition>(player, LaneTransition{2, 0.9f});
    transform.position.x = constants::LANE_X[1] +
        (constants::LANE_X[2] - constants::LANE_X[1]) * transition.lerp_t;
    auto obs = make_split_path(reg, Shape::Circle, 2, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}





TEST_CASE("collision: obstacle too far away is ignored", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Place obstacle far above player
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 200.0f);

    collision_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
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

    CHECK_FALSE(is_phase_transition_pending(reg));
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
    reg.emplace<WorldPosition>(visual_leftover, WorldPosition{{constants::LANE_X[1], constants::PLAYER_Y}});
    set_required_shape_tag(reg, visual_leftover, Shape::Triangle);

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<ScoredTag>(visual_leftover));
    CHECK_FALSE(reg.all_of<MissTag>(visual_leftover));
}

TEST_CASE("collision: split path cleared with matching shape and lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle in lane 1
    auto obs = make_split_path(reg, Shape::Circle, 1, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("collision: split path fails with wrong shape", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle, requires Triangle
    make_split_path(reg, Shape::Triangle, 1, constants::PLAYER_Y);

    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
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

    CHECK_FALSE(is_phase_transition_pending(reg));
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: no player means no collision processing", "[collision]") {
    auto reg = make_registry();
    // No player, just an obstacle at collision range
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("collision: not in Playing phase skips processing", "[collision]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseGameOverTag>(reg);
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
}



TEST_CASE("collision: BAD timing does not adjust window_start", "[collision][rhythm]") {
    // Post-#223: Bad scale = 1.0 → collision_system does NOT adjust window_start.
    // The window_start shortening path (scale < 1.0) only fires for Perfect and Good.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Put player in Active phase
    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);
    sw.window_timer = 0.0f;
    sw.window_start = song.song_time;

    // Pressed presence anchors grading. Set arrival_time far enough so it's BAD.
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time});
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
    CHECK(reg.all_of<WindowGraded>(player));
}

TEST_CASE("collision: Perfect timing shrinks window via window_start adjustment", "[collision][rhythm]") {
    // Post-#223: Perfect scale = 0.50 (< 1.0); collision_system adjusts
    // window_start backward to collapse the remaining Active window to 50%.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);
    sw.window_timer = 0.0f;
    sw.window_start = song.song_time;

    // Pressed at song_time so the hit is perfect.
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time});

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    float original_window_start = sw.window_start;
    collision_system(reg, 0.016f);

    // Perfect scale = 0.50; remaining = window_duration - 0 = window_duration
    // window_start is shifted backward by remaining * (1 - 0.50) = remaining * 0.50
    float remaining = song.window_duration - 0.0f;
    float expected_shift = remaining * 0.50f;
    CHECK_THAT(sw.window_start, Catch::Matchers::WithinAbs(original_window_start - expected_shift, 0.0001f));
    CHECK(reg.all_of<WindowGraded>(player));
}
