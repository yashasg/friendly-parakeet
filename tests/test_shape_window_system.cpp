#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

#include <cmath>

// ── shape_window_system: Idle phase ──────────────────────────

TEST_CASE("shape_window: Idle phase does nothing", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    CHECK(window_phase_is_idle(reg, player));

    reg.ctx().get<SongState>().song_time += 0.1f;
    shape_window_system(reg, 0.1f);

    CHECK(window_phase_is_idle(reg, player));
    CHECK(sw.window_timer == 0.0f);
}

// ── shape_window_system: MorphIn phase ───────────────────────

TEST_CASE("shape_window: MorphIn increments timer", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Circle);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    ps.morph_t = 0.0f;

    reg.ctx().get<SongState>().song_time += 0.02f;
    shape_window_system(reg, 0.02f);

    CHECK(sw.window_timer > 0.0f);
    CHECK(ps.morph_t > 0.0f);
}

TEST_CASE("shape_window: MorphIn completes and transitions to Active", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Triangle);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    ps.morph_t = 0.0f;

    // Advance past morph duration
    reg.ctx().get<SongState>().song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);

    CHECK(window_phase_is_active(reg, player));
    CHECK(ps.morph_t == 1.0f);
    CHECK(sw.window_timer == 0.0f);  // reset for Active phase
    CHECK(reg.all_of<ShapeTriangleTag>(player));
}

TEST_CASE("shape_window: MorphIn morph_t proportional to timer", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Square);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    ps.morph_t = 0.0f;

    float half_morph = song.morph_duration / 2.0f;
    reg.ctx().get<SongState>().song_time += half_morph;
    shape_window_system(reg, half_morph);

    CHECK_THAT(ps.morph_t, Catch::Matchers::WithinAbs(0.5f, 0.05f));
    CHECK(window_phase_is_morph_in(reg, player));
}

// ── shape_window_system: Active phase ────────────────────────

TEST_CASE("shape_window: Active increments timer", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    set_window_phase_active(reg, player);
    set_player_shape_tag(reg, player, Shape::Circle);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;

    reg.ctx().get<SongState>().song_time += 0.1f;
    shape_window_system(reg, 0.1f);

    CHECK(sw.window_timer > 0.0f);
    CHECK(window_phase_is_active(reg, player));
}

TEST_CASE("shape_window: Active transitions to MorphOut when window expires", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_active(reg, player);
    set_player_shape_tag(reg, player, Shape::Square);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;

    // Advance past window_duration
    reg.ctx().get<SongState>().song_time += song.window_duration + 0.01f;
    shape_window_system(reg, song.window_duration + 0.01f);

    CHECK(window_phase_is_morph_out(reg, player));
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.morph_t == 0.0f);
}

// ── shape_window_system: MorphOut phase ──────────────────────

TEST_CASE("shape_window: MorphOut increments timer and morph_t", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    set_window_phase_morph_out(reg, player);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    ps.morph_t = 0.0f;

    reg.ctx().get<SongState>().song_time += 0.02f;
    shape_window_system(reg, 0.02f);

    CHECK(sw.window_timer > 0.0f);
    CHECK(ps.morph_t > 0.0f);
}

TEST_CASE("shape_window: MorphOut completes and returns to Idle/Hexagon", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_out(reg, player);
    set_player_shape_tag(reg, player, Shape::Triangle);
    set_target_shape_tag(reg, player, Shape::Triangle);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    ps.morph_t = 0.0f;

    // Advance past morph duration
    reg.ctx().get<SongState>().song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);

    CHECK(window_phase_is_idle(reg, player));
    CHECK(ps.morph_t == 1.0f);
    CHECK(reg.all_of<ShapeHexagonTag>(player));
    CHECK(reg.all_of<TargetShapeHexagonTag>(player));
    CHECK(sw.window_timer == 0.0f);
}

// ── shape_window_system: Full cycle ──────────────────────────

TEST_CASE("shape_window: full cycle MorphIn -> Active -> MorphOut -> Idle", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Start in MorphIn
    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Circle);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    ps.morph_t = 0.0f;

    // Complete MorphIn
    reg.ctx().get<SongState>().song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(window_phase_is_active(reg, player));
    CHECK(reg.all_of<ShapeCircleTag>(player));

    // Complete Active
    reg.ctx().get<SongState>().song_time += song.window_duration + 0.01f;
    shape_window_system(reg, song.window_duration + 0.01f);
    CHECK(window_phase_is_morph_out(reg, player));

    // Complete MorphOut
    reg.ctx().get<SongState>().song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(window_phase_is_idle(reg, player));
    CHECK(reg.all_of<ShapeHexagonTag>(player));
}

// ── shape_window_system: phase guard ─────────────────────────

TEST_CASE("shape_window: no processing when not Playing", "[shape_window]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Paused;
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    set_window_phase_morph_in(reg, player);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;

    reg.ctx().get<SongState>().song_time += 0.1f;
    tick_playing_systems(reg, 0.1f);

    CHECK(sw.window_timer == 0.0f);
}

TEST_CASE("shape_window: no processing when SongState absent", "[shape_window]") {
    auto reg = make_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    set_window_phase_morph_in(reg, player);
    sw.window_timer = 0.0f;

    shape_window_system(reg, 0.1f);

    CHECK(sw.window_timer == 0.0f);
}

// ── shape_window_system: no player ───────────────────────────

TEST_CASE("shape_window: no crash without player entity", "[shape_window]") {
    auto reg = make_rhythm_registry();
    // No player created
    reg.ctx().get<SongState>().song_time += 0.1f;
    shape_window_system(reg, 0.1f);
    SUCCEED("No crash with empty player view");
}

TEST_CASE("shape_window: Active stays Active if time not yet expired", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_active(reg, player);
    set_player_shape_tag(reg, player, Shape::Square);
    sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;

    float half_window = song.window_duration / 2.0f;
    reg.ctx().get<SongState>().song_time += half_window;
    shape_window_system(reg, half_window);

    CHECK(window_phase_is_active(reg, player));
    CHECK_THAT(sw.window_timer, Catch::Matchers::WithinAbs(half_window, 0.001f));
}

TEST_CASE("shape_window: invalid morph_duration completes MorphIn without non-finite progress", "[shape_window][issue915]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Square);
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;
    song.morph_duration = 0.0f;

    shape_window_system(reg, 0.016f);

    CHECK(std::isfinite(ps.morph_t));
    CHECK(ps.morph_t == 1.0f);
    CHECK(window_phase_is_active(reg, player));
    CHECK(reg.all_of<ShapeSquareTag>(player));
}

TEST_CASE("shape_window: invalid morph_duration completes MorphOut without non-finite progress", "[shape_window][issue915]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_out(reg, player);
    set_target_shape_tag(reg, player, Shape::Triangle);
    set_player_shape_tag(reg, player, Shape::Triangle);
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;
    song.morph_duration = -0.1f;

    shape_window_system(reg, 0.016f);

    CHECK(std::isfinite(ps.morph_t));
    CHECK(ps.morph_t == 1.0f);
    CHECK(window_phase_is_idle(reg, player));
    CHECK(reg.all_of<ShapeHexagonTag>(player));
}
