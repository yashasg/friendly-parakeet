#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "util/enum_names.h"
#include "util/rhythm_math.h"

// ── compute_timing_tier ──────────────────────────────────────

TEST_CASE("timing_tier: perfect zone 0.0-0.333", "[timing]") {
    CHECK(compute_timing_tier(0.0f)  == TimingTier::Perfect);
    CHECK(compute_timing_tier(0.333f) == TimingTier::Perfect);
}

TEST_CASE("timing_tier: good zone 0.334-0.666", "[timing]") {
    CHECK(compute_timing_tier(0.34f) == TimingTier::Good);
    CHECK(compute_timing_tier(0.66f) == TimingTier::Good);
}

TEST_CASE("timing_tier: ok zone 0.667-1.0", "[timing]") {
    CHECK(compute_timing_tier(0.67f) == TimingTier::Ok);
    CHECK(compute_timing_tier(1.0f) == TimingTier::Ok);
}

TEST_CASE("timing_tier: bad zone > 1.0", "[timing]") {
    CHECK(compute_timing_tier(1.01f) == TimingTier::Bad);
    CHECK(compute_timing_tier(1.5f)  == TimingTier::Bad);
}

TEST_CASE("timing_tier: boundary at exactly 0.333 is perfect", "[timing]") {
    CHECK(compute_timing_tier(0.333f) == TimingTier::Perfect);
}

TEST_CASE("timing_tier: boundary at exactly 0.666 is good", "[timing]") {
    CHECK(compute_timing_tier(0.666f) == TimingTier::Good);
}

TEST_CASE("timing_tier: boundary at exactly 1.0 is ok", "[timing]") {
    CHECK(compute_timing_tier(1.0f) == TimingTier::Ok);
}

// ── timing_multiplier ────────────────────────────────────────

TEST_CASE("timing_multiplier: Perfect gives 1.50x", "[timing]") {
    CHECK(timing_multiplier(TimingTier::Perfect) == 1.50f);
}

TEST_CASE("timing_multiplier: Good gives 1.00x", "[timing]") {
    CHECK(timing_multiplier(TimingTier::Good) == 1.00f);
}

TEST_CASE("timing_multiplier: Ok gives 0.50x", "[timing]") {
    CHECK(timing_multiplier(TimingTier::Ok) == 0.50f);
}

TEST_CASE("timing_multiplier: Bad gives 0.25x", "[timing]") {
    CHECK(timing_multiplier(TimingTier::Bad) == 0.25f);
}

// ── window_scale_for_tier ────────────────────────────────────
// Spec (rhythm-spec.md §5/§7): better timing → smaller scale → window collapses sooner.

TEST_CASE("window_scale: Perfect gives 0.50", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Perfect) == 0.50f);
}

TEST_CASE("window_scale: Good gives 0.75", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Good) == 0.75f);
}

TEST_CASE("window_scale: Ok gives 1.00", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Ok) == 1.00f);
}

TEST_CASE("window_scale: Bad gives 1.00 (no-op, miss handled elsewhere)", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Bad) == 1.00f);
}

// Regression for issue #223 — values must be strictly ordered:
// Perfect(0.50) < Good(0.75) < Ok(1.00) == Bad(1.00).
// If this order inverts again the inversion bug has regressed.
TEST_CASE("window_scale: ordering — better tier gives smaller scale", "[timing][regression]") {
    CHECK(window_scale_for_tier(TimingTier::Perfect) < window_scale_for_tier(TimingTier::Good));
    CHECK(window_scale_for_tier(TimingTier::Good)    < window_scale_for_tier(TimingTier::Ok));
    CHECK(window_scale_for_tier(TimingTier::Ok)     == window_scale_for_tier(TimingTier::Bad));
}

TEST_CASE("window_scale: Perfect strictly shrinks, Ok/Bad are neutral", "[timing][regression]") {
    CHECK(window_scale_for_tier(TimingTier::Perfect) <  1.0f);
    CHECK(window_scale_for_tier(TimingTier::Good)    <  1.0f);
    CHECK(window_scale_for_tier(TimingTier::Ok)      == 1.0f);
    CHECK(window_scale_for_tier(TimingTier::Bad)     == 1.0f);
}

// ── song_state_compute_derived ───────────────────────────────

TEST_CASE("song_state_derived: beat_period from BPM", "[song_state]") {
    SongState s;
    s.bpm = 120.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);
    CHECK_THAT(s.beat_period, Catch::Matchers::WithinAbs(0.5f, 0.001f));
}

TEST_CASE("song_state_derived: scroll_speed = APPROACH_DIST / lead_time", "[song_state]") {
    SongState s;
    s.bpm = 120.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);
    // lead_time = 4 * 0.5 = 2.0s
    // scroll_speed = 1040 / 2.0 = 520
    CHECK_THAT(s.scroll_speed, Catch::Matchers::WithinAbs(520.0f, 0.1f));
}

TEST_CASE("song_state_derived: lead_time = lead_beats * beat_period", "[song_state]") {
    SongState s;
    s.bpm = 120.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);
    CHECK_THAT(s.lead_time, Catch::Matchers::WithinAbs(2.0f, 0.001f));
}

