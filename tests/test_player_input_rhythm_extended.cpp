#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── player_input_system: rhythm mode window management ───────

TEST_CASE("player_input_rhythm: shape press in Idle begins MorphIn", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    CHECK(sw.phase == WindowPhase::Idle);

    auto btn = make_shape_button(reg, Shape::Circle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

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
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    // Should restart as MorphIn for Square
    CHECK(sw.phase == WindowPhase::MorphIn);
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
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

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
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    auto& audio = reg.ctx().get<AudioQueue>();
    CHECK(audio.count > 0);
    CHECK(audio.queue[0] == SFX::ShapeShift);
}

TEST_CASE("player_input_rhythm: lane change works in rhythm mode", "[player][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});

    player_input_system(reg, 0.016f);

    auto& lane = reg.get<Lane>(player);
    CHECK(lane.target == 0);
}

TEST_CASE("player_input: non-rhythm shape press changes immediately", "[player]") {
    auto reg = make_registry();
    // No SongState → freeplay mode
    make_player(reg);

    auto btn = make_shape_button(reg, Shape::Square);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        CHECK(ps.current == Shape::Square);
        CHECK(ps.previous == Shape::Circle);
        CHECK(ps.morph_t == 0.0f);
    }
}

TEST_CASE("player_input: non-rhythm same shape press does nothing", "[player]") {
    auto reg = make_registry();
    make_player(reg);
    // Player starts as Circle

    auto btn = make_shape_button(reg, Shape::Circle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [e, ps] : view.each()) {
        CHECK(ps.morph_t == 1.0f);  // unchanged
    }
    CHECK(reg.ctx().get<AudioQueue>().count == 0);
}

TEST_CASE("player_input: non-rhythm shape press updates Color", "[player]") {
    auto reg = make_registry();
    auto p = make_player(reg);

    auto btn = make_shape_button(reg, Shape::Square);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    auto& col = reg.get<Color>(p);
    // Square color: { 255, 100, 100, 255 }
    CHECK(col.r == 255);
    CHECK(col.g == 100);
    CHECK(col.b == 100);
}

