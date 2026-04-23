#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── shape_window_system: full lifecycle ──────────────────────

TEST_CASE("shape_window: MorphIn advances morph_t toward 1.0", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    sw.target_shape = Shape::Circle;
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    // Advance song_time by half morph_duration
    song.song_time += song.morph_duration / 2.0f;
    shape_window_system(reg, 0.016f);

    CHECK(ps.morph_t > 0.0f);
    CHECK(ps.morph_t < 1.0f);
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
}

TEST_CASE("shape_window: MorphIn transitions to Active", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    sw.target_shape = Shape::Circle;
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    // Advance past morph_duration
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(ps.morph_t == 1.0f);
    CHECK(ps.current == Shape::Circle);
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
}

TEST_CASE("shape_window: Active phase expires into MorphOut", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    sw.window_start = song.song_time;
    sw.window_scale = 1.0f;

    // Advance past window_duration
    song.song_time += song.window_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));
    CHECK(ps.morph_t == 0.0f);
}

TEST_CASE("shape_window: MorphOut returns to Idle with Hexagon", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    ps.previous = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    // Advance past morph_duration
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
    CHECK(ps.current == Shape::Hexagon);
    CHECK(ps.previous == Shape::Hexagon);
    CHECK(sw.target_shape == Shape::Hexagon);
    CHECK(sw.window_scale == 1.0f);
    CHECK_FALSE(sw.graded);
}

TEST_CASE("shape_window: Idle phase does nothing", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Idle);
    Shape original_shape = ps.current;

    song.song_time += 1.0f;
    shape_window_system(reg, 0.016f);

    CHECK(ps.current == original_shape);
}

TEST_CASE("shape_window: window_scale > 1 extends Active duration", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    sw.window_start = song.song_time;
    sw.window_scale = 1.50f;  // Perfect extends to 1.5x duration

    // Advance by normal window_duration — should still be Active
    song.song_time += song.window_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Active));

    // Now advance past the extended duration
    song.song_time += song.window_duration * 0.6f;
    shape_window_system(reg, 0.016f);

    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));
}

TEST_CASE("shape_window: not in Playing phase skips processing", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    reg.ctx().get<GameState>().phase = GamePhase::Paused;

    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    sw.window_start = song.song_time;

    song.song_time += song.morph_duration + 0.1f;
    shape_window_system(reg, 0.016f);

    // Should still be MorphIn (paused)
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
}

TEST_CASE("shape_window: color updates on Active transition", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    sw.target_shape = Shape::Square;  // Red
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    auto& col = reg.get<Color>(player);
    // Square is { 255, 100, 100, 255 }
    CHECK(col.r == 255);
    CHECK(col.g == 100);
}

TEST_CASE("shape_window: full lifecycle MorphIn→Active→MorphOut→Idle", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Start MorphIn
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    sw.target_shape = Shape::Triangle;
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    // Phase 1: MorphIn → Active
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
    CHECK(ps.current == Shape::Triangle);

    // Phase 2: Active → MorphOut
    song.song_time += song.window_duration + 0.01f;
    shape_window_system(reg, 0.016f);
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));

    // Phase 3: MorphOut → Idle
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
    CHECK(ps.current == Shape::Hexagon);
}

