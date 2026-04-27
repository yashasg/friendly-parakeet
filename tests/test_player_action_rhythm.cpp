#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── player_input_system: rhythm mode ───────────────────────────────

TEST_CASE("player_action: rhythm mode starts window on button press from Idle", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 5.0f;

    auto btn = make_shape_button(reg, Shape::Circle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    CHECK(sw.target_shape == Shape::Circle);
    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.morph_t == 0.0f);
    CHECK(sw.window_start == 5.0f);
    CHECK(sw.graded == false);
    CHECK(sw.window_scale == 1.0f);

    // SFX should be pushed
    CHECK(reg.ctx().get<AudioQueue>().count > 0);
}

TEST_CASE("player_action: rhythm mode calculates peak_time correctly", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 10.0f;

    auto btn = make_shape_button(reg, Shape::Triangle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    auto& sw = reg.get<ShapeWindow>(player);
    float expected_peak = 10.0f + song.morph_duration + song.half_window;
    CHECK_THAT(sw.peak_time, Catch::Matchers::WithinAbs(expected_peak, 0.001f));
}

TEST_CASE("player_action: rhythm mode ignores same shape during Active (spam protection)", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    sw.phase = WindowPhase::Active;
    ps.current = Shape::Square;
    sw.window_timer = 0.1f;

    auto btn = make_shape_button(reg, Shape::Square);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    // Same shape re-press: window timer resets for next obstacle
    CHECK(sw.phase == WindowPhase::Active);
    CHECK(sw.window_timer == 0.0f);
    CHECK_FALSE(sw.graded);
}

TEST_CASE("player_action: rhythm mode interrupts Active with different shape", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 8.0f;
    sw.phase = WindowPhase::Active;
    ps.current = Shape::Square;
    sw.window_timer = 0.3f;

    auto btn = make_shape_button(reg, Shape::Triangle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    CHECK(sw.target_shape == Shape::Triangle);
    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.morph_t == 0.0f);
    CHECK(sw.window_start == 8.0f);
    CHECK(sw.graded == false);
}

TEST_CASE("player_action: rhythm mode no action when no tap in queue", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);

    // Empty EventQueue
    player_input_system(reg, 0.016f);

    CHECK(sw.phase == WindowPhase::Idle);
}

TEST_CASE("player_action: rhythm mode does not interrupt MorphIn phase", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    sw.phase = WindowPhase::MorphIn;
    sw.target_shape = Shape::Circle;
    sw.window_timer = 0.05f;

    auto btn = make_shape_button(reg, Shape::Square);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    // MorphIn should not be interrupted (only Active can be interrupted)
    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Circle);
}

// Fixed by #209: MorphOut is now interruptible — player input is preserved.
TEST_CASE("player_action: rhythm mode ACCEPTS button press during MorphOut (#209)", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 15.0f;
    sw.phase = WindowPhase::MorphOut;
    ps.current = Shape::Circle;
    ps.previous = Shape::Circle;
    sw.window_timer = 0.05f;
    sw.window_start = 14.9f;

    auto btn = make_shape_button(reg, Shape::Triangle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    // MorphOut interrupted: new MorphIn started for Triangle
    CHECK(sw.target_shape == Shape::Triangle);
    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.morph_t == 0.0f);
    CHECK(sw.window_start == 15.0f);
    CHECK(sw.graded == false);
    CHECK(reg.ctx().get<AudioQueue>().count > 0);
}

// ── player_input_system: legacy mode (no SongState) ────────────────

TEST_CASE("player_action: legacy mode instant shape change", "[player_legacy]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    auto btn = make_shape_button(reg, Shape::Triangle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Triangle);
    CHECK(ps.previous == Shape::Circle);
    CHECK(ps.morph_t == 0.0f);
    CHECK(reg.ctx().get<AudioQueue>().count > 0);
}

TEST_CASE("player_action: legacy mode no change for same shape", "[player_legacy]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    auto btn = make_shape_button(reg, Shape::Circle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Circle);
    CHECK(reg.ctx().get<AudioQueue>().count == 0);
}

// ── player_input_system: swipe actions still work in rhythm mode ───

TEST_CASE("player_action: swipe left works in rhythm mode", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});

    player_input_system(reg, 0.016f);

    CHECK(reg.get<Lane>(player).target == 0);
}

TEST_CASE("player_action: jump disabled in rhythm mode", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Up});

    player_input_system(reg, 0.016f);

    CHECK(reg.get<VerticalState>(player).mode == VMode::Grounded);
}

// ── #213: GoEvents consumed exactly once per frame ────────────────

TEST_CASE("player_input: GoEvents consumed after first tick, lane lerp_t not reset (#213)", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);

    REQUIRE(lane.current == 1);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});

    // First tick: starts lane transition, consumes the event.
    player_input_system(reg, 1.0f / 60.0f);
    CHECK(lane.target == 0);
    CHECK(lane.lerp_t == 0.0f);

    // Simulate partial lerp progress (as player_movement_system would do).
    lane.lerp_t = 0.3f;

    // Second tick: GoEvent must NOT be replayed — lerp_t must not reset.
    player_input_system(reg, 1.0f / 60.0f);
    CHECK(lane.lerp_t == 0.3f);  // unchanged: event was consumed on tick 1
    CHECK(lane.target == 0);
}

TEST_CASE("player_input: ButtonPressEvents consumed after first tick (#213)", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 5.0f;

    auto btn = make_shape_button(reg, Shape::Circle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    // First tick: starts a MorphIn window, consumes the event.
    player_input_system(reg, 1.0f / 60.0f);
    CHECK(sw.phase == WindowPhase::MorphIn);

    // Record state after first tick.
    float start_after_tick1 = sw.window_start;

    // Advance song time so a second begin_shape_window call would produce a different window_start.
    song.song_time = 6.0f;

    // Second tick: ButtonPressEvent must NOT be replayed — window_start must not change.
    player_input_system(reg, 1.0f / 60.0f);
    CHECK(sw.window_start == start_after_tick1);  // unchanged
}

// ── #207: morph_t single-owner rule ──────────────────────────────

TEST_CASE("player_movement: morph_t not advanced in rhythm mode — shape_window_system owns it (#207)", "[player]") {
    auto reg = make_rhythm_registry();  // song.playing = true
    auto p = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.morph_t = 0.3f;

    // player_movement_system must not touch morph_t when in rhythm mode.
    player_movement_system(reg, 0.016f);

    CHECK(ps.morph_t == 0.3f);  // unchanged
}

TEST_CASE("player_movement: morph_t IS advanced in freeplay mode (#207 regression)", "[player]") {
    auto reg = make_registry();  // SongState present but playing = false
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.morph_t = 0.3f;

    player_movement_system(reg, 0.016f);

    CHECK(ps.morph_t > 0.3f);  // freeplay: player_movement_system owns morph_t
}
