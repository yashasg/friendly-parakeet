#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── semantic input pipeline: rhythm mode window management ───────

TEST_CASE("player_input_rhythm: shape press in Idle begins MorphIn window", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    CHECK(sw.phase == WindowPhase::Idle);

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Circle);
    CHECK(sw.window_start == song.song_time);
}

TEST_CASE("player_input_rhythm: different shape in Active restarts window", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Already Active as Circle
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.window_start = song.song_time - 0.5f;

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    // Should restart as MorphIn for Square.
    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Square);
}

TEST_CASE("player_input_rhythm: same shape in Active is no-op", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.window_start = song.song_time - 0.5f;
    sw.window_timer = 0.5f;
    sw.press_time = song.song_time - 0.5f;
    sw.peak_time = song.song_time - 0.25f;
    sw.graded = true;  // was previously graded

    const float initial_window_start = sw.window_start;
    const float initial_window_timer = sw.window_timer;
    const float initial_press_time = sw.press_time;
    const float initial_peak_time = sw.peak_time;

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    CHECK(sw.phase == WindowPhase::Active);
    CHECK(sw.window_start == initial_window_start);
    CHECK(sw.window_timer == initial_window_timer);
    CHECK(sw.press_time == initial_press_time);
    CHECK(sw.peak_time == initial_peak_time);
    CHECK(sw.graded);
}

TEST_CASE("player_input_rhythm: shape change pushes ShapeShift SFX", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    auto sfx_cap = drain_sfx_events(reg);
    CHECK(sfx_cap.count > 0);
    CHECK(sfx_cap.buf[0] == SFX::ShapeShift);
}

TEST_CASE("player_input_rhythm: shape press does not target authored motif lane", "[player][rhythm][issue874]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);
    lane.target = lane.current;

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    CHECK(lane.target == 1);
}

TEST_CASE("player_input_rhythm: shape press does not set stale target when already in authored motif lane",
          "[player][rhythm][issue531]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    lane.current = 0;
    lane.target = -1;

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    CHECK(lane.target == -1);
}

TEST_CASE("player_input_rhythm: shape press preserves in-flight lane target",
          "[player][rhythm][issue874]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    lane.current = 1;
    lane.target = 2;
    lane.lerp_t = 0.5f;

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    CHECK(lane.target == 2);
    CHECK(lane.lerp_t == 0.5f);
}

TEST_CASE("player_input_rhythm: lane change works in rhythm mode", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});

    run_semantic_input_tick(reg);

    auto& lane = reg.get<Lane>(player);
    CHECK(lane.target == 0);
}

TEST_CASE("player_input: non-rhythm shape press changes immediately", "[player]") {
    auto reg = make_registry();
    // No SongState → freeplay mode
    auto player = make_player(reg);

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        CHECK(ps.current == Shape::Square);
        CHECK(ps.morph_t == 1.0f);
    }

    auto& lane = reg.get<Lane>(player);
    CHECK(lane.target == -1);
}

TEST_CASE("player_input: non-rhythm same shape press does nothing", "[player]") {
    auto reg = make_registry();
    make_player(reg);
    // Player starts as Circle

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        CHECK(ps.morph_t == 1.0f);  // unchanged
    }
    CHECK(drain_sfx_events(reg).count == 0);
}

TEST_CASE("player_input: non-rhythm shape press updates Color", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    auto& col = reg.get<Color>(p);
    // Square color: { 255, 100, 100, 255 }
    CHECK(col.r == 255);
    CHECK(col.g == 100);
    CHECK(col.b == 100);
}


TEST_CASE("player_input_rhythm: circle press in lane 0 does not force mismatch before collision",
          "[player][rhythm][issue531]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& transform = reg.get<WorldTransform>(player);

    lane.current = 0;
    lane.target = -1;
    lane.lerp_t = 1.0f;
    transform.position.x = constants::LANE_X[0];

    auto& ps = reg.get<PlayerShape>(player);
    ps.current = Shape::Hexagon;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.get<WorldTransform>(obs).position.x = constants::LANE_X[0];
    reg.get<ShapeGateLane>(obs).lane = int8_t{0};

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    reg.ctx().get<entt::dispatcher>().update<ButtonPressEvent>();
    auto& song = reg.ctx().get<SongState>();
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    player_movement_system(reg, 0.016f);
    collision_system(reg, 0.016f);

    CHECK(lane.target == -1);
    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}
