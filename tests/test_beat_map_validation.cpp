#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "beat_map_loader.h"

// ── validate_beat_map: BPM rules ─────────────────────────────

TEST_CASE("validate: valid BPM passes", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

TEST_CASE("validate: BPM below 60 fails", "[validate]") {
    BeatMap map;
    map.bpm = 50.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    CHECK_FALSE(errors.empty());
}

TEST_CASE("validate: BPM above 300 fails", "[validate]") {
    BeatMap map;
    map.bpm = 310.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: BPM at 60 boundary passes", "[validate]") {
    BeatMap map;
    map.bpm = 60.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
}

TEST_CASE("validate: BPM at 300 boundary passes", "[validate]") {
    BeatMap map;
    map.bpm = 300.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
}

// ── validate_beat_map: offset rules ──────────────────────────

TEST_CASE("validate: negative offset fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = -0.1f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: offset above 5.0 fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 5.1f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

// ── validate_beat_map: lead_beats rules ──────────────────────

TEST_CASE("validate: lead_beats below 2 fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 1;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: lead_beats above 8 fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 9;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

// ── validate_beat_map: beat ordering ─────────────────────────

TEST_CASE("validate: empty beats list fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    // No beats

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: duplicate beat indices fail", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Square, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: non-monotonic beat indices fail", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({5, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({3, ObstacleKind::ShapeGate, Shape::Square, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: beat index beyond duration fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;  // beat_period = 0.5
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 10.0f;  // max_beat = 10 / 0.5 = 20
    map.beats.push_back({25, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

// ── validate_beat_map: shape distance rule ───────────────────

TEST_CASE("validate: different shapes >= 3 beats apart passes", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({3, ObstacleKind::ShapeGate, Shape::Square, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
}

TEST_CASE("validate: different shapes < 3 beats apart fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Square, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: same shapes close together is OK", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({1, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
}

// ── validate_beat_map: lane rules ────────────────────────────

TEST_CASE("validate: shape_gate invalid lane fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 5, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("validate: split_path invalid lane fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::SplitPath, Shape::Circle, -1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

// ── validate_beat_map: multiple errors ───────────────────────

TEST_CASE("validate: multiple errors accumulated", "[validate]") {
    BeatMap map;
    map.bpm = 10.0f;     // Out of range
    map.offset = -1.0f;  // Out of range
    map.lead_beats = 0;  // Out of range
    map.duration = 60.0f;
    // No beats

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    CHECK(errors.size() >= 3);
}

// ── init_song_state ──────────────────────────────────────────

TEST_CASE("init_song_state: copies metadata from BeatMap", "[init_song]") {
    BeatMap map;
    map.bpm = 140.0f;
    map.offset = 0.5f;
    map.lead_beats = 3;
    map.duration = 90.0f;

    SongState state;
    init_song_state(state, map);

    CHECK(state.bpm == 140.0f);
    CHECK(state.offset == 0.5f);
    CHECK(state.lead_beats == 3);
    CHECK(state.duration_sec == 90.0f);
}

TEST_CASE("init_song_state: resets playback state", "[init_song]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;

    SongState state;
    state.song_time = 50.0f;
    state.current_beat = 10;
    state.playing = true;
    state.finished = true;
    state.next_spawn_idx = 5;

    init_song_state(state, map);

    CHECK(state.song_time == 0.0f);
    CHECK(state.current_beat == -1);
    CHECK_FALSE(state.playing);
    CHECK_FALSE(state.finished);
    CHECK(state.next_spawn_idx == 0);
}

TEST_CASE("init_song_state: computes derived fields", "[init_song]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;

    SongState state;
    init_song_state(state, map);

    CHECK_THAT(state.beat_period, Catch::Matchers::WithinAbs(0.5f, 0.001f));
    CHECK_THAT(state.lead_time, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    CHECK(state.scroll_speed > 0.0f);
    CHECK(state.window_duration > 0.0f);
    CHECK(state.morph_duration > 0.0f);
}

// ── rhythm helper functions ──────────────────────────────────

TEST_CASE("compute_timing_tier: Perfect for <= 0.25", "[rhythm_helpers]") {
    CHECK(compute_timing_tier(0.0f) == TimingTier::Perfect);
    CHECK(compute_timing_tier(0.25f) == TimingTier::Perfect);
}

TEST_CASE("compute_timing_tier: Good for <= 0.50", "[rhythm_helpers]") {
    CHECK(compute_timing_tier(0.26f) == TimingTier::Good);
    CHECK(compute_timing_tier(0.50f) == TimingTier::Good);
}

TEST_CASE("compute_timing_tier: Ok for <= 0.75", "[rhythm_helpers]") {
    CHECK(compute_timing_tier(0.51f) == TimingTier::Ok);
    CHECK(compute_timing_tier(0.75f) == TimingTier::Ok);
}

TEST_CASE("compute_timing_tier: Bad for > 0.75", "[rhythm_helpers]") {
    CHECK(compute_timing_tier(0.76f) == TimingTier::Bad);
    CHECK(compute_timing_tier(1.0f) == TimingTier::Bad);
}

TEST_CASE("timing_multiplier: returns correct values", "[rhythm_helpers]") {
    CHECK(timing_multiplier(TimingTier::Perfect) == 1.50f);
    CHECK(timing_multiplier(TimingTier::Good) == 1.00f);
    CHECK(timing_multiplier(TimingTier::Ok) == 0.50f);
    CHECK(timing_multiplier(TimingTier::Bad) == 0.25f);
}

TEST_CASE("window_scale_for_tier: returns correct values", "[rhythm_helpers]") {
    CHECK(window_scale_for_tier(TimingTier::Perfect) == 1.50f);
    CHECK(window_scale_for_tier(TimingTier::Good) == 1.00f);
    CHECK(window_scale_for_tier(TimingTier::Ok) == 0.75f);
    CHECK(window_scale_for_tier(TimingTier::Bad) == 0.50f);
}

TEST_CASE("song_state_compute_derived: beat_period calculation", "[rhythm_helpers]") {
    SongState s;
    s.bpm = 120.0f;
    s.offset = 0.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);

    CHECK_THAT(s.beat_period, Catch::Matchers::WithinAbs(0.5f, 0.001f));
}

TEST_CASE("song_state_compute_derived: lead_time calculation", "[rhythm_helpers]") {
    SongState s;
    s.bpm = 120.0f;
    s.offset = 0.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);

    // lead_time = lead_beats * beat_period = 4 * 0.5 = 2.0
    CHECK_THAT(s.lead_time, Catch::Matchers::WithinAbs(2.0f, 0.001f));
}

TEST_CASE("song_state_compute_derived: window_duration minimum enforced", "[rhythm_helpers]") {
    SongState s;
    s.bpm = 300.0f;  // Very fast BPM, beat_period = 0.2
    s.offset = 0.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);

    // window_duration = BASE_WINDOW_BEATS * beat_period = 1.6 * 0.2 = 0.32
    // But MIN_WINDOW = 0.36, so window_duration should be clamped
    CHECK(s.window_duration >= 0.36f);
}

TEST_CASE("song_state_compute_derived: morph_duration minimum enforced", "[rhythm_helpers]") {
    SongState s;
    s.bpm = 300.0f;  // Very fast BPM, beat_period = 0.2
    s.offset = 0.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);

    // morph_duration = BASE_MORPH_BEATS * beat_period = 0.2 * 0.2 = 0.04
    // But MIN_MORPH = 0.06, so morph_duration should be clamped
    CHECK(s.morph_duration >= 0.06f);
}

TEST_CASE("song_state_compute_derived: scroll_speed calculation", "[rhythm_helpers]") {
    SongState s;
    s.bpm = 120.0f;
    s.offset = 0.0f;
    s.lead_beats = 4;
    song_state_compute_derived(s);

    // scroll_speed = APPROACH_DIST / lead_time = 1040 / 2.0 = 520
    CHECK_THAT(s.scroll_speed, Catch::Matchers::WithinAbs(520.0f, 1.0f));
}
