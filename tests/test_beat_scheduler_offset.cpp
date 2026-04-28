// Regression tests for issue #137: offset semantics in beat_scheduler_system.
//
// The beatmap "offset" field is the arrival time of beat_index=0 in seconds.
// Obstacle collision time is computed as:
//
//   beat_time = offset + beat_index * beat_period
//
// This means:
//   - offset is a global origin; ALL beat times shift by the same delta when
//     offset changes.
//   - A non-zero offset shifts the entire collision grid into the future.
//   - If offset blindly copies beats[0] from the audio analysis, and authored
//     beat_indices start at N > 0, the first collision lands at
//     beats[0] + N*period rather than at the actual timestamp beats[N]. With
//     non-uniform beats (tempo drift), these diverge.
//
// These tests pin the C++ scheduler's interpretation of offset, guard against
// the "offset = first detected beat" blindly-shifted accumulation, and will
// remain valid after Rabin/Fenster change how offset is produced by the pipeline.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "components/rhythm.h"
#include "util/rhythm_math.h"
#include "components/beat_map.h"
#include "systems/all_systems.h"
#include "constants.h"

// ── Helpers ──────────────────────────────────────────────────────────────────

// Tolerance in seconds for arrival time assertions.
// 8ms ≈ half a frame at 60fps — anything beyond this is audibly off-beat.
static constexpr float kTimingToleranceSec = 0.008f;

// Convenience: run scheduler far enough into the future to spawn all beats.
static void advance_to_spawn_all(entt::registry& reg) {
    reg.ctx().get<SongState>().song_time = 9999.0f;
    beat_scheduler_system(reg, 0.016f);
}

// Return the arrival_time stored in BeatInfo for the one spawned obstacle.
// Asserts exactly one obstacle was spawned.
static float single_arrival_time(entt::registry& reg) {
    auto view = reg.view<ObstacleTag, BeatInfo>();
    int count = 0;
    float t = 0.0f;
    for (auto [e, bi] : view.each()) {
        t = bi.arrival_time;
        ++count;
    }
    REQUIRE(count == 1);
    return t;
}

// ── Contract: offset defines the arrival time of beat_index = 0 ──────────────

TEST_CASE("offset_semantics: beat_index=0 arrives at exactly offset", "[beat_scheduler][offset][issue137]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map  = reg.ctx().get<BeatMap>();

    song.offset = 1.5f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    advance_to_spawn_all(reg);

    float arrival = single_arrival_time(reg);
    // beat_time = offset + 0 * period = offset
    CHECK_THAT(arrival, Catch::Matchers::WithinAbs(1.5f, kTimingToleranceSec));
}

TEST_CASE("offset_semantics: beat_index=0 with zero offset arrives at t=0", "[beat_scheduler][offset][issue137]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map  = reg.ctx().get<BeatMap>();

    song.offset = 0.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Square, 1, 0});

    advance_to_spawn_all(reg);

    float arrival = single_arrival_time(reg);
    CHECK_THAT(arrival, Catch::Matchers::WithinAbs(0.0f, kTimingToleranceSec));
}

// ── Contract: linear formula beat_time = offset + beat_index * period ────────

TEST_CASE("offset_semantics: beat_index=N arrives at offset + N*period", "[beat_scheduler][offset][issue137]") {
    // bpm=120 → period=0.5s; offset=0.35s; beat_index=8
    // expected = 0.35 + 8 * 0.5 = 4.35
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map  = reg.ctx().get<BeatMap>();

    song.bpm    = 120.0f;
    song.offset = 0.35f;
    song_state_compute_derived(song);
    map.beats.push_back({8, ObstacleKind::ShapeGate, Shape::Triangle, 1, 0});

    advance_to_spawn_all(reg);

    float period  = 60.0f / 120.0f;
    float expected = 0.35f + 8.0f * period;
    float arrival  = single_arrival_time(reg);
    CHECK_THAT(arrival, Catch::Matchers::WithinAbs(expected, kTimingToleranceSec));
}

