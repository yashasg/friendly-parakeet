#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "ui/screen_controllers/gameplay_hud_screen_controller.h"
#include "util/rhythm_math.h"
#include "entities/beat_map.h"

using Catch::Matchers::WithinAbs;

// Beat Map Parsing

TEST_CASE("beatmap: parse minimal valid JSON", "[rhythm][beatmap]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "test_song",
        "bpm": 120,
        "offset": 0.2,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 4, "kind": "shape_gate", "shape": "circle", "lane": 1 },
            { "beat": 8, "kind": "shape_gate", "shape": "triangle", "lane": 1 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    CHECK(errors.empty());
    CHECK(map.song_id == "test_song");
    CHECK(map.bpm == 120.0f);
    CHECK(map.offset == 0.2f);
    CHECK(map.lead_beats == 4);
    CHECK(map.beats.size() == 2);
    CHECK(map.beats[0].beat_index == 4);
    CHECK(map.beats[0].kind == ObstacleKind::ShapeGate);
    CHECK(map.beats[0].shape == Shape::Circle);
    CHECK(map.beats[1].beat_index == 8);
    CHECK(map.beats[1].shape == Shape::Triangle);
}

TEST_CASE("beatmap: parse active obstacle kinds", "[rhythm][beatmap][issue873]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0, "lead_beats": 4, "duration_sec": 60,
        "beats": [
            { "beat": 4,  "kind": "shape_gate",  "shape": "circle",   "lane": 1 },
            { "beat": 8,  "kind": "onset_marker", "time_sec": 4.0 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 2);
    CHECK(map.beats[0].kind == ObstacleKind::ShapeGate);
    CHECK(map.beats[1].kind == ObstacleKind::OnsetMarker);
}

TEST_CASE("beatmap: tempo_changes in JSON are ignored", "[rhythm][beatmap]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0, "lead_beats": 4, "duration_sec": 120,
        "tempo_changes": [
            { "beat": 64, "bpm": 140 }
        ],
        "beats": [
            { "beat": 4, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    CHECK(map.bpm == 120.0f);
    CHECK(map.beats.size() == 1);
}

TEST_CASE("beatmap: invalid JSON returns error", "[rhythm][beatmap]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    REQUIRE_FALSE(parse_beat_map("{ invalid json }", map, errors));
    REQUIRE_FALSE(errors.empty());
}

TEST_CASE("beatmap: beats sorted by beat_index", "[rhythm][beatmap]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0, "lead_beats": 4, "duration_sec": 60,
        "beats": [
            { "beat": 12, "kind": "shape_gate", "shape": "square", "lane": 1 },
            { "beat": 4,  "kind": "shape_gate", "shape": "circle", "lane": 1 },
            { "beat": 8,  "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    CHECK(map.beats[0].beat_index == 4);
    CHECK(map.beats[1].beat_index == 8);
    CHECK(map.beats[2].beat_index == 12);
}

// Beat Map Validation

TEST_CASE("beatmap: validate BPM range", "[rhythm][beatmap]") {
    BeatMap map;
    map.bpm = 50.0f; map.offset = 0.0f; map.lead_beats = 4; map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));

    map.bpm = 350.0f; errors.clear();
    CHECK_FALSE(validate_beat_map(map, errors));

    map.bpm = 120.0f; errors.clear();
    CHECK(validate_beat_map(map, errors));
}

TEST_CASE("beatmap: validate offset range", "[rhythm][beatmap]") {
    BeatMap map;
    map.bpm = 120.0f; map.offset = -1.0f; map.lead_beats = 4; map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("beatmap: validate lead_beats range", "[rhythm][beatmap]") {
    BeatMap map;
    map.bpm = 120.0f; map.offset = 0.0f; map.lead_beats = 1; map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("beatmap: validate empty beats rejected", "[rhythm][beatmap]") {
    BeatMap map;
    map.bpm = 120.0f; map.offset = 0.0f; map.lead_beats = 4; map.duration = 60.0f;
    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("beatmap: validate different-shape gates min spacing", "[rhythm][beatmap]") {
    BeatMap map;
    map.bpm = 120.0f; map.offset = 0.0f; map.lead_beats = 4; map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({5, ObstacleKind::ShapeGate, Shape::Triangle, 1, 0});
    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

TEST_CASE("beatmap: same-shape gates can be adjacent", "[rhythm][beatmap]") {
    BeatMap map;
    map.bpm = 120.0f; map.offset = 0.0f; map.lead_beats = 4; map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({5, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
}

TEST_CASE("beatmap: validate beat index beyond duration", "[rhythm][beatmap]") {
    BeatMap map;
    map.bpm = 120.0f; map.offset = 0.0f; map.lead_beats = 4; map.duration = 10.0f;
    map.beats.push_back({100, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
}

// SongState Initialization

TEST_CASE("song_state: derived values at 120 BPM", "[rhythm][songstate]") {
    SongState state;
    state.bpm = 120.0f; state.offset = 0.2f; state.lead_beats = 4; state.duration_sec = 180.0f;
    song_state_compute_derived(state);
    CHECK_THAT(state.beat_period, WithinAbs(0.5f, 0.001f));
    CHECK_THAT(state.lead_time, WithinAbs(2.0f, 0.001f));
    CHECK_THAT(state.scroll_speed, WithinAbs(constants::APPROACH_DIST / 2.0f, 1.0f));
    CHECK_THAT(state.window_duration, WithinAbs(0.3f, 0.001f));
    CHECK_THAT(state.half_window, WithinAbs(0.15f, 0.001f));
}

TEST_CASE("song_state: derived values at 80 BPM", "[rhythm][songstate]") {
    SongState state;
    state.bpm = 80.0f; state.lead_beats = 4;
    song_state_compute_derived(state);
    CHECK_THAT(state.beat_period, WithinAbs(0.75f, 0.001f));
    CHECK_THAT(state.lead_time, WithinAbs(3.0f, 0.001f));
    CHECK_THAT(state.scroll_speed, WithinAbs(constants::APPROACH_DIST / 3.0f, 1.0f));
    CHECK_THAT(state.window_duration, WithinAbs(0.3f, 0.001f));
}

TEST_CASE("song_state: window duration floor at high BPM", "[rhythm][songstate]") {
    SongState state;
    state.bpm = 300.0f; state.lead_beats = 4;
    song_state_compute_derived(state);
    CHECK_THAT(state.window_duration, WithinAbs(0.3f, 0.001f));
}

TEST_CASE("song_state: init from beat map", "[rhythm][songstate]") {
    BeatMap map;
    map.bpm = 140.0f; map.offset = 0.3f; map.lead_beats = 3; map.duration = 120.0f;
    SongState state;
    init_song_state(state, map);
    CHECK(state.bpm == 140.0f);
    CHECK(state.offset == 0.3f);
    CHECK_FALSE(state.playing);
    CHECK(state.song_time == 0.0f);
    CHECK_THAT(state.beat_period, WithinAbs(60.0f / 140.0f, 0.001f));
}

// Song Playback System

TEST_CASE("song_playback: advances song_time", "[rhythm][playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.playing = true; song.song_time = 0.0f;
    song_playback_system(reg, 0.016f);
    CHECK_THAT(song.song_time, WithinAbs(0.016f, 0.001f));
}

TEST_CASE("song_playback: increments current_beat", "[rhythm][playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.playing = true; song.offset = 0.0f; song.song_time = 0.0f;
    song_playback_system(reg, 0.5f);
    CHECK(song.current_beat >= 1);
}

TEST_CASE("song_playback: detects song end", "[rhythm][playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.playing = true; song.duration_sec = 1.0f; song.song_time = 0.99f;
    song_playback_system(reg, 0.02f);
    CHECK(song.finished);
    CHECK_FALSE(song.playing);
}

TEST_CASE("song_playback: does nothing when not playing", "[rhythm][playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.playing = false; song.song_time = 0.0f;
    song_playback_system(reg, 0.016f);
    CHECK(song.song_time == 0.0f);
}

// Beat Scheduler System

TEST_CASE("beat_scheduler: spawns obstacle at spawn_time", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true;
    // Advance past spawn_time
    song.song_time = 0.1f;
    beat_scheduler_system(reg, 0.016f);
    CHECK(reg.view<ObstacleTag>().size() == 1);
}

TEST_CASE("beat_scheduler: obstacle has correct BeatInfo", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);
    map.beats.push_back({8, ObstacleKind::ShapeGate, Shape::Triangle, 1, 0});
    song.playing = true; song.song_time = 2.5f;
    beat_scheduler_system(reg, 0.016f);
    auto obs_view = reg.view<ObstacleTag, BeatInfo>();
    int obs_count = 0; for (auto _ : obs_view) { (void)_; obs_count++; }
    REQUIRE(obs_count == 1);
    for (auto [e, bi] : obs_view.each()) {
        CHECK(bi.beat_index == 8);
        CHECK_THAT(bi.arrival_time, WithinAbs(4.0f, 0.001f));
    }
}

TEST_CASE("beat_scheduler: does not spawn before spawn_time", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);
    map.beats.push_back({20, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 0.5f;
    beat_scheduler_system(reg, 0.016f);
    CHECK(reg.view<ObstacleTag>().size() == 0);
}

TEST_CASE("beat_scheduler: spawns multiple when time catches up", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({8, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 3.0f;
    beat_scheduler_system(reg, 0.016f);
    CHECK(reg.view<ObstacleTag>().size() == 2);
}

TEST_CASE("beat_scheduler: rhythm obstacles use BeatInfo without MotionVelocity", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 0.1f;
    beat_scheduler_system(reg, 0.016f);
    auto obs_view = reg.view<ObstacleTag, BeatInfo>();
    REQUIRE(std::distance(obs_view.begin(), obs_view.end()) == 1);
    for (auto [e, info] : obs_view.each()) {
        (void)info;
        CHECK_FALSE(reg.all_of<MotionVelocity>(e));
    }
    auto invalid_view = reg.view<ObstacleTag, BeatInfo, MotionVelocity>();
    CHECK(std::distance(invalid_view.begin(), invalid_view.end()) == 0);
}

// Shape Window System

TEST_CASE("shape_window: idle does nothing", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    reg.ctx().get<SongState>().song_time += 0.016f;
    shape_window_system(reg, 0.016f);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(sw.phase == WindowPhase::Idle);
}

TEST_CASE("shape_window: morph_in transitions to active", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    sw.phase = WindowPhase::MorphIn;
    sw.target_shape = Shape::Triangle;
    sw.window_timer = 0.0f; ps.morph_t = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    reg.ctx().get<SongState>().song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(ps.current == Shape::Triangle);
    CHECK(sw.phase == WindowPhase::Active);
}

TEST_CASE("shape_window: active transitions to morph_out", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    sw.phase = WindowPhase::Active;
    ps.current = Shape::Triangle; sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    reg.ctx().get<SongState>().song_time += song.window_duration + 0.01f;
    shape_window_system(reg, song.window_duration + 0.01f);
    CHECK(sw.phase == WindowPhase::MorphOut);
}

TEST_CASE("shape_window: morph_out reverts to hexagon", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    sw.phase = WindowPhase::MorphOut;
    ps.current = Shape::Triangle; sw.window_timer = 0.0f;
    sw.window_start = reg.ctx().get<SongState>().song_time;
    reg.ctx().get<SongState>().song_time += song.morph_duration + 0.01f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(sw.phase == WindowPhase::Idle);
}

TEST_CASE("shape_window: full lifecycle", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    sw.phase = WindowPhase::MorphIn;
    sw.target_shape = Shape::Square;
    float morph_dt = song.morph_duration / 5.0f;
    for (int i = 0; i < 6; ++i) { reg.ctx().get<SongState>().song_time += morph_dt; shape_window_system(reg, morph_dt); }
    CHECK(sw.phase == WindowPhase::Active);
    CHECK(ps.current == Shape::Square);
    float active_dt = song.window_duration / 10.0f;
    for (int i = 0; i < 11; ++i) { reg.ctx().get<SongState>().song_time += active_dt; shape_window_system(reg, active_dt); }
    CHECK(sw.phase == WindowPhase::MorphOut);
    for (int i = 0; i < 6; ++i) { reg.ctx().get<SongState>().song_time += morph_dt; shape_window_system(reg, morph_dt); }
    CHECK(ps.current == Shape::Hexagon);
    CHECK(sw.phase == WindowPhase::Idle);
}

// Player Action System (Rhythm Mode)

TEST_CASE("player_action: button press starts MorphIn window in rhythm mode", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);
    run_semantic_input_tick(reg, 0.016f);
    auto& sw = reg.get<ShapeWindow>(player);
    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Triangle);
}

TEST_CASE("player_action: press_time recorded correctly", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 5.0f;
    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    run_semantic_input_tick(reg, 0.016f);
    auto& sw = reg.get<ShapeWindow>(player);
    CHECK_THAT(sw.press_time, WithinAbs(5.0f, 0.001f));
}

TEST_CASE("player_action: same shape during active is ignored", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    sw.phase = WindowPhase::Active;
    ps.current = Shape::Triangle; sw.target_shape = Shape::Triangle;
    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);
    run_semantic_input_tick(reg, 0.016f);
    CHECK(sw.phase == WindowPhase::Active);
}

TEST_CASE("player_action: different shape mid-window restarts", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    sw.phase = WindowPhase::Active;
    ps.current = Shape::Triangle; sw.target_shape = Shape::Triangle;
    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    run_semantic_input_tick(reg, 0.016f);
    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Circle);
}

TEST_CASE("player_action: no action with empty queue", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    run_semantic_input_tick(reg, 0.016f);
    auto& sw = reg.get<ShapeWindow>(player);
    CHECK(sw.phase == WindowPhase::Idle);
}

TEST_CASE("player_action: legacy mode instant shape change", "[rhythm][action]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);
    run_semantic_input_tick(reg, 0.016f);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    CHECK(ps.current == Shape::Triangle);
    CHECK(sw.phase == WindowPhase::Idle);
}

// Collision System with Timing Grades

TEST_CASE("collision: hexagon fails shape gates — drains energy", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);
    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 1);
}

TEST_CASE("collision: MISS drains energy", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);
    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: timing grade PERFECT on press-at-arrival", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f; sw.press_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    // arrival_time == press_time -> Perfect
    reg.emplace<BeatInfo>(obs, 0, 5.0f, 5.0f - song.lead_time);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<ScoredTag>(obs));
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Perfect);
}

TEST_CASE("collision: HUD perfect cue distance maps to PERFECT timing", "[rhythm][collision][hud]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    const float cue_dist = gameplay_hud_perfect_distance(&song);
    const float cue_lead_seconds = cue_dist / song.scroll_speed;
    CHECK_THAT(cue_lead_seconds, WithinAbs(kTimingPerfectSeconds, 0.0001f));
    CHECK_THAT(gameplay_hud_good_distance(&song) / song.scroll_speed,
               WithinAbs(kTimingGoodSeconds, 0.0001f));
    CHECK_THAT(gameplay_hud_ok_distance(&song) / song.scroll_speed,
               WithinAbs(kTimingOkSeconds, 0.0001f));

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.press_time = 5.0f;
    song.song_time = sw.press_time + cue_lead_seconds;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<ScoredTag>(obs));
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Perfect);
}

TEST_CASE("collision: timing grade GOOD at 50pct", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f; sw.press_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    // arrival_time offset by 50% of half_window (~75ms) -> Good
    reg.emplace<BeatInfo>(obs, 0, 5.0f + song.half_window * 0.5f, 0.0f);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Good);
}

TEST_CASE("collision: timing grade OK at 80pct", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f; sw.press_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    // arrival_time offset by 80% of half_window (~120ms) -> Ok
    reg.emplace<BeatInfo>(obs, 0, 5.0f + song.half_window * 0.8f, 0.0f);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Ok);
}

