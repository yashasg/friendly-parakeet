// Regression tests for #474: SettingsState::audio_offset_ms is now consumed by
// beat_scheduler_system. A non-zero offset shifts spawn_time (and therefore
// the obstacle's beat-crossing event) by the same delta. Positive offset
// delays beat timing (matches settings.h docstring).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "test_helpers.h"
#include "components/beat_map.h"
#include "components/rhythm.h"
#include "systems/all_systems.h"
#include "util/rhythm_math.h"
#include "util/settings.h"
#include "util/settings_persistence.h"

namespace {

constexpr float kTimingToleranceSec = 0.001f;

// Inject a single beat at known beat_index then run the scheduler to
// completion. Returns BeatInfo::arrival_time and the entity's actual
// effective_spawn_time via the second out-parameter so callers can assert
// the *spawn_time* shift (which is the offset's observable effect on the
// scheduling event), independent of arrival_time invariants.
struct ScheduleResult {
    float arrival_time;
    float effective_spawn_time;
};

ScheduleResult schedule_one(int16_t audio_offset_ms,
                            float song_time_when_scheduling,
                            float beatmap_offset_seconds,
                            float bpm,
                            int   beat_index) {
    auto reg = make_rhythm_registry();

    auto& settings = reg.ctx().get<SettingsState>();
    settings.audio_offset_ms = audio_offset_ms;

    auto& song = reg.ctx().get<SongState>();
    song.bpm    = bpm;
    song.offset = beatmap_offset_seconds;
    song_state_compute_derived(song);
    song.song_time = song_time_when_scheduling;

    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({beat_index, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    int n = 0;
    ScheduleResult out{0.0f, 0.0f};
    for (auto [e, bi] : view.each()) {
        out.arrival_time = bi.arrival_time;
        out.effective_spawn_time = bi.spawn_time;
        ++n;
    }
    REQUIRE(n == 1);
    return out;
}

} // namespace

TEST_CASE("audio_offset_ms helper converts ms→seconds with documented sign", "[settings][audio_offset][issue474]") {
    SettingsState s;
    s.audio_offset_ms = 0;
    CHECK(settings::audio_offset_seconds(s) == 0.0f);
    s.audio_offset_ms = 200;
    CHECK_THAT(settings::audio_offset_seconds(s), Catch::Matchers::WithinAbs(0.200f, 1e-6f));
    s.audio_offset_ms = -150;
    CHECK_THAT(settings::audio_offset_seconds(s), Catch::Matchers::WithinAbs(-0.150f, 1e-6f));
}

TEST_CASE("beat_scheduler: positive audio_offset_ms delays spawn_time (#474)",
          "[beat_scheduler][audio_offset][issue474]") {
    // Same beatmap & song clock, only audio_offset_ms differs. The delta in
    // spawn_time must equal audio_offset_seconds(state). Drive the scheduler
    // far enough into the future that both runs definitely spawn the beat.
    constexpr float bpm = 120.0f;
    constexpr float beatmap_offset = 1.0f;
    constexpr int   idx = 4;
    // beat_time = 1.0 + 4 * 0.5 = 3.0; lead_time default = 2.0 → baseline
    // spawn_time = 1.0. We pick song_time = 1.5 so all three runs trigger
    // a small overshoot (no clamp to PLAYER_Y) and the offset shift is
    // visible 1:1 in effective_spawn_time.
    constexpr float when_song_time = 1.5f;

    const auto baseline = schedule_one(/*offset_ms*/   0, when_song_time, beatmap_offset, bpm, idx);
    const auto delayed  = schedule_one(/*offset_ms*/+200, when_song_time, beatmap_offset, bpm, idx);
    const auto advanced = schedule_one(/*offset_ms*/-200, when_song_time, beatmap_offset, bpm, idx);

    // arrival_time tracks the beatmap's beat_time which is unchanged by the
    // calibration helper; what shifts is the spawn schedule. We therefore
    // assert on raw arrival_time only that it stayed equal across runs.
    CHECK(baseline.arrival_time == delayed.arrival_time);
    CHECK(baseline.arrival_time == advanced.arrival_time);

    // The visible effect: spawn_time is pushed by exactly audio_offset_seconds.
    // (When the scheduler "overshoots" because song_time is far ahead it
    // clamps spawn_time to keep the obstacle at PLAYER_Y, so we measure
    // delta against the SAME-clamp baseline by using identical song_time.)
    const float delayed_delta  = delayed.effective_spawn_time  - baseline.effective_spawn_time;
    const float advanced_delta = advanced.effective_spawn_time - baseline.effective_spawn_time;

    CHECK_THAT(delayed_delta,  Catch::Matchers::WithinAbs(+0.200f, kTimingToleranceSec));
    CHECK_THAT(advanced_delta, Catch::Matchers::WithinAbs(-0.200f, kTimingToleranceSec));
}
