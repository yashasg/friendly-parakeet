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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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

TEST_CASE("collision: combo gate requires shape AND lane", "[collision]") {
    auto reg = make_registry();
    make_player(reg);
    // Player is Circle in lane 1
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
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
    reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
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

TEST_CASE("collision: low bar drains energy when sliding", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Sliding;
    make_vertical_bar(reg, ObstacleKind::LowBar, constants::PLAYER_Y);

    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
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

TEST_CASE("collision: lane push left scores and pushes player left", "[collision][lane_push]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Player starts in lane 1 (center); push left moves to lane 0
    auto obs = make_lane_push(reg, ObstacleKind::LanePushLeft, constants::PLAYER_Y);

    // UNIT-SCOPED: self-wires systems to isolate collision + response logic.
    // The production path (system order) is covered by:
    //   "integration: lane push consumed in production tick order"
    collision_system(reg, 0.016f);
    lane_push_response_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    auto& lane = reg.get<Lane>(p);
    CHECK(lane.target == 0);
}

TEST_CASE("collision: lane push right scores and pushes player right", "[collision][lane_push]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Player starts in lane 1 (center); push right moves to lane 2
    auto obs = make_lane_push(reg, ObstacleKind::LanePushRight, constants::PLAYER_Y);

    // UNIT-SCOPED: self-wires systems to isolate collision + response logic.
    // The production path (system order) is covered by:
    //   "integration: lane push consumed in production tick order"
    collision_system(reg, 0.016f);
    lane_push_response_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    auto& lane = reg.get<Lane>(p);
    CHECK(lane.target == 2);
}

TEST_CASE("collision: lane push out of range does not move player off edge", "[collision][lane_push]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Move player to rightmost lane (lane 2); push right would go out of bounds
    reg.get<Lane>(p).current = 2;
    reg.get<WorldTransform>(p).position.x = constants::LANE_X[2];
    auto obs = make_lane_push(reg, ObstacleKind::LanePushRight, constants::PLAYER_Y);

    // UNIT-SCOPED: self-wires systems to isolate collision + response logic.
    // The production path (system order) is covered by:
    //   "integration: lane push consumed in production tick order"
    collision_system(reg, 0.016f);
    lane_push_response_system(reg, 0.016f);

    // Always scored even when push is out of range
    CHECK(reg.all_of<ScoredTag>(obs));
    auto& lane = reg.get<Lane>(p);
    // target stays -1 (no movement initiated)
    CHECK(lane.target < 0);
}

TEST_CASE("collision: lane push too far away is ignored", "[collision][lane_push]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_lane_push(reg, ObstacleKind::LanePushLeft, constants::PLAYER_Y - 200.0f);

    // UNIT-SCOPED: self-wires systems to isolate collision + response logic.
    // The production path (system order) is covered by:
    //   "integration: lane push consumed in production tick order"
    collision_system(reg, 0.016f);
    lane_push_response_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: lane push emplaces PendingLanePush before response system runs", "[collision][lane_push]") {
    // Verifies event-response separation: collision_system emits the event
    // component, lane_push_response_system consumes it.
    // UNIT-SCOPED: self-wires systems to isolate the emit/consume split.
    // The production path (system order) is covered by:
    //   "integration: lane push consumed in production tick order"
    auto reg = make_registry();
    auto p = make_player(reg);
    make_lane_push(reg, ObstacleKind::LanePushLeft, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    // After collision_system: PendingLanePush emplaced with delta=-1
    REQUIRE(reg.all_of<PendingLanePush>(p));
    CHECK(reg.get<PendingLanePush>(p).delta == -1);
    // Lane not yet updated
    CHECK(reg.get<Lane>(p).target < 0);

    lane_push_response_system(reg, 0.016f);

    // After response system: event consumed, lane updated
    CHECK_FALSE(reg.all_of<PendingLanePush>(p));
    CHECK(reg.get<Lane>(p).target == 0);
}

TEST_CASE("collision: lane push right emplaces PendingLanePush with delta +1", "[collision][lane_push]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    make_lane_push(reg, ObstacleKind::LanePushRight, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<PendingLanePush>(p));
    CHECK(reg.get<PendingLanePush>(p).delta == 1);
}

// ── Integration test: production tick path ───────────────────────────────────
// Calls tick_fixed_systems (the real production fixed-step tick) to verify that
// lane_push_response_system is wired between collision_system and
// miss_detection_system in the production loop.  This test CANNOT be satisfied
// by self-wiring the three systems in order — it only passes when
// lane_push_response_system is present in tick_playing_systems (which is called
// from tick_fixed_systems).  Remove it from playing_systems_runner.cpp and this
// test fails; the unit test below would still pass.
TEST_CASE("integration: lane push consumed in production tick order", "[collision][lane_push][integration]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Player starts in lane 1 (center); push left → lane 0
    make_lane_push(reg, ObstacleKind::LanePushLeft, constants::PLAYER_Y);

    // Single call to the real production fixed-step tick.
    // scroll_system runs before collision_system within tick_playing_systems, but
    // COLLISION_MARGIN (40 px) >> scroll displacement (400 px/s × 0.016 s = 6.4 px),
    // so the obstacle remains within the collision window.
    tick_fixed_systems(reg, 0.016f);

    // PendingLanePush must be consumed — proves lane_push_response_system ran
    // between collision_system and miss_detection_system in the production loop.
    CHECK_FALSE(reg.all_of<PendingLanePush>(p));
    // Lane target reflects the push applied by lane_push_response_system.
    CHECK(reg.get<Lane>(p).target == 0);
}

// ── Unit test: canonical three-system ordering ────────────────────────────────
// Directly exercises collision → lane_push_response → miss_detection in isolation.
// Useful for diagnosing which step broke if the integration test above fails,
// but cannot catch production-wiring omissions on its own.
TEST_CASE("unit: lane push consumed when collision → response → miss called in order", "[collision][lane_push]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Player starts in lane 1 (center); push left → lane 0
    make_lane_push(reg, ObstacleKind::LanePushLeft, constants::PLAYER_Y);

    // Self-wired in documented production order — unit scope only.
    collision_system(reg, 0.016f);
    lane_push_response_system(reg, 0.016f);
    miss_detection_system(reg, 0.016f);

    // PendingLanePush must be consumed (not accumulating)
    CHECK_FALSE(reg.all_of<PendingLanePush>(p));
    // Lane target reflects the push
    CHECK(reg.get<Lane>(p).target == 0);
}

// ── Multi-obstacle contention test ────────────────────────────────────────────
// Pins the first-obstacle-wins semantics: when two LanePush obstacles hit in
// the same tick, the second cannot overwrite PendingLanePush.delta.
TEST_CASE("collision: two lane pushes in same tick — first wins, delta not overwritten", "[collision][lane_push]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    // Two obstacles at the same y: one pushes left (delta -1), one pushes right (delta +1).
    // Whichever collision_system processes first sets PendingLanePush; the second is ignored
    // by the !reg.all_of<PendingLanePush> guard.
    make_lane_push(reg, ObstacleKind::LanePushLeft,  constants::PLAYER_Y);
    make_lane_push(reg, ObstacleKind::LanePushRight, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    // Exactly one PendingLanePush — delta is whichever obstacle won (pin, don't assert a value)
    REQUIRE(reg.all_of<PendingLanePush>(p));
    const int8_t winning_delta = reg.get<PendingLanePush>(p).delta;
    CHECK((winning_delta == -1 || winning_delta == 1));  // exactly one of the two

    lane_push_response_system(reg, 0.016f);

    // Consumed exactly once — no residue
    CHECK_FALSE(reg.all_of<PendingLanePush>(p));
    // Lane moved by winning_delta from center (1)
    int expected_lane = 1 + static_cast<int>(winning_delta);
    CHECK(reg.get<Lane>(p).target == expected_lane);
}