TEST_CASE("collision: timing grade BAD beyond window", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f; sw.press_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    // arrival_time offset by 120% of half_window (>150ms) -> Bad
    reg.emplace<BeatInfo>(obs, 0, 5.0f + song.half_window * 1.2f, 0.0f);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Bad);
}

TEST_CASE("collision: stale press from previous beat does not get Perfect", "[rhythm][collision]") {
    // Regression: pressing on beat N and cruising through beat N+2 should
    // NOT award Perfect for beat N+2. Timing is graded from button press time.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Player pressed Circle at song_time 2.0.
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.press_time = 2.0f;
    sw.window_start = 2.0f;
    sw.graded = false;

    // Now it's beat N+2 (time 3.5 at 120 BPM = 0.5s per beat)
    song.song_time = 3.5f;

    // Obstacle for beat N+2 arrives at 3.5
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 7, 3.5f, 3.5f - song.lead_time);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<TimingGrade>(obs));
    // press_time (2.0) is far from arrival (3.5) -> not Perfect
    CHECK(reg.get<TimingGrade>(obs).tier != TimingTier::Perfect);
}

TEST_CASE("collision: PERFECT clears obstacle without game over", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f; sw.press_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, 5.0f, 5.0f - song.lead_time);
    collision_system(reg, 0.016f);
    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: SongResults updated", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f; sw.press_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, 5.0f, 5.0f - song.lead_time);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    CHECK(reg.ctx().get<SongResults>().perfect_count == 1);
}

