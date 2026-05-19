#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "util/rhythm_math.h"

// ── timing_multiplier_from_delta_abs ─────────────────────────
// Per #1202/#1204: TimingTier eradicated; multiplier is now derived from the
// absolute delta-seconds via the threshold ladder. Each canonical delta below
// sits squarely inside the corresponding former-tier band.

TEST_CASE("timing_multiplier: delta=0 (Perfect band) gives 1.50x", "[timing]") {
    CHECK(timing_multiplier_from_delta_abs(0.0f) == 1.50f);
}

TEST_CASE("timing_multiplier: just past Perfect (Good band) gives 1.00x", "[timing]") {
    CHECK(timing_multiplier_from_delta_abs(kTimingPerfectSeconds + 0.001f) == 1.00f);
}

TEST_CASE("timing_multiplier: just past Good (Ok band) gives 0.50x", "[timing]") {
    CHECK(timing_multiplier_from_delta_abs(kTimingGoodSeconds + 0.001f) == 0.50f);
}

TEST_CASE("timing_multiplier: just past Ok (Bad band) gives 0.25x", "[timing]") {
    CHECK(timing_multiplier_from_delta_abs(kTimingOkSeconds + 0.001f) == 0.25f);
}

// ── window_scale_from_delta_abs ──────────────────────────────
// Spec (rhythm-spec.md §5/§7): better timing → smaller scale → window collapses sooner.

TEST_CASE("window_scale: delta=0 (Perfect band) gives 0.50", "[timing]") {
    CHECK(window_scale_from_delta_abs(0.0f) == 0.50f);
}

TEST_CASE("window_scale: just past Perfect (Good band) gives 0.75", "[timing]") {
    CHECK(window_scale_from_delta_abs(kTimingPerfectSeconds + 0.001f) == 0.75f);
}

TEST_CASE("window_scale: just past Good (Ok band) gives 1.00", "[timing]") {
    CHECK(window_scale_from_delta_abs(kTimingGoodSeconds + 0.001f) == 1.00f);
}

TEST_CASE("window_scale: just past Ok (Bad band) gives 1.00 (no-op, miss handled elsewhere)", "[timing]") {
    CHECK(window_scale_from_delta_abs(kTimingOkSeconds + 0.001f) == 1.00f);
}

// Regression for issue #223 — values must be strictly ordered:
// Perfect(0.50) < Good(0.75) < Ok(1.00) == Bad(1.00).
// If this order inverts again the inversion bug has regressed.
TEST_CASE("window_scale: ordering — better timing gives smaller scale", "[timing][regression]") {
    CHECK(window_scale_from_delta_abs(0.0f) <
          window_scale_from_delta_abs(kTimingPerfectSeconds + 0.001f));
    CHECK(window_scale_from_delta_abs(kTimingPerfectSeconds + 0.001f) <
          window_scale_from_delta_abs(kTimingGoodSeconds + 0.001f));
    CHECK(window_scale_from_delta_abs(kTimingGoodSeconds + 0.001f) ==
          window_scale_from_delta_abs(kTimingOkSeconds + 0.001f));
}

TEST_CASE("window_scale: Perfect/Good strictly shrink, Ok/Bad are neutral", "[timing][regression]") {
    CHECK(window_scale_from_delta_abs(0.0f)                              <  1.0f);
    CHECK(window_scale_from_delta_abs(kTimingPerfectSeconds + 0.001f)    <  1.0f);
    CHECK(window_scale_from_delta_abs(kTimingGoodSeconds    + 0.001f)    == 1.0f);
    CHECK(window_scale_from_delta_abs(kTimingOkSeconds      + 0.001f)    == 1.0f);
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
    CHECK_THAT(s.scroll_speed, Catch::Matchers::WithinAbs(constants::APPROACH_DIST / 2.0f, 0.1f));
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

TEST_CASE("ToString: Shape covers all shapes", "[ToString]") {
    CHECK(to_string(Shape::Circle)   == "Circle");
    CHECK(to_string(Shape::Square)   == "Square");
    CHECK(to_string(Shape::Triangle) == "Triangle");
    CHECK(to_string(Shape::Hexagon)  == "Hexagon");
}

TEST_CASE("ToString: HapticEvent covers all events", "[ToString]") {
    CHECK(to_string(HapticEvent::ShapeShift)   == "ShapeShift");
    CHECK(to_string(HapticEvent::LaneSwitch)   == "LaneSwitch");
    CHECK(to_string(HapticEvent::JumpLand)     == "JumpLand");
    CHECK(to_string(HapticEvent::DeathCrash)   == "DeathCrash");
    CHECK(to_string(HapticEvent::NewHighScore) == "NewHighScore");
    CHECK(to_string(HapticEvent::RetryTap)     == "RetryTap");
    CHECK(to_string(HapticEvent::UIButtonTap)  == "UIButtonTap");
}

// ObstacleKind enum was eradicated in issue #1202/#1204 (per Fabian's
// existential processing); each former value is now a zero-column tag
// (ShapeGateTag/SplitPathTag/OnsetMarkerTag) and the magic_enum
// reflection check no longer applies.

// TimingTier enum was eradicated in issue #1202/#1204 (per Fabian's existential
// processing); each former value is now a zero-column tag in app/tags/tags.h
// (TimingPerfectTag/TimingGoodTag/TimingOkTag/TimingBadTag) and the
// magic_enum reflection check no longer applies.

// ── dispatcher helpers ───────────────────────────────────────

TEST_CASE("dispatcher: mixed go and press operations", "[input]") {
    auto reg = make_registry();

    auto& disp = reg.ctx().get<entt::dispatcher>();
    GoCapture go_cap;
    PressCapture press_cap;
    disp.sink<GoUpEvent>()  .connect<&GoCapture::on_up>(go_cap);
    disp.sink<GoDownEvent>().connect<&GoCapture::on_down>(go_cap);
    disp.sink<ShapePressCircleEvent>().connect<&PressCapture::on_circle>(press_cap);

    disp.enqueue<GoUpEvent>({});
    disp.enqueue<ShapePressCircleEvent>({});
    disp.enqueue<GoDownEvent>({});
    disp.update<GoUpEvent>();
    disp.update<GoDownEvent>();
    disp.update<ShapePressCircleEvent>();

    disp.sink<GoUpEvent>()  .disconnect<&GoCapture::on_up>(go_cap);
    disp.sink<GoDownEvent>().disconnect<&GoCapture::on_down>(go_cap);
    disp.sink<ShapePressCircleEvent>().disconnect<&PressCapture::on_circle>(press_cap);

    REQUIRE(go_cap.count() == 2);
    REQUIRE(press_cap.shape_count() == 1);
    CHECK(go_cap.up   == 1);
    CHECK(go_cap.down == 1);
    CHECK(press_cap.circle == 1);
}

// ── make_rhythm_registry / make_rhythm_player helpers ────────

TEST_CASE("make_rhythm_registry: SongState is configured", "[helpers]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    CHECK(song.bpm == 120.0f);
    CHECK(reg.ctx().contains<SongPlayingTag>());
    CHECK(song.beat_period > 0.0f);
    CHECK(song.scroll_speed > 0.0f);
}

TEST_CASE("make_rhythm_player: starts as Hexagon", "[helpers]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    CHECK(reg.all_of<ShapeHexagonTag>(player));
    CHECK(reg.all_of<TargetShapeHexagonTag>(player));
    CHECK(window_phase_is_idle(reg, player));
}
