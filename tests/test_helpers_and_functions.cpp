#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "enum_names.h"

// ── compute_timing_tier ──────────────────────────────────────

TEST_CASE("timing_tier: perfect zone 0.0-0.25", "[timing]") {
    CHECK(compute_timing_tier(0.0f)  == TimingTier::Perfect);
    CHECK(compute_timing_tier(0.25f) == TimingTier::Perfect);
}

TEST_CASE("timing_tier: good zone 0.26-0.50", "[timing]") {
    CHECK(compute_timing_tier(0.26f) == TimingTier::Good);
    CHECK(compute_timing_tier(0.50f) == TimingTier::Good);
}

TEST_CASE("timing_tier: ok zone 0.51-0.75", "[timing]") {
    CHECK(compute_timing_tier(0.51f) == TimingTier::Ok);
    CHECK(compute_timing_tier(0.75f) == TimingTier::Ok);
}

TEST_CASE("timing_tier: bad zone 0.76-1.0", "[timing]") {
    CHECK(compute_timing_tier(0.76f) == TimingTier::Bad);
    CHECK(compute_timing_tier(1.0f)  == TimingTier::Bad);
}

TEST_CASE("timing_tier: boundary at exactly 0.25 is perfect", "[timing]") {
    CHECK(compute_timing_tier(0.25f) == TimingTier::Perfect);
}

TEST_CASE("timing_tier: boundary at exactly 0.50 is good", "[timing]") {
    CHECK(compute_timing_tier(0.50f) == TimingTier::Good);
}

TEST_CASE("timing_tier: boundary at exactly 0.75 is ok", "[timing]") {
    CHECK(compute_timing_tier(0.75f) == TimingTier::Ok);
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

TEST_CASE("window_scale: Perfect gives 1.50", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Perfect) == 1.50f);
}

TEST_CASE("window_scale: Good gives 1.00", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Good) == 1.00f);
}

TEST_CASE("window_scale: Ok gives 0.75", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Ok) == 0.75f);
}

TEST_CASE("window_scale: Bad gives 0.50", "[timing]") {
    CHECK(window_scale_for_tier(TimingTier::Bad) == 0.50f);
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

TEST_CASE("song_state_derived: window_duration scales for high BPM", "[song_state]") {
    SongState slow, fast;
    slow.bpm = 100.0f;
    slow.lead_beats = 4;
    fast.bpm = 180.0f;
    fast.lead_beats = 4;
    song_state_compute_derived(slow);
    song_state_compute_derived(fast);
    // Higher BPM → shorter window
    CHECK(fast.window_duration < slow.window_duration);
}

TEST_CASE("song_state_derived: window_duration has minimum floor", "[song_state]") {
    SongState s;
    s.bpm = 300.0f;
    s.lead_beats = 2;
    song_state_compute_derived(s);
    CHECK(s.window_duration >= 0.36f);
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

TEST_CASE("song_state_derived: BPM below 130 has no scale reduction", "[song_state]") {
    SongState s;
    s.bpm = 100.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);
    // At 100 BPM, bpm_scale = 1.0
    // window_duration = BASE_WINDOW_BEATS * beat_period * 1.0 = 1.6 * 0.6 = 0.96
    CHECK_THAT(s.window_duration, Catch::Matchers::WithinAbs(1.6f * s.beat_period, 0.001f));
}

// ── enum name helpers ────────────────────────────────────────

TEST_CASE("enum_names: shape_name covers all shapes", "[enum_names]") {
    CHECK(std::string(shape_name(Shape::Circle))   == "Circle");
    CHECK(std::string(shape_name(Shape::Square))    == "Square");
    CHECK(std::string(shape_name(Shape::Triangle))  == "Triangle");
    CHECK(std::string(shape_name(Shape::Hexagon))   == "Hexagon");
}

TEST_CASE("enum_names: obstacle_kind_name covers all kinds", "[enum_names]") {
    CHECK(std::string(obstacle_kind_name(ObstacleKind::ShapeGate)) == "ShapeGate");
    CHECK(std::string(obstacle_kind_name(ObstacleKind::LaneBlock)) == "LaneBlock");
    CHECK(std::string(obstacle_kind_name(ObstacleKind::LowBar))    == "LowBar");
    CHECK(std::string(obstacle_kind_name(ObstacleKind::HighBar))   == "HighBar");
    CHECK(std::string(obstacle_kind_name(ObstacleKind::ComboGate)) == "ComboGate");
    CHECK(std::string(obstacle_kind_name(ObstacleKind::SplitPath)) == "SplitPath");
}

TEST_CASE("enum_names: timing_tier_name covers all tiers", "[enum_names]") {
    CHECK(std::string(timing_tier_name(TimingTier::Bad))     == "Bad");
    CHECK(std::string(timing_tier_name(TimingTier::Ok))      == "Ok");
    CHECK(std::string(timing_tier_name(TimingTier::Good))    == "Good");
    CHECK(std::string(timing_tier_name(TimingTier::Perfect)) == "Perfect");
}

// ── ActionQueue ──────────────────────────────────────────────

TEST_CASE("action_queue: go adds direction action", "[input]") {
    ActionQueue aq{};
    aq.go(Direction::Left);
    CHECK(aq.count == 1);
    CHECK(aq.actions[0].verb == ActionVerb::Go);
    CHECK(aq.actions[0].dir == Direction::Left);
}

TEST_CASE("action_queue: tap with position stores coordinates", "[input]") {
    ActionQueue aq{};
    aq.tap(Button::Position, 100.0f, 200.0f);
    CHECK(aq.count == 1);
    CHECK(aq.actions[0].verb == ActionVerb::Tap);
    CHECK(aq.actions[0].button == Button::Position);
    CHECK(aq.actions[0].x == 100.0f);
    CHECK(aq.actions[0].y == 200.0f);
}

TEST_CASE("action_queue: overflow is rejected", "[input]") {
    ActionQueue aq{};
    for (int i = 0; i < ActionQueue::MAX + 5; ++i) {
        aq.go(Direction::Right);
    }
    CHECK(aq.count == ActionQueue::MAX);
}

TEST_CASE("action_queue: clear resets count", "[input]") {
    ActionQueue aq{};
    aq.go(Direction::Left);
    aq.tap(Button::Confirm);
    CHECK(aq.count == 2);
    aq.clear();
    CHECK(aq.count == 0);
}

TEST_CASE("action_queue: mixed go and tap operations", "[input]") {
    ActionQueue aq{};
    aq.go(Direction::Up);
    aq.tap(Button::ShapeCircle);
    aq.go(Direction::Down);
    CHECK(aq.count == 3);
    CHECK(aq.actions[0].verb == ActionVerb::Go);
    CHECK(aq.actions[0].dir == Direction::Up);
    CHECK(aq.actions[1].verb == ActionVerb::Tap);
    CHECK(aq.actions[1].button == Button::ShapeCircle);
    CHECK(aq.actions[2].verb == ActionVerb::Go);
    CHECK(aq.actions[2].dir == Direction::Down);
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
    CHECK(sw.phase_raw == 0);
}