// Energy System

TEST_CASE("game_state: triggers game over at 0 energy", "[rhythm][energy]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<EnergyState>().energy = 0.0f;
    game_state_system(reg, 0.016f);
    CHECK(reg.ctx().get<GameState>().transition_pending);
    CHECK(reg.ctx().get<GameState>().next_phase == GamePhase::GameOver);
}

TEST_CASE("energy_system: does nothing when energy is positive", "[rhythm][energy]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<EnergyState>().energy = 0.5f;
    energy_system(reg, 0.016f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("game_state: enter_game_over marks song finished on depletion", "[rhythm][energy]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<EnergyState>().energy = 0.0f;
    game_state_system(reg, 0.016f);
    game_state_system(reg, 0.016f);
    auto& song = reg.ctx().get<SongState>();
    CHECK(song.finished);
    CHECK_FALSE(song.playing);
}

// Scoring with Timing

TEST_CASE("scoring: timing_mult applied to scored obstacle", "[rhythm][scoring]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 0; score.chain_count = 0;
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<Obstacle>(obs, int16_t{200});
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Perfect, 1.0f);
    int prev = score.score;
    scoring_system(reg, 0.016f);
    int gained = score.score - prev;
    int dt_bonus = static_cast<int>(0.016f * constants::PTS_PER_SECOND);
    CHECK(gained >= 300 + dt_bonus);
}

// Timing Helpers


TEST_CASE("timing: timing_multiplier values", "[rhythm][timing]") {
    CHECK(timing_multiplier(TimingTier::Perfect) == 1.50f);
    CHECK(timing_multiplier(TimingTier::Good) == 1.00f);
    CHECK(timing_multiplier(TimingTier::Ok) == 0.50f);
    CHECK(timing_multiplier(TimingTier::Bad) == 0.25f);
}

TEST_CASE("timing: window_scale_for_tier values", "[rhythm][timing]") {
    // Post-#223 inversion: smaller scale = better timing = faster window collapse.
    CHECK(window_scale_for_tier(TimingTier::Perfect) == 0.50f);
    CHECK(window_scale_for_tier(TimingTier::Good) == 0.75f);
    CHECK(window_scale_for_tier(TimingTier::Ok) == 1.00f);
    CHECK(window_scale_for_tier(TimingTier::Bad) == 1.00f);
}

// Window Scaling

TEST_CASE("window_scaling: PERFECT grade shortens remaining window", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    // Put player in Active phase, midway through window
    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.window_timer = song.window_duration * 0.5f; // halfway through
    song.song_time = 5.0f;
    sw.press_time = 5.0f; // PERFECT timing
    sw.window_start = song.song_time - sw.window_timer;

    float start_before = sw.window_start;
    float timer_before = sw.window_timer;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, 5.0f, 5.0f - song.lead_time);
    collision_system(reg, 0.016f);

    CHECK(sw.graded);
    // Post-#223: Perfect scale = 0.50 (shrinks remaining window by 50%)
    // window_start moved backward so remaining Active window expires at 50%
    float remaining = song.window_duration - timer_before;
    float expected_shift = remaining * 0.50f;
    CHECK_THAT(sw.window_start, WithinAbs(start_before - expected_shift, 0.001f));
    // window_timer is not touched by collision_system
    CHECK_THAT(sw.window_timer, WithinAbs(timer_before, 0.001f));
}

