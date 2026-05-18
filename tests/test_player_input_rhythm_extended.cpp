#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── semantic input pipeline: rhythm mode window management ───────

TEST_CASE("player_input_rhythm: shape press in Idle begins MorphIn window", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    CHECK(window_phase_is_idle(reg, player));

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    CHECK(window_phase_is_morph_in(reg, player));
    CHECK(reg.all_of<TargetShapeCircleTag>(player));
    CHECK(sw.window_start == song.song_time);
}

TEST_CASE("player_input_rhythm: different shape in Active restarts window", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Already Active as Circle
    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    sw.window_start = song.song_time - 0.5f;

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    // Should restart as MorphIn for Square.
    CHECK(window_phase_is_morph_in(reg, player));
    CHECK(reg.all_of<TargetShapeSquareTag>(player));
}

TEST_CASE("player_input_rhythm: same shape in Active is no-op", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    sw.window_start = song.song_time - 0.5f;
    sw.window_timer = 0.5f;
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time - 0.5f});
    reg.emplace_or_replace<WindowGraded>(player);  // was previously graded

    const float initial_window_start = sw.window_start;
    const float initial_window_timer = sw.window_timer;
    const float initial_press_time   = reg.get<Pressed>(player).press_time;

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    CHECK(window_phase_is_active(reg, player));
    CHECK(sw.window_start == initial_window_start);
    CHECK(sw.window_timer == initial_window_timer);
    CHECK(reg.get<Pressed>(player).press_time == initial_press_time);
    CHECK(reg.all_of<WindowGraded>(player));
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
    // No LaneTransition row → settled. A shape press must not synthesize one.

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    CHECK_FALSE(reg.all_of<LaneTransition>(player));
    CHECK(lane.current == 1);
}

TEST_CASE("player_input_rhythm: shape press does not set stale target when already in authored motif lane",
          "[player][rhythm][issue531]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    lane.current = 0;
    // No LaneTransition row → settled.

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    CHECK_FALSE(reg.all_of<LaneTransition>(player));
}

TEST_CASE("player_input_rhythm: shape press preserves in-flight lane target",
          "[player][rhythm][issue874]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    lane.current = 1;
    reg.emplace<LaneTransition>(player, LaneTransition{2, 0.5f});

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    REQUIRE(reg.all_of<LaneTransition>(player));
    CHECK(reg.get<LaneTransition>(player).target == 2);
    CHECK(reg.get<LaneTransition>(player).lerp_t == 0.5f);
}

TEST_CASE("player_input_rhythm: lane change works in rhythm mode", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoLeftEvent>({});

    run_semantic_input_tick(reg);

    REQUIRE(reg.all_of<LaneTransition>(player));
    CHECK(reg.get<LaneTransition>(player).target == 0);
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
        CHECK(reg.all_of<ShapeSquareTag>(e));
        CHECK(ps.morph_t == 1.0f);
    }

    CHECK_FALSE(reg.all_of<LaneTransition>(player));
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
    auto& transform = reg.get<WorldPosition>(player);

    lane.current = 0;
    // No LaneTransition row → settled.
    transform.position.x = constants::LANE_X[0];

    set_player_shape_tag(reg, player, Shape::Hexagon);

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.get<WorldPosition>(obs).position.x = constants::LANE_X[0];
    reg.get<int8_t>(obs) = int8_t{0};

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    update_press_events(reg);
    auto& song = reg.ctx().get<SongState>();
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    player_movement_system(reg, 0.016f);
    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<LaneTransition>(player));
    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}
