#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <magic_enum/magic_enum.hpp>
#include "test_helpers.h"
#include "ui/screen_controllers/screen_controller_base.h"
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
    CHECK(magic_enum::enum_name(Shape::Circle)   == "Circle");
    CHECK(magic_enum::enum_name(Shape::Square)   == "Square");
    CHECK(magic_enum::enum_name(Shape::Triangle) == "Triangle");
    CHECK(magic_enum::enum_name(Shape::Hexagon)  == "Hexagon");
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
    disp.sink<GoEvent>().connect<&GoCapture::capture>(go_cap);
    disp.sink<ShapePressCircleEvent>().connect<&PressCapture::on_circle>(press_cap);

    disp.enqueue<GoEvent>({Direction::Up});
    disp.enqueue<ShapePressCircleEvent>({});
    disp.enqueue<GoEvent>({Direction::Down});
    disp.update<GoEvent>();
    disp.update<ShapePressCircleEvent>();

    disp.sink<GoEvent>().disconnect<&GoCapture::capture>(go_cap);
    disp.sink<ShapePressCircleEvent>().disconnect<&PressCapture::on_circle>(press_cap);

    REQUIRE(go_cap.count == 2);
    REQUIRE(press_cap.shape_count() == 1);
    CHECK(go_cap.buf[0].dir == Direction::Up);
    CHECK(press_cap.circle == 1);
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
    CHECK(reg.all_of<ShapeHexagonTag>(player));
    CHECK(reg.all_of<TargetShapeHexagonTag>(player));
    CHECK(window_phase_is_idle(reg, player));
}

namespace {
struct DummyScreenController {
    int init_calls = 0;
    void init() { ++init_calls; }
};
}

TEST_CASE("screen_controller helper keeps controller state in registry context", "[ui][architecture]") {
    entt::registry reg_a;
    entt::registry reg_b;

    auto& a1 = screen_controller<DummyScreenController>(reg_a);
    auto& a2 = screen_controller<DummyScreenController>(reg_a);
    auto& b1 = screen_controller<DummyScreenController>(reg_b);

    CHECK(&a1 == &a2);
    CHECK(&a1 != &b1);
    CHECK(a1.init_calls == 1);
    CHECK(b1.init_calls == 1);
}