TEST_CASE("window_scaling: GOOD grade shortens window slightly", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.window_timer = song.window_duration * 0.4f;
    song.song_time = 5.0f;
    sw.press_time = 5.0f;
    sw.window_start = song.song_time - sw.window_timer;

    float start_before = sw.window_start;
    float timer_before = sw.window_timer;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    // arrival offset 50% from press -> Good
    reg.emplace<BeatInfo>(obs, 0, 5.0f + song.half_window * 0.5f, 0.0f);
    collision_system(reg, 0.016f);

    CHECK(sw.graded);
    // Post-#223: Good scale = 0.75 (shrinks remaining window by 25%)
    float remaining = song.window_duration - timer_before;
    float expected_shift = remaining * 0.25f;
    CHECK_THAT(sw.window_start, WithinAbs(start_before - expected_shift, 0.001f));
    CHECK_THAT(sw.window_timer, WithinAbs(timer_before, 0.001f));
}

TEST_CASE("window_scaling: OK grade keeps window unchanged", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f;
    sw.window_timer = song.window_duration * 0.3f;
    // Keep window_start consistent with song_time and window_timer
    sw.window_start = song.song_time - sw.window_timer;
    sw.press_time = 5.0f;

    float start_before = sw.window_start;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    // arrival offset 80% from press -> Ok
    reg.emplace<BeatInfo>(obs, 0, 5.0f + song.half_window * 0.8f, 0.0f);
    collision_system(reg, 0.016f);

    CHECK(sw.graded);
    // Post-#223: Ok scale = 1.0 → no window adjustment
    CHECK_THAT(sw.window_start, WithinAbs(start_before, 0.001f));
    // window_timer must NOT be changed by collision_system
    CHECK_THAT(sw.window_timer, WithinAbs(song.window_duration * 0.3f, 0.001f));
}

