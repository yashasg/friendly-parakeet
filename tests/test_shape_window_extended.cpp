#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "util/rhythm_math.h"

// ── shape_window_system: full lifecycle ──────────────────────

TEST_CASE("shape_window: MorphIn advances morph_t toward 1.0", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Circle);
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    // Advance song_time by half morph_duration
    song.song_time += song.morph_duration / 2.0f;
    shape_window_system(reg, 0.016f);

    CHECK(ps.morph_t > 0.0f);
    CHECK(ps.morph_t < 1.0f);
    CHECK(window_phase_is_morph_in(reg, player));
}

TEST_CASE("shape_window: MorphIn transitions to Active", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Circle);
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    // Advance past morph_duration
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(ps.morph_t == 1.0f);
    CHECK(reg.all_of<ShapeCircleTag>(player));
    CHECK(window_phase_is_active(reg, player));
}

TEST_CASE("shape_window: Active phase expires into MorphOut", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    sw.window_start = song.song_time;

    // Advance past window_duration
    song.song_time += song.window_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(window_phase_is_morph_out(reg, player));
    CHECK(ps.morph_t == 0.0f);
}

TEST_CASE("shape_window: MorphOut returns to Idle with Hexagon", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_morph_out(reg, player);
    sw.window_start = song.song_time;
    sw.press_time = song.song_time - 0.25f;
    ps.morph_t = 0.0f;

    // Advance past morph_duration
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(window_phase_is_idle(reg, player));
    CHECK(reg.all_of<ShapeHexagonTag>(player));
    CHECK(reg.all_of<TargetShapeHexagonTag>(player));
    CHECK(sw.press_time == -1.0f);
    CHECK_FALSE(sw.graded);
}

TEST_CASE("shape_window: Idle phase does nothing", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_idle(reg, player);
    Shape original_shape = current_player_shape(reg, player);

    song.song_time += 1.0f;
    shape_window_system(reg, 0.016f);

    CHECK(current_player_shape(reg, player) == original_shape);
}

TEST_CASE("shape_window: Active duration is driven by song window duration", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    sw.window_start = song.song_time;

    // collision_system shortens the active window by shifting window_start;
    // shape_window_system itself derives Active duration purely from
    // song_time vs window_start + window_duration.
    song.song_time += song.window_duration + 0.01f;
    shape_window_system(reg, 0.016f);

    CHECK(window_phase_is_morph_out(reg, player));
}

TEST_CASE("shape_window: not in Playing phase skips processing", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    set_test_phase<GamePhasePausedTag>(reg);

    set_window_phase_morph_in(reg, player);
    sw.window_start = song.song_time;

    song.song_time += song.morph_duration + 0.1f;
    tick_playing_systems(reg, 0.016f);

    // Runner skips all systems when not Playing — MorphIn must not advance.
    CHECK(window_phase_is_morph_in(reg, player));
}

TEST_CASE("shape_window: color updates on Active transition", "[shape_window][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Square);  // Red
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
    set_window_phase_morph_in(reg, player);
    set_target_shape_tag(reg, player, Shape::Triangle);
    sw.window_start = song.song_time;
    ps.morph_t = 0.0f;

    // Phase 1: MorphIn → Active
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);
    CHECK(window_phase_is_active(reg, player));
    CHECK(reg.all_of<ShapeTriangleTag>(player));

    // Phase 2: Active → MorphOut
    song.song_time += song.window_duration + 0.01f;
    shape_window_system(reg, 0.016f);
    CHECK(window_phase_is_morph_out(reg, player));

    // Phase 3: MorphOut → Idle
    song.song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, 0.016f);
    CHECK(window_phase_is_idle(reg, player));
    CHECK(reg.all_of<ShapeHexagonTag>(player));
}

// ── Regression tests for issue #223 (window_scale inversion) ────────────────
// collision_system shortens the Active window by adjusting window_start backward
// when scale < 1.0: window_start -= remaining * (1.0 - scale).
// These tests drive that path directly so a re-inversion of window_scale_for_tier
// would immediately break them.

TEST_CASE("shape_window regression #223: Perfect scale (0.50) collapses window to <=50%", "[shape_window][rhythm][regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Simulate what collision_system does on a Perfect hit at t=0 of the Active window.
    // At that instant window_timer == 0, remaining == window_duration.
    float scale = window_scale_from_delta_abs(0.0f);  // delta=0 → Perfect band
    CHECK(scale == 0.50f);

    set_window_phase_active(reg, player);
    sw.window_start = song.song_time;

    // Apply the collision_system shortening: shift window_start backward.
    float remaining = song.window_duration;  // hit at window open (timer==0)
    sw.window_start -= remaining * (1.0f - scale);

    // The active window should now expire at (original_window_start + remaining*scale).
    // Advance by 55% of window_duration; should already be in MorphOut.
    song.song_time += song.window_duration * 0.55f;
    shape_window_system(reg, 0.016f);

    CHECK(window_phase_is_morph_out(reg, player));
}

TEST_CASE("shape_window regression #223: Ok scale (1.00) leaves window duration unchanged", "[shape_window][rhythm][regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    float scale = window_scale_from_delta_abs(kTimingGoodSeconds + 0.001f);  // Ok band
    CHECK(scale == 1.00f);

    set_window_phase_active(reg, player);
    sw.window_start = song.song_time;
    // scale == 1.0 → no window_start adjustment, no extension.

    // Advance by 90% of window_duration — should still be Active.
    song.song_time += song.window_duration * 0.90f;
    shape_window_system(reg, 0.016f);
    CHECK(window_phase_is_active(reg, player));

    // Advance past the full window_duration — should transition to MorphOut.
    song.song_time += song.window_duration * 0.15f;
    shape_window_system(reg, 0.016f);
    CHECK(window_phase_is_morph_out(reg, player));
}
