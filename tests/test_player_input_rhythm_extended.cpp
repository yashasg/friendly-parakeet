#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── semantic input pipeline: rhythm mode window management ───────

TEST_CASE("player_input_rhythm: shape press in Idle begins Active window", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    CHECK(sw.phase == WindowPhase::Idle);

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    CHECK(sw.phase == WindowPhase::Active);
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

    // Should restart as Active for Square
    CHECK(sw.phase == WindowPhase::Active);
    CHECK(sw.target_shape == Shape::Square);
}

TEST_CASE("player_input_rhythm: same shape in Active re-extends window", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.window_start = song.song_time - 0.5f;
    sw.graded = true;  // was previously graded

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg);

    // Should remain Active and reset timing
    CHECK(sw.phase == WindowPhase::Active);
    CHECK(sw.window_timer == 0.0f);
    CHECK(sw.window_start == song.song_time);
    CHECK_FALSE(sw.graded);
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

TEST_CASE("player_input_rhythm: shape press also remaps lane target", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    CHECK(lane.target == 2);
}

TEST_CASE("player_input_rhythm: shape press does not set stale target when already in mapped lane",
          "[player][rhythm]") {
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

TEST_CASE("player_input_rhythm: shape press cancels conflicting in-flight lane target",
          "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    lane.current = 1;
    lane.target = 2;
    lane.lerp_t = 0.5f;

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);
    run_semantic_input_tick(reg);

    CHECK(lane.target == 1);
    CHECK(lane.lerp_t == 0.0f);
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
        CHECK(ps.previous == Shape::Circle);
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