TEST_CASE("song_state_derived: window_duration fixed at timing policy", "[song_state]") {
    SongState slow, fast;
    slow.bpm = 100.0f;
    slow.lead_beats = 4;
    fast.bpm = 180.0f;
    fast.lead_beats = 4;
    song_state_compute_derived(slow);
    song_state_compute_derived(fast);
    CHECK_THAT(slow.window_duration, Catch::Matchers::WithinAbs(0.3f, 0.001f));
    CHECK_THAT(fast.window_duration, Catch::Matchers::WithinAbs(0.3f, 0.001f));
}

TEST_CASE("song_state_derived: window_duration respects bpm ceiling cap", "[song_state]") {
    SongState s;
    s.bpm = 300.0f;
    s.lead_beats = 2;
    song_state_compute_derived(s);
    CHECK(s.window_duration <= (60.0f / 180.0f));
}

TEST_CASE("song_state_derived: morph_duration has minimum floor", "[song_state]") {
    SongState s;
    s.bpm = 300.0f;
    s.lead_beats = 2;
    song_state_compute_derived(s);
    CHECK(s.morph_duration >= 0.06f);
}

TEST_CASE("song_state_derived: half_window is half of window_duration", "[song_state]") {
    SongState s;
    s.bpm = 120.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);
    CHECK_THAT(s.half_window, Catch::Matchers::WithinAbs(s.window_duration / 2.0f, 0.001f));
}

TEST_CASE("song_state_derived: fixed window independent of song BPM", "[song_state]") {
    SongState s;
    s.bpm = 100.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);
    CHECK_THAT(s.window_duration, Catch::Matchers::WithinAbs(0.3f, 0.001f));
}

// ── enum name helpers ────────────────────────────────────────

TEST_CASE("enum_name_or_unknown: Shape covers all shapes", "[enum_names]") {
    CHECK(std::string(enum_name_or_unknown(Shape::Circle))   == "Circle");
    CHECK(std::string(enum_name_or_unknown(Shape::Square))    == "Square");
    CHECK(std::string(enum_name_or_unknown(Shape::Triangle))  == "Triangle");
    CHECK(std::string(enum_name_or_unknown(Shape::Hexagon))   == "Hexagon");
}

TEST_CASE("enum_name_or_unknown: ObstacleKind covers all kinds", "[enum_names]") {
    CHECK(std::string(enum_name_or_unknown(ObstacleKind::ShapeGate))     == "ShapeGate");
    CHECK(std::string(enum_name_or_unknown(ObstacleKind::LaneBlock))     == "LaneBlock");
    CHECK(std::string(enum_name_or_unknown(ObstacleKind::LowBar))        == "LowBar");
    CHECK(std::string(enum_name_or_unknown(ObstacleKind::HighBar))       == "HighBar");
    CHECK(std::string(enum_name_or_unknown(ObstacleKind::ComboGate))     == "ComboGate");
    CHECK(std::string(enum_name_or_unknown(ObstacleKind::SplitPath))     == "SplitPath");
}

TEST_CASE("enum_name_or_unknown: TimingTier covers all tiers", "[enum_names]") {
    CHECK(std::string(enum_name_or_unknown(TimingTier::Bad))     == "Bad");
    CHECK(std::string(enum_name_or_unknown(TimingTier::Ok))      == "Ok");
    CHECK(std::string(enum_name_or_unknown(TimingTier::Good))    == "Good");
    CHECK(std::string(enum_name_or_unknown(TimingTier::Perfect)) == "Perfect");
}

// ── dispatcher helpers ───────────────────────────────────────

TEST_CASE("dispatcher: mixed go and press operations", "[input]") {
    auto reg = make_registry();

    auto& disp = reg.ctx().get<entt::dispatcher>();
    GoCapture go_cap;
    PressCapture press_cap;
    disp.sink<GoEvent>().connect<&GoCapture::capture>(go_cap);
    disp.sink<ButtonPressEvent>().connect<&PressCapture::capture>(press_cap);

    disp.enqueue<GoEvent>({Direction::Up});
    disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Circle});
    disp.enqueue<GoEvent>({Direction::Down});
    disp.update<GoEvent>();
    disp.update<ButtonPressEvent>();

    disp.sink<GoEvent>().disconnect<&GoCapture::capture>(go_cap);
    disp.sink<ButtonPressEvent>().disconnect<&PressCapture::capture>(press_cap);

    REQUIRE(go_cap.count == 2);
    REQUIRE(press_cap.count == 1);
    CHECK(go_cap.buf[0].dir == Direction::Up);
    CHECK(press_cap.buf[0].kind  == ButtonPressKind::Shape);
    CHECK(press_cap.buf[0].shape == Shape::Circle);
    CHECK(go_cap.buf[1].dir == Direction::Down);
}

// ── make_rhythm_registry / make_rhythm_player helpers ────────

TEST_CASE("make_rhythm_registry: SongState is configured", "[helpers]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    CHECK(song.bpm == 120.0f);
    CHECK(song.playing);
    CHECK(song.beat_period > 0.0f);
    CHECK(song.scroll_speed > 0.0f);
}

TEST_CASE("make_rhythm_player: starts as Hexagon", "[helpers]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(ps.previous == Shape::Hexagon);
    auto& sw = reg.get<ShapeWindow>(player);
    CHECK(sw.target_shape == Shape::Hexagon);
    CHECK(sw.phase == WindowPhase::Idle);
}