TEST_CASE("window_scaling: BAD grade keeps window unchanged", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    song.song_time = 5.0f;
    sw.window_timer = song.window_duration * 0.2f;
    // Keep window_start consistent with song_time and window_timer
    sw.window_start = song.song_time - sw.window_timer;
    sw.press_time = 5.0f;

    float start_before = sw.window_start;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    // arrival offset 120% from press -> Bad
    reg.emplace<BeatInfo>(obs, 0, 5.0f + song.half_window * 1.2f, 0.0f);
    collision_system(reg, 0.016f);

    CHECK(sw.graded);
    // Post-#223: Bad scale = 1.0 → no window adjustment
    CHECK_THAT(sw.window_start, WithinAbs(start_before, 0.001f));
    // window_timer must NOT be changed by collision_system
    CHECK_THAT(sw.window_timer, WithinAbs(song.window_duration * 0.2f, 0.001f));
}

TEST_CASE("window_scaling: second obstacle does not re-scale", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    sw.phase = WindowPhase::Active;
    sw.window_timer = song.window_duration * 0.4f;
    song.song_time = 5.0f;
    sw.press_time = 5.0f;

    // First obstacle grades PERFECT, jumps timer
    auto obs1 = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs1, 0, 5.0f, 5.0f - song.lead_time);
    collision_system(reg, 0.016f);
    float timer_after_first = sw.window_timer;
    CHECK(sw.graded);

    // Second obstacle should NOT jump timer again
    auto obs2 = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs2, 1, 5.0f, 5.0f - song.lead_time);
    collision_system(reg, 0.016f);
    CHECK_THAT(sw.window_timer, WithinAbs(timer_after_first, 0.001f));
}

