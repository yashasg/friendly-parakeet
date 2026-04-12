#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── player_action: rhythm mode ───────────────────────────────

TEST_CASE("player_action: rhythm mode starts window on button press from Idle", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 5.0f;

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Circle;

    player_action_system(reg, 0.016f);

    CHECK(sw.target_shape == Shape::Circle);
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
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

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Triangle;

    player_action_system(reg, 0.016f);

    auto& sw = reg.get<ShapeWindow>(player);
    float expected_peak = 10.0f + song.morph_duration + song.half_window;
    CHECK_THAT(sw.peak_time, Catch::Matchers::WithinAbs(expected_peak, 0.001f));
}

TEST_CASE("player_action: rhythm mode ignores same shape during Active (spam protection)", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Square;
    sw.window_timer = 0.1f;

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Square;  // Same shape as current

    player_action_system(reg, 0.016f);

    // Same shape re-press: window timer resets for next obstacle
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
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
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Square;
    sw.window_timer = 0.3f;

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Triangle;  // Different from current

    player_action_system(reg, 0.016f);

    CHECK(sw.target_shape == Shape::Triangle);
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
    CHECK(sw.window_timer == 0.0f);
    CHECK(ps.morph_t == 0.0f);
    CHECK(sw.window_start == 8.0f);
    CHECK(sw.graded == false);
}

TEST_CASE("player_action: rhythm mode ignores Hexagon button press", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Hexagon;

    player_action_system(reg, 0.016f);

    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
}

TEST_CASE("player_action: rhythm mode no action when button not pressed", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = false;

    player_action_system(reg, 0.016f);

    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
}

TEST_CASE("player_action: rhythm mode does not interrupt MorphIn phase", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    sw.target_shape = Shape::Circle;
    sw.window_timer = 0.05f;

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Square;

    player_action_system(reg, 0.016f);

    // MorphIn should not be interrupted (only Active can be interrupted)
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
    CHECK(sw.target_shape == Shape::Circle);
}

TEST_CASE("player_action: rhythm mode does not interrupt MorphOut phase", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
    sw.window_timer = 0.02f;

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Triangle;

    player_action_system(reg, 0.016f);

    // MorphOut should not be interrupted
    CHECK(sw.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));
}

// ── player_action: legacy mode (no SongState) ────────────────

TEST_CASE("player_action: legacy mode instant shape change", "[player_legacy]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Triangle;

    player_action_system(reg, 0.016f);

    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Triangle);
    CHECK(ps.previous == Shape::Circle);
    CHECK(ps.morph_t == 0.0f);
    CHECK(reg.ctx().get<AudioQueue>().count > 0);
}

TEST_CASE("player_action: legacy mode no change for same shape", "[player_legacy]") {
    auto reg = make_registry();
    auto player = make_player(reg);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Circle;  // same as default

    player_action_system(reg, 0.016f);

    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Circle);
    CHECK(reg.ctx().get<AudioQueue>().count == 0);
}

// ── player_action: swipe actions still work in rhythm mode ───

TEST_CASE("player_action: swipe left works in rhythm mode", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = SwipeGesture::SwipeLeft;

    player_action_system(reg, 0.016f);

    CHECK(reg.get<Lane>(player).target == 0);
}

TEST_CASE("player_action: jump works in rhythm mode", "[player_rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);

    auto& gesture = reg.ctx().get<GestureResult>();
    gesture.gesture = SwipeGesture::SwipeUp;

    player_action_system(reg, 0.016f);

    CHECK(reg.get<VerticalState>(player).mode == VMode::Jumping);
}
