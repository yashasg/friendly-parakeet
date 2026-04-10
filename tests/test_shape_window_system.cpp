#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── shape_window_system: Idle phase ──────────────────────────

TEST_CASE("shape_window: Idle phase does nothing", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));

    shape_window_system(reg, 0.1f);

    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
    CHECK(ps.window_timer == 0.0f);
}

// ── shape_window_system: MorphIn phase ───────────────────────

TEST_CASE("shape_window: MorphIn increments timer", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.target_shape = Shape::Circle;
    ps.window_timer = 0.0f;
    ps.morph_t = 0.0f;

    shape_window_system(reg, 0.02f);

    CHECK(ps.window_timer > 0.0f);
    CHECK(ps.morph_t > 0.0f);
}

TEST_CASE("shape_window: MorphIn completes and transitions to Active", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.target_shape = Shape::Triangle;
    ps.previous = Shape::Hexagon;
    ps.window_timer = 0.0f;
    ps.morph_t = 0.0f;

    // Advance past morph duration
    shape_window_system(reg, song.morph_duration + 0.01f);

    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
    CHECK(ps.morph_t == 1.0f);
    CHECK(ps.window_timer == 0.0f);  // reset for Active phase
    CHECK(ps.current == Shape::Triangle);
}

TEST_CASE("shape_window: MorphIn morph_t proportional to timer", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.target_shape = Shape::Square;
    ps.window_timer = 0.0f;
    ps.morph_t = 0.0f;

    float half_morph = song.morph_duration / 2.0f;
    shape_window_system(reg, half_morph);

    CHECK_THAT(ps.morph_t, Catch::Matchers::WithinAbs(0.5f, 0.05f));
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
}

// ── shape_window_system: Active phase ────────────────────────

TEST_CASE("shape_window: Active increments timer", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Circle;
    ps.window_timer = 0.0f;

    shape_window_system(reg, 0.1f);

    CHECK(ps.window_timer > 0.0f);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
}

TEST_CASE("shape_window: Active transitions to MorphOut when window expires", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Square;
    ps.window_timer = 0.0f;

    // Advance past window_duration
    shape_window_system(reg, song.window_duration + 0.01f);

    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));
    CHECK(ps.window_timer == 0.0f);
    CHECK(ps.previous == Shape::Square);
    CHECK(ps.morph_t == 0.0f);
}

// ── shape_window_system: MorphOut phase ──────────────────────

TEST_CASE("shape_window: MorphOut increments timer and morph_t", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
    ps.previous = Shape::Circle;
    ps.window_timer = 0.0f;
    ps.morph_t = 0.0f;

    shape_window_system(reg, 0.02f);

    CHECK(ps.window_timer > 0.0f);
    CHECK(ps.morph_t > 0.0f);
}

TEST_CASE("shape_window: MorphOut completes and returns to Idle/Hexagon", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
    ps.previous = Shape::Triangle;
    ps.current = Shape::Triangle;
    ps.target_shape = Shape::Triangle;
    ps.window_timer = 0.0f;
    ps.morph_t = 0.0f;

    // Advance past morph duration
    shape_window_system(reg, song.morph_duration + 0.01f);

    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
    CHECK(ps.morph_t == 1.0f);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(ps.previous == Shape::Hexagon);
    CHECK(ps.target_shape == Shape::Hexagon);
    CHECK(ps.window_timer == 0.0f);
}

// ── shape_window_system: Full cycle ──────────────────────────

TEST_CASE("shape_window: full cycle MorphIn -> Active -> MorphOut -> Idle", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    // Start in MorphIn
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.target_shape = Shape::Circle;
    ps.previous = Shape::Hexagon;
    ps.window_timer = 0.0f;
    ps.morph_t = 0.0f;

    // Complete MorphIn
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
    CHECK(ps.current == Shape::Circle);

    // Complete Active
    shape_window_system(reg, song.window_duration + 0.01f);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));

    // Complete MorphOut
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
    CHECK(ps.current == Shape::Hexagon);
}

// ── shape_window_system: phase guard ─────────────────────────

TEST_CASE("shape_window: no processing when not Playing", "[shape_window]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Paused;
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.window_timer = 0.0f;

    shape_window_system(reg, 0.1f);

    CHECK(ps.window_timer == 0.0f);
}

TEST_CASE("shape_window: no processing when SongState absent", "[shape_window]") {
    auto reg = make_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.window_timer = 0.0f;

    shape_window_system(reg, 0.1f);

    CHECK(ps.window_timer == 0.0f);
}

// ── shape_window_system: no player ───────────────────────────

TEST_CASE("shape_window: no crash without player entity", "[shape_window]") {
    auto reg = make_rhythm_registry();
    // No player created
    shape_window_system(reg, 0.1f);
    SUCCEED("No crash with empty player view");
}

TEST_CASE("shape_window: Active stays Active if time not yet expired", "[shape_window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Square;
    ps.window_timer = 0.0f;

    float half_window = song.window_duration / 2.0f;
    shape_window_system(reg, half_window);

    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
    CHECK_THAT(ps.window_timer, Catch::Matchers::WithinAbs(half_window, 0.001f));
}