TEST_CASE("window_scaling: graded resets on new window", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);

    // Simulate a graded window
    sw.graded = true;

    // Start a new window via semantic input drain
    auto btn = make_shape_button(reg, Shape::Triangle);
    press_button(reg, btn);
    sw.phase = WindowPhase::Idle;
    run_semantic_input_tick(reg, 0.016f);

    CHECK_FALSE(sw.graded);
}

// Integration: obstacle arrives on-beat

TEST_CASE("integration: obstacle arrives on-beat within 1 frame", "[rhythm][integration]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 0.1f;
    constexpr float dt = 1.0f / 60.0f;
    bool obstacle_at_player = false;
    int frames = 0;
    while (frames < 300) {
        // Advance song_time once per frame (no music loaded → manual advance)
        song.song_time += dt;
        beat_scheduler_system(reg, dt);
        scroll_system(reg, dt);
        frames++;
        auto view = reg.view<ObstacleTag, WorldTransform>();
        for (auto [e, wt] : view.each()) {
            if (wt.position.y >= constants::PLAYER_Y) {
                obstacle_at_player = true;
                float beat_time = song.offset + 4 * song.beat_period;
                CHECK_THAT(song.song_time, WithinAbs(beat_time, dt + 0.001f));
                break;
            }
        }
        if (obstacle_at_player) break;
    }
    CHECK(obstacle_at_player);
}
