#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── semantic input pipeline: rhythm mode ───────────────────────────────

TEST_CASE("player_action: rhythm mode starts MorphIn on button press from Idle", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 5.0f;

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    CHECK(sw.target_shape == Shape::Triangle);
    CHECK(window_phase_is_morph_in(reg, player));
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(ps.morph_t == 0.0f);
    CHECK(sw.window_start == 5.0f);
    CHECK(sw.graded == false);

    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(window_phase_is_active(reg, player));
    CHECK(ps.current == Shape::Triangle);
    CHECK(ps.morph_t == 1.0f);

    // SFX should be pushed
    CHECK(drain_sfx_events(reg).count > 0);
}

TEST_CASE("player_action: post-song rhythm context still starts window for trailing obstacles",
          "[player_rhythm][issue866]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = song.duration_sec + 0.25f;
    song.playing = false;
    song.finished = true;

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    CHECK(sw.target_shape == Shape::Square);
    CHECK(window_phase_is_morph_in(reg, player));
    CHECK(sw.window_start == song.song_time);
    CHECK(sw.press_time == song.song_time);
}

TEST_CASE("player_action: rhythm mode treats same shape during Active as no-op", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    set_window_phase_active(reg, player);
    ps.current = Shape::Square;
    sw.window_timer = 0.1f;
    sw.window_start = 1.0f;
    sw.press_time = 1.0f;
    sw.graded = true;

    const float initial_window_start = sw.window_start;
    const float initial_window_timer = sw.window_timer;
    const float initial_press_time = sw.press_time;

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    CHECK(window_phase_is_active(reg, player));
    CHECK(sw.window_start == initial_window_start);
    CHECK(sw.window_timer == initial_window_timer);
    CHECK(sw.press_time == initial_press_time);
    CHECK(sw.graded);
}

TEST_CASE("player_action: rhythm mode interrupts Active with different shape", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 8.0f;
    set_window_phase_active(reg, player);
    ps.current = Shape::Square;
    sw.window_timer = 0.3f;

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    CHECK(sw.target_shape == Shape::Triangle);
    CHECK(window_phase_is_morph_in(reg, player));
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.morph_t == 0.0f);
    CHECK(sw.window_start == 8.0f);
    CHECK(sw.graded == false);
}

TEST_CASE("player_action: rhythm mode no action when no tap in queue", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    // No ButtonPressEvent in queue
    run_semantic_input_tick(reg, 0.016f);

    CHECK(window_phase_is_idle(reg, player));
}

TEST_CASE("player_action: rhythm mode does not interrupt MorphIn phase", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    set_window_phase_morph_in(reg, player);
    sw.target_shape = Shape::Circle;
    sw.window_timer = 0.05f;

    auto btn = make_shape_button(reg, Shape::Square);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    // MorphIn should not be interrupted (only Active can be interrupted)
    CHECK(window_phase_is_morph_in(reg, player));
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
    set_window_phase_morph_out(reg, player);
    ps.current = Shape::Circle;
    sw.window_timer = 0.05f;
    sw.window_start = 14.9f;

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    // MorphOut interrupted: new MorphIn window started for Triangle
    CHECK(sw.target_shape == Shape::Triangle);
    CHECK(window_phase_is_morph_in(reg, player));
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.morph_t == 0.0f);
    CHECK(sw.window_start == 15.0f);
    CHECK(sw.graded == false);
    CHECK(drain_sfx_events(reg).count > 0);
}

// ── semantic input pipeline: legacy mode (no SongState) ────────────────

TEST_CASE("player_action: legacy mode instant shape change", "[player_legacy]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Triangle);
    CHECK(ps.morph_t == 1.0f);
    CHECK(drain_sfx_events(reg).count > 0);
}

TEST_CASE("player_action: legacy mode no change for same shape", "[player_legacy]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);

    run_semantic_input_tick(reg, 0.016f);

    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Circle);
    CHECK(drain_sfx_events(reg).count == 0);
}

// ── semantic input pipeline: swipe actions still work in rhythm mode ───

TEST_CASE("player_action: swipe left works in rhythm mode", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});

    run_semantic_input_tick(reg, 0.016f);

    CHECK(reg.get<Lane>(player).target == 0);
}

TEST_CASE("player_action: jump works in rhythm mode", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Up});

    run_semantic_input_tick(reg, 0.016f);

    CHECK(reg.get<VerticalState>(player).mode == VMode::Jumping);
}

// ── #213: GoEvents consumed exactly once per frame ────────────────

TEST_CASE("player_input: GoEvents consumed after first tick, lane lerp_t not reset (#213)", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);

    REQUIRE(lane.current == 1);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});

    // First tick: starts lane transition, consumes the event.
    run_semantic_input_tick(reg, 1.0f / 60.0f);
    CHECK(lane.target == 0);
    CHECK(lane.lerp_t == 0.0f);

    // Simulate partial lerp progress (as player_movement_system would do).
    lane.lerp_t = 0.3f;

    // Second tick: GoEvent must NOT be replayed — lerp_t must not reset.
    run_semantic_input_tick(reg, 1.0f / 60.0f);
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
    press_button(reg, btn);

    // First tick: starts a MorphIn window, consumes the event.
    run_semantic_input_tick(reg, 1.0f / 60.0f);
    CHECK(window_phase_is_morph_in(reg, player));

    // Record state after first tick.
    float start_after_tick1 = sw.window_start;

    // Advance song time so a second begin_shape_window call would produce a different window_start.
    song.song_time = 6.0f;

    // Second tick: ButtonPressEvent must NOT be replayed — window_start must not change.
    run_semantic_input_tick(reg, 1.0f / 60.0f);
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

TEST_CASE("player_movement: post-song rhythm context still leaves morph_t to shape_window_system",
          "[player][issue866]") {
    auto reg = make_rhythm_registry();
    auto p = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();
    auto& ps = reg.get<PlayerShape>(p);
    song.playing = false;
    song.finished = true;
    ps.morph_t = 0.3f;

    player_movement_system(reg, 0.016f);

    CHECK(ps.morph_t == 0.3f);
}

TEST_CASE("player_movement: morph_t IS advanced in freeplay mode (#207 regression)", "[player]") {
    auto reg = make_registry();  // SongState present but playing = false
    auto p = make_player(reg);
    auto& ps = reg.get<PlayerShape>(p);
    ps.morph_t = 0.3f;

    player_movement_system(reg, 0.016f);

    CHECK(ps.morph_t > 0.3f);  // freeplay: player_movement_system owns morph_t
}
