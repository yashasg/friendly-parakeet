// Regression tests for #474: SettingsState::audio_offset_ms is now consumed by
// beat_scheduler_system. A non-zero offset shifts spawn_time (and therefore
// the obstacle's beat-crossing event) by the same delta. Positive offset
// delays beat timing (matches settings.h docstring).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "test_helpers.h"
#include "entities/beat_map.h"
#include "components/player.h"
#include "components/rhythm.h"
#include "systems/shape_mesh.h"
#include "components/camera_resources.h"
#include "systems/all_systems.h"
#include "systems/runtime_systems.h"
#include "util/rhythm_math.h"
#include "entities/settings.h"

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

    auto& settings = settings_state(reg);
    settings.audio_offset_ms = audio_offset_ms;

    auto& song = reg.ctx().get<SongState>();
    song.bpm    = bpm;
    song.offset = beatmap_offset_seconds;
    song_state_compute_derived(song);
    song.song_time = song_time_when_scheduling;

    auto& map = beat_map(reg);
    map.shape_gate_circle_beats.push_back({beat_index, 1});

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

int count_obstacles(entt::registry& reg) {
    int count = 0;
    for (auto entity : reg.view<ObstacleTag>()) {
        (void)entity;
        ++count;
    }
    return count;
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

TEST_CASE("song_playback: positive audio_offset delays beat crossing",
          "[song_playback][audio_offset][issue210]") {
    auto reg = make_rhythm_registry();
    auto& settings = settings_state(reg);
    settings.audio_offset_ms = +200;

    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    set_beat_cursor(reg, -1);

    auto& map = beat_map(reg);
    map.beat_times = {1.0f};

    song_playback_system(reg, 1.1f);
    CHECK(beat_cursor_value(reg) == -1);

    song_playback_system(reg, 0.1f);
    CHECK(beat_cursor_value(reg) == 0);
}

TEST_CASE("song_playback: negative audio_offset advances beat crossing",
          "[song_playback][audio_offset][issue210]") {
    auto reg = make_rhythm_registry();
    auto& settings = settings_state(reg);
    settings.audio_offset_ms = -200;

    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    set_beat_cursor(reg, -1);

    auto& map = beat_map(reg);
    map.beat_times = {1.0f};

    song_playback_system(reg, 0.85f);
    CHECK(beat_cursor_value(reg) == 0);
}

TEST_CASE("beat_scheduler: audio_offset gates spawn before calibrated spawn time",
          "[beat_scheduler][audio_offset][issue210]") {
    auto reg = make_rhythm_registry();
    auto& settings = settings_state(reg);
    settings.audio_offset_ms = +200;

    auto& song = reg.ctx().get<SongState>();
    song.lead_time = 0.0f;
    song.song_time = 0.1f;
    song.next_shape_gate_circle_idx = 0;

    auto& map = beat_map(reg);
    map.shape_gate_circle_beats.push_back({0, 1});

    beat_scheduler_system(reg, 0.016f);
    CHECK(count_obstacles(reg) == 0);

    song.song_time = 0.2f;
    beat_scheduler_system(reg, 0.016f);
    CHECK(count_obstacles(reg) == 1);
}

TEST_CASE("audio_offset: zero setting matches absent SettingsState",
          "[song_playback][beat_scheduler][audio_offset][issue210]") {
    auto playback_with_zero = make_rhythm_registry();
    auto& zero_song = playback_with_zero.ctx().get<SongState>();
    zero_song.song_time = 0.0f;
    set_beat_cursor(playback_with_zero, -1);
    beat_map(playback_with_zero).beat_times = {0.4f, 0.9f, 1.6f};
    song_playback_system(playback_with_zero, 1.0f);

    auto playback_without_settings = make_rhythm_registry();
    destroy_settings_entity(playback_without_settings);
    auto& absent_song = playback_without_settings.ctx().get<SongState>();
    absent_song.song_time = 0.0f;
    set_beat_cursor(playback_without_settings, -1);
    beat_map(playback_without_settings).beat_times = {0.4f, 0.9f, 1.6f};
    song_playback_system(playback_without_settings, 1.0f);

    CHECK(beat_cursor_value(playback_without_settings) ==
          beat_cursor_value(playback_with_zero));

    auto scheduler_with_zero = make_rhythm_registry();
    auto& zero_schedule_song = scheduler_with_zero.ctx().get<SongState>();
    zero_schedule_song.song_time = 1.0f;
    zero_schedule_song.next_shape_gate_circle_idx = 0;
    beat_map(scheduler_with_zero).shape_gate_circle_beats.push_back(
        {2, 1});
    beat_scheduler_system(scheduler_with_zero, 0.016f);

    auto scheduler_without_settings = make_rhythm_registry();
    destroy_settings_entity(scheduler_without_settings);
    auto& absent_schedule_song = scheduler_without_settings.ctx().get<SongState>();
    absent_schedule_song.song_time = 1.0f;
    absent_schedule_song.next_shape_gate_circle_idx = 0;
    beat_map(scheduler_without_settings).shape_gate_circle_beats.push_back(
        {2, 1});
    beat_scheduler_system(scheduler_without_settings, 0.016f);

    CHECK(absent_schedule_song.next_shape_gate_circle_idx == zero_schedule_song.next_shape_gate_circle_idx);
    CHECK(count_obstacles(scheduler_without_settings) == count_obstacles(scheduler_with_zero));
}


namespace {

struct OffsetGameplayResult {
    bool perfect{false};
    bool good{false};
    bool ok{false};
    bool bad{false};
    int score = 0;
    float energy = 0.0f;
    int perfect_count = 0;
    int miss_count = 0;
    float arrival_time = 0.0f;
    float visual_arrival_time = 0.0f;
};

OffsetGameplayResult run_calibrated_on_arrival_hit(int16_t audio_offset_ms) {
    auto reg = make_rhythm_registry();

    auto& settings = settings_state(reg);
    settings.audio_offset_ms = audio_offset_ms;

    auto player = make_rhythm_player(reg);
    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);

    auto& song = reg.ctx().get<SongState>();
    song.bpm    = 120.0f;
    song.offset = 1.0f;
    song_state_compute_derived(song);

    constexpr int kBeatIndex = 4;
    auto& map = beat_map(reg);
    map.shape_gate_circle_beats.push_back({kBeatIndex, 1});

    const float beat_time = song.offset + static_cast<float>(kBeatIndex) * song.beat_period;
    const float spawn_time = beat_time - song.lead_time + settings::audio_offset_seconds(settings);
    song.song_time = spawn_time + 0.05f;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    REQUIRE(view.begin() != view.end());
    auto obstacle = *view.begin();

    auto& info = reg.get<BeatInfo>(obstacle);
    auto& wt = reg.get<WorldPosition>(obstacle);
    wt.position.y = constants::PLAYER_Y;

    reg.emplace_or_replace<Pressed>(player, Pressed{info.spawn_time + song.lead_time});

    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<ScoredTag>(obstacle));
    REQUIRE(reg.all_of<TimingGrade>(obstacle));

    OffsetGameplayResult out;
    out.perfect = reg.all_of<TimingPerfectTag>(obstacle);
    out.good    = reg.all_of<TimingGoodTag>(obstacle);
    out.ok      = reg.all_of<TimingOkTag>(obstacle);
    out.bad     = reg.all_of<TimingBadTag>(obstacle);

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    const auto& score = reg.ctx().get<ScoreState>();
    const auto& energy = reg.ctx().get<EnergyState>();
    const auto& results = reg.ctx().get<SongResults>();

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

    CHECK(baseline.perfect);
    CHECK(delayed.perfect);
    CHECK(advanced.perfect);

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

TEST_CASE("floor beat visuals use calibrated beat timing",
          "[floor_visuals][audio_offset][issue810]") {
    constexpr float song_time = 1.25f;
    constexpr float beat_time = 1.00f;
    constexpr float scroll_speed = 100.0f;

    const float baseline_z =
        floor_visuals::beat_line_z(song_time, beat_time, scroll_speed, 0.0f);
    const float delayed_z =
        floor_visuals::beat_line_z(song_time, beat_time, scroll_speed, +0.25f);
    const float advanced_z =
        floor_visuals::beat_line_z(song_time, beat_time, scroll_speed, -0.25f);

    CHECK_THAT(delayed_z - baseline_z, Catch::Matchers::WithinAbs(-25.0f, 1e-4f));
    CHECK_THAT(advanced_z - baseline_z, Catch::Matchers::WithinAbs(+25.0f, 1e-4f));

    CHECK_THAT(floor_visuals::pulse_for_beat(song_time, beat_time, +0.25f),
               Catch::Matchers::WithinAbs(1.0f, 1e-6f));
    CHECK(floor_visuals::pulse_for_beat(song_time, beat_time, 0.0f)
          < floor_visuals::pulse_for_beat(song_time, beat_time, +0.25f));
    CHECK_THAT(floor_visuals::calibrated_beat_time(beat_time, -0.25f),
               Catch::Matchers::WithinAbs(0.75f, 1e-6f));
}

TEST_CASE("game camera floor pulse honors audio_offset_ms",
          "[camera3d][floor_visuals][audio_offset][issue810]") {
    auto reg = make_rhythm_registry();
    reg.ctx().emplace<FloorParams>();
    reg.ctx().emplace<ShapeMeshConfig>();

    auto& settings = settings_state(reg);
    settings.audio_offset_ms = +250;

    auto& song = reg.ctx().get<SongState>();
    song.song_time = 1.25f;
    set_beat_cursor(reg, 0);
    song.playing = true;

    auto& map = beat_map(reg);
    map.beat_times = {1.0f};

    game_camera_system(reg, 0.0f);
    const auto delayed_alpha = reg.ctx().get<FloorParams>().alpha;

    settings.audio_offset_ms = 0;
    game_camera_system(reg, 0.0f);
    const auto baseline_alpha = reg.ctx().get<FloorParams>().alpha;

    CHECK(delayed_alpha > baseline_alpha);
    CHECK(delayed_alpha == static_cast<uint8_t>(constants::FLOOR_ALPHA_PEAK));
}
