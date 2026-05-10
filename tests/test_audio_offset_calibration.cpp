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
// scheduling event), alongside calibrated-arrival invariants.
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

TEST_CASE("beat_scheduler: audio_offset shifts calibrated arrival and spawn_time",
          "[beat_scheduler][audio_offset][issue530]") {
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

    const float delayed_arrival_delta  = delayed.arrival_time - baseline.arrival_time;
    const float advanced_arrival_delta = advanced.arrival_time - baseline.arrival_time;

    CHECK_THAT(delayed_arrival_delta,  Catch::Matchers::WithinAbs(+0.200f, kTimingToleranceSec));
    CHECK_THAT(advanced_arrival_delta, Catch::Matchers::WithinAbs(-0.200f, kTimingToleranceSec));

    const float delayed_spawn_delta  = delayed.effective_spawn_time  - baseline.effective_spawn_time;
    const float advanced_spawn_delta = advanced.effective_spawn_time - baseline.effective_spawn_time;

    CHECK_THAT(delayed_spawn_delta,  Catch::Matchers::WithinAbs(+0.200f, kTimingToleranceSec));
    CHECK_THAT(advanced_spawn_delta, Catch::Matchers::WithinAbs(-0.200f, kTimingToleranceSec));
}


namespace {

struct OffsetGameplayResult {
    TimingTier tier = TimingTier::Bad;
    int score = 0;
    float energy = 0.0f;
    int perfect_count = 0;
    int miss_count = 0;
    float arrival_time = 0.0f;
    float visual_arrival_time = 0.0f;
};

OffsetGameplayResult run_calibrated_on_arrival_hit(int16_t audio_offset_ms) {
    auto reg = make_rhythm_registry();

    auto& settings = reg.ctx().get<SettingsState>();
    settings.audio_offset_ms = audio_offset_ms;

    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.graded = false;

    auto& song = reg.ctx().get<SongState>();
    song.bpm    = 120.0f;
    song.offset = 1.0f;
    song_state_compute_derived(song);

    constexpr int kBeatIndex = 4;
    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({kBeatIndex, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    const float beat_time = song.offset + static_cast<float>(kBeatIndex) * song.beat_period;
    const float spawn_time = beat_time - song.lead_time + settings::audio_offset_seconds(settings);
    song.song_time = spawn_time + 0.05f;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    REQUIRE(view.begin() != view.end());
    auto obstacle = *view.begin();

    auto& info = reg.get<BeatInfo>(obstacle);
    auto& wt = reg.get<WorldTransform>(obstacle);
    wt.position.y = constants::PLAYER_Y;

    sw.press_time = info.spawn_time + song.lead_time;

    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<ScoredTag>(obstacle));
    REQUIRE(reg.all_of<TimingGrade>(obstacle));

    const auto tier = reg.get<TimingGrade>(obstacle).tier;

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    const auto& score = reg.ctx().get<ScoreState>();
    const auto& energy = reg.ctx().get<EnergyState>();
    const auto& results = reg.ctx().get<SongResults>();

    OffsetGameplayResult out;
    out.tier = tier;
    out.score = score.score;
    out.energy = energy.energy;
    out.perfect_count = results.perfect_count;
    out.miss_count = results.miss_count;
    out.arrival_time = info.arrival_time;
    out.visual_arrival_time = info.spawn_time + song.lead_time;
    return out;
}

} // namespace

TEST_CASE("collision grading stays invariant for calibrated on-arrival presses",
          "[collision][audio_offset][issue530]") {
    const auto baseline = run_calibrated_on_arrival_hit(0);
    const auto delayed  = run_calibrated_on_arrival_hit(+200);
    const auto advanced = run_calibrated_on_arrival_hit(-200);

    CHECK(baseline.tier == TimingTier::Perfect);
    CHECK(delayed.tier == baseline.tier);
    CHECK(advanced.tier == baseline.tier);

    CHECK_THAT(baseline.arrival_time - baseline.visual_arrival_time,
               Catch::Matchers::WithinAbs(0.0f, kTimingToleranceSec));
    CHECK_THAT(delayed.arrival_time - delayed.visual_arrival_time,
               Catch::Matchers::WithinAbs(0.0f, kTimingToleranceSec));
    CHECK_THAT(advanced.arrival_time - advanced.visual_arrival_time,
               Catch::Matchers::WithinAbs(0.0f, kTimingToleranceSec));
}

TEST_CASE("score and energy semantics stay aligned under signed audio offsets",
          "[integration][audio_offset][issue530]") {
    const auto baseline = run_calibrated_on_arrival_hit(0);
    const auto delayed  = run_calibrated_on_arrival_hit(+200);
    const auto advanced = run_calibrated_on_arrival_hit(-200);

    CHECK(delayed.score == baseline.score);
    CHECK(advanced.score == baseline.score);

    CHECK_THAT(delayed.energy, Catch::Matchers::WithinAbs(baseline.energy, 1e-6f));
    CHECK_THAT(advanced.energy, Catch::Matchers::WithinAbs(baseline.energy, 1e-6f));

    CHECK(delayed.perfect_count == baseline.perfect_count);
    CHECK(advanced.perfect_count == baseline.perfect_count);
    CHECK(delayed.miss_count == baseline.miss_count);
    CHECK(advanced.miss_count == baseline.miss_count);
}