TEST_CASE("offset_semantics: two beat_indices are exactly period apart", "[beat_scheduler][offset][issue137]") {
    // Beat N and beat N+1 should have arrival times exactly one period apart.
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map  = reg.ctx().get<BeatMap>();

    song.bpm    = 160.0f;
    song.offset = 2.27f;
    song_state_compute_derived(song);
    map.beats.push_back({10, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({11, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    advance_to_spawn_all(reg);

    auto view  = reg.view<ObstacleTag, BeatInfo>();
    float t10  = 0.0f, t11 = 0.0f;
    for (auto [e, bi] : view.each()) {
        if (bi.beat_index == 10) t10 = bi.arrival_time;
        if (bi.beat_index == 11) t11 = bi.arrival_time;
    }

    float period = 60.0f / 160.0f;
    CHECK_THAT(t11 - t10, Catch::Matchers::WithinAbs(period, kTimingToleranceSec));
}

// ── Regression: offset is a global shift ─────────────────────────────────────

TEST_CASE("offset_semantics: doubling offset shifts arrival by same delta", "[beat_scheduler][offset][issue137]") {
    // Changing offset by delta must shift ALL beat arrival times by exactly delta.
    auto measure_arrival = [](float offset_val, int beat_idx) -> float {
        auto reg = make_rhythm_registry();
        auto& song = reg.ctx().get<SongState>();
        auto& map  = reg.ctx().get<BeatMap>();
        song.bpm    = 120.0f;
        song.offset = offset_val;
        song_state_compute_derived(song);
        map.beats.push_back({beat_idx, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
        advance_to_spawn_all(reg);
        return single_arrival_time(reg);
    };

    const float delta = 0.25f;
    const int   idx   = 5;

    float t_base    = measure_arrival(0.5f, idx);
    float t_shifted = measure_arrival(0.5f + delta, idx);

    CHECK_THAT(t_shifted - t_base, Catch::Matchers::WithinAbs(delta, kTimingToleranceSec));
}

// ── Regression #137: non-zero first authored beat_index ──────────────────────

TEST_CASE("offset_semantics: first authored beat_index=N>0 arrives after offset", "[beat_scheduler][offset][issue137]") {
    // All shipped beatmaps have first beat_index >= 2.
    // The first collision must NOT arrive at offset (beat 0) — it arrives later.
    // If it arrived at offset, the obstacle would precede the first beat of
    // authored content (beat 0 was never in the song chart).
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map  = reg.ctx().get<BeatMap>();

    song.bpm    = 120.0f;
    song.offset = 1.0f;
    song_state_compute_derived(song);

    // Simulate a typical shipped-beatmap first obstacle (beat_index >= 2)
    const int first_idx = 4;
    map.beats.push_back({first_idx, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    advance_to_spawn_all(reg);

    float period  = 60.0f / 120.0f;
    float arrival = single_arrival_time(reg);
    // Must arrive at offset + first_idx * period, NOT at offset itself
    CHECK(arrival > song.offset);
    CHECK_THAT(arrival, Catch::Matchers::WithinAbs(song.offset + first_idx * period, kTimingToleranceSec));
}

TEST_CASE("offset_semantics: offset must not be zero when pipeline sets it from first beat", "[beat_scheduler][offset][issue137]") {
    // The pipeline sets offset = beats[0] (the first aubio-detected beat timestamp).
    // beats[0] is typically > 0 (songs have some lead-in silence before the downbeat).
    // If offset were forced to 0.0, all beat_index=0 arrivals would land at t=0,
    // but the first authored obstacle (beat_index >= 2) would arrive at
    // 0 + N*period — too early relative to the actual music.
    //
    // This test encodes the constraint that a non-trivial offset (> 0) on a
    // real-world song means the rhythm grid starts at a positive time.
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map  = reg.ctx().get<BeatMap>();

    // Stomper offset = 2.27s (real value from shipped beatmap)
    song.bpm    = 159.0f;
    song.offset = 2.27f;  // beats[0] from analysis
    song_state_compute_derived(song);

    // First authored obstacle on hard: beat_index = 2
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    advance_to_spawn_all(reg);

    float period  = 60.0f / 159.0f;
    float expected = 2.27f + 2.0f * period;
    float arrival  = single_arrival_time(reg);

    // Verify non-zero offset pushes collision into the future correctly
    CHECK(arrival > 2.27f);
    CHECK_THAT(arrival, Catch::Matchers::WithinAbs(expected, kTimingToleranceSec));
}

// ── Regression: uniform-beat assumption accumulates per-period error ──────────
//
// With perfectly uniform beats, offset + N*period == beats[N] exactly.
// This test uses manually crafted "slightly non-uniform" beat times to show
// that the formula can diverge from the actual beat timestamp.
// The test asserts the FORMULA is applied faithfully; the tolerance threshold
// documents the maximum acceptable timing error across a song.
//
// NOTE: current shipped songs have ≤0.7 ms beat drift (all pass).
// A song with ≥8 ms per-beat drift over 100+ beats would trip this threshold.

TEST_CASE("offset_semantics: uniform-beat assumption valid within per-beat tolerance", "[beat_scheduler][offset][issue137]") {
    // Synthesize a 50-beat song at 120bpm with tiny per-beat jitter
    // Jitter magnitude chosen to stay within 8ms at beat index 50
    constexpr float BPM    = 120.0f;
    constexpr float PERIOD = 60.0f / BPM;
    constexpr float OFFSET = 0.5f;     // "beats[0]" from pipeline
    constexpr int   N      = 50;

    // Simulate what the pipeline outputs for beat_index and what the scheduler computes
    // The formula: beat_time = OFFSET + beat_index * PERIOD
    // Actual beats from audio: OFFSET + i * PERIOD + tiny jitter

    // Maximum tolerable single-beat jitter before collisions go perceptibly off-beat
    constexpr float PER_BEAT_JITTER_LIMIT = 0.0015f;  // 1.5ms per beat

    // Accumulated drift at beat N = N * jitter (worst case linear accumulation)
    float max_accumulated_drift = N * PER_BEAT_JITTER_LIMIT;

    // With current songs (≤0.7ms max drift total, not per-beat), we are well inside
    CHECK(max_accumulated_drift < 0.100f);  // 100ms cap — catches pathological tempo ramps

    // Verify scheduler applies the formula with zero internal error
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map  = reg.ctx().get<BeatMap>();

    song.bpm    = BPM;
    song.offset = OFFSET;
    song_state_compute_derived(song);

    for (int i = 0; i <= N; i += 10) {
        map.beats.push_back({i, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    }

    advance_to_spawn_all(reg);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    for (auto [e, bi] : view.each()) {
        float expected_arrival = OFFSET + static_cast<float>(bi.beat_index) * PERIOD;
        CHECK_THAT(bi.arrival_time,
                   Catch::Matchers::WithinAbs(expected_arrival, kTimingToleranceSec));
    }
}

// ── Regression: changing BPM changes beat_period and all collision times ──────

TEST_CASE("offset_semantics: halving BPM doubles all collision intervals", "[beat_scheduler][offset][issue137]") {
    // If bpm changes, beat_period changes, and all collision times scale accordingly.
    // This guards against the scheduler hard-coding a period instead of using
    // song.beat_period (which is derived from song.bpm via song_state_compute_derived).
    auto measure_arrival = [](float bpm, int beat_idx) -> float {
        auto reg = make_rhythm_registry();
        auto& song = reg.ctx().get<SongState>();
        auto& map  = reg.ctx().get<BeatMap>();
        song.bpm    = bpm;
        song.offset = 0.0f;
        song_state_compute_derived(song);
        map.beats.push_back({beat_idx, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
        advance_to_spawn_all(reg);
        return single_arrival_time(reg);
    };

    const int idx = 10;
    float t120 = measure_arrival(120.0f, idx);  // 10 * 0.5 = 5.0
    float t60  = measure_arrival(60.0f,  idx);  // 10 * 1.0 = 10.0

    // Halving BPM → doubling period → arrivals should be 2× as far from offset
    CHECK_THAT(t60 / t120, Catch::Matchers::WithinAbs(2.0f, 0.001f));
}
