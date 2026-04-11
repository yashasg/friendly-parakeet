#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "beat_map_loader.h"

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

TEST_CASE("beatmap: parse all obstacle kinds", "[rhythm][beatmap]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0, "lead_beats": 4, "duration_sec": 60,
        "beats": [
            { "beat": 4,  "kind": "shape_gate",  "shape": "circle",   "lane": 1 },
            { "beat": 8,  "kind": "lane_block",   "blocked": [0, 2] },
            { "beat": 12, "kind": "low_bar" },
            { "beat": 16, "kind": "high_bar" },
            { "beat": 20, "kind": "combo_gate",  "shape": "square", "blocked": [0, 1] },
            { "beat": 24, "kind": "split_path",  "shape": "triangle", "lane": 2 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 6);
    CHECK(map.beats[0].kind == ObstacleKind::ShapeGate);
    CHECK(map.beats[1].kind == ObstacleKind::LaneBlock);
    CHECK(map.beats[1].blocked_mask == 0b101);
    CHECK(map.beats[2].kind == ObstacleKind::LowBar);
    CHECK(map.beats[3].kind == ObstacleKind::HighBar);
    CHECK(map.beats[4].kind == ObstacleKind::ComboGate);
    CHECK(map.beats[4].blocked_mask == 0b011);
    CHECK(map.beats[5].kind == ObstacleKind::SplitPath);
    CHECK(map.beats[5].lane == 2);
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
    CHECK_THAT(state.scroll_speed, WithinAbs(520.0f, 1.0f));
    CHECK_THAT(state.window_duration, WithinAbs(0.8f, 0.001f));
    CHECK_THAT(state.half_window, WithinAbs(0.4f, 0.001f));
}

TEST_CASE("song_state: derived values at 80 BPM", "[rhythm][songstate]") {
    SongState state;
    state.bpm = 80.0f; state.lead_beats = 4;
    song_state_compute_derived(state);
    CHECK_THAT(state.beat_period, WithinAbs(0.75f, 0.001f));
    CHECK_THAT(state.lead_time, WithinAbs(3.0f, 0.001f));
    CHECK_THAT(state.scroll_speed, WithinAbs(346.67f, 1.0f));
    CHECK_THAT(state.window_duration, WithinAbs(1.2f, 0.001f));
}

TEST_CASE("song_state: window duration floor at high BPM", "[rhythm][songstate]") {
    SongState state;
    state.bpm = 300.0f; state.lead_beats = 4;
    song_state_compute_derived(state);
    CHECK(state.window_duration >= 0.36f);
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
    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 0.0f;
    beat_scheduler_system(reg, 0.016f);
    CHECK(reg.view<ObstacleTag>().size() == 1);
}

TEST_CASE("beat_scheduler: obstacle has correct BeatInfo", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();
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
    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({20, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 0.5f;
    beat_scheduler_system(reg, 0.016f);
    CHECK(reg.view<ObstacleTag>().size() == 0);
}

TEST_CASE("beat_scheduler: spawns multiple when time catches up", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({8, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 3.0f;
    beat_scheduler_system(reg, 0.016f);
    CHECK(reg.view<ObstacleTag>().size() == 2);
}

TEST_CASE("beat_scheduler: scroll speed matches song state", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 0.0f;
    beat_scheduler_system(reg, 0.016f);
    auto obs_view = reg.view<ObstacleTag, Velocity>();
    REQUIRE(std::distance(obs_view.begin(), obs_view.end()) == 1);
    for (auto [e, vel] : obs_view.each()) {
        CHECK_THAT(vel.dy, WithinAbs(song.scroll_speed, 0.1f));
    }
}

TEST_CASE("beat_scheduler: spawns lane_block with blocked mask", "[rhythm][scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();
    BeatEntry entry; entry.beat_index = 4; entry.kind = ObstacleKind::LaneBlock; entry.blocked_mask = 0b101;
    map.beats.push_back(entry);
    song.playing = true; song.song_time = 0.0f;
    beat_scheduler_system(reg, 0.016f);
    auto view = reg.view<ObstacleTag, BlockedLanes>();
    REQUIRE(std::distance(view.begin(), view.end()) == 1);
    for (auto [e, bl] : view.each()) { CHECK(bl.mask == 0b101); }
}

// Shape Window System

TEST_CASE("shape_window: idle does nothing", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    shape_window_system(reg, 0.016f);
    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
}

TEST_CASE("shape_window: morph_in transitions to active", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.target_shape = Shape::Triangle;
    ps.window_timer = 0.0f; ps.morph_t = 0.0f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(ps.current == Shape::Triangle);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
}

TEST_CASE("shape_window: active transitions to morph_out", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Triangle; ps.window_timer = 0.0f;
    shape_window_system(reg, song.window_duration + 0.01f);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));
}

TEST_CASE("shape_window: morph_out reverts to hexagon", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
    ps.current = Shape::Triangle; ps.window_timer = 0.0f;
    shape_window_system(reg, song.morph_duration + 0.01f);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
}

TEST_CASE("shape_window: full lifecycle", "[rhythm][window]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
    ps.target_shape = Shape::Square;
    float morph_dt = song.morph_duration / 5.0f;
    for (int i = 0; i < 6; ++i) shape_window_system(reg, morph_dt);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
    CHECK(ps.current == Shape::Square);
    float active_dt = song.window_duration / 10.0f;
    for (int i = 0; i < 11; ++i) shape_window_system(reg, active_dt);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::MorphOut));
    for (int i = 0; i < 6; ++i) shape_window_system(reg, morph_dt);
    CHECK(ps.current == Shape::Hexagon);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
}

// Player Action System (Rhythm Mode)

TEST_CASE("player_action: button press starts window in rhythm mode", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true; btn.shape = Shape::Triangle;
    player_action_system(reg, 0.016f);
    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
    CHECK(ps.target_shape == Shape::Triangle);
}

TEST_CASE("player_action: peak_time calculated correctly", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    song.song_time = 5.0f;
    btn.pressed = true; btn.shape = Shape::Circle;
    player_action_system(reg, 0.016f);
    auto& ps = reg.get<PlayerShape>(player);
    float expected_peak = 5.0f + song.morph_duration + song.half_window;
    CHECK_THAT(ps.peak_time, WithinAbs(expected_peak, 0.001f));
}

TEST_CASE("player_action: same shape during active is ignored", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Triangle; ps.target_shape = Shape::Triangle;
    btn.pressed = true; btn.shape = Shape::Triangle;
    player_action_system(reg, 0.016f);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Active));
}

TEST_CASE("player_action: different shape mid-window restarts", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.current = Shape::Triangle; ps.target_shape = Shape::Triangle;
    btn.pressed = true; btn.shape = Shape::Circle;
    player_action_system(reg, 0.016f);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::MorphIn));
    CHECK(ps.target_shape == Shape::Circle);
}

TEST_CASE("player_action: hexagon press is ignored", "[rhythm][action]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true; btn.shape = Shape::Hexagon;
    player_action_system(reg, 0.016f);
    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.phase_raw == static_cast<uint8_t>(WindowPhase::Idle));
}

TEST_CASE("player_action: legacy mode instant shape change", "[rhythm][action]") {
    auto reg = make_registry();
    auto player = make_player(reg);
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true; btn.shape = Shape::Triangle;
    player_action_system(reg, 0.016f);
    auto& ps = reg.get<PlayerShape>(player);
    CHECK(ps.current == Shape::Triangle);
    CHECK(ps.phase_raw == 0);
}

// Collision System with Timing Grades

TEST_CASE("collision: hexagon fails shape gates — game over", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 1);
}

TEST_CASE("collision: MISS is instant game over", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
}

TEST_CASE("collision: timing grade PERFECT at peak", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    song.song_time = 5.0f; ps.peak_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<ScoredTag>(obs));
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Perfect);
}

TEST_CASE("collision: timing grade GOOD at 30pct", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    song.song_time = 5.0f; ps.peak_time = 5.0f + song.half_window * 0.3f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Good);
}

TEST_CASE("collision: timing grade OK at 60pct", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    song.song_time = 5.0f; ps.peak_time = 5.0f + song.half_window * 0.6f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Ok);
}

TEST_CASE("collision: timing grade BAD at 80pct", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    song.song_time = 5.0f; ps.peak_time = 5.0f + song.half_window * 0.8f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.get<TimingGrade>(obs).tier == TimingTier::Bad);
}

TEST_CASE("collision: PERFECT clears obstacle without game over", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    song.song_time = 5.0f; ps.peak_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("collision: SongResults updated", "[rhythm][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();
    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    song.song_time = 5.0f; ps.peak_time = 5.0f;
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    CHECK(reg.ctx().get<SongResults>().perfect_count == 1);
}

// HP System

TEST_CASE("hp_system: triggers game over at 0 HP", "[rhythm][hp]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<HPState>().current = 0;
    hp_system(reg, 0.016f);
    CHECK(reg.ctx().get<GameState>().transition_pending);
    CHECK(reg.ctx().get<GameState>().next_phase == GamePhase::GameOver);
}

TEST_CASE("hp_system: does nothing when HP > 0", "[rhythm][hp]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<HPState>().current = 3;
    hp_system(reg, 0.016f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("hp_system: marks song finished on death", "[rhythm][hp]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<HPState>().current = 0;
    hp_system(reg, 0.016f);
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
    reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate, int16_t{200});
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Perfect, 1.0f);
    int prev = score.score;
    scoring_system(reg, 0.016f);
    int gained = score.score - prev;
    int dt_bonus = static_cast<int>(0.016f * constants::PTS_PER_SECOND);
    CHECK(gained >= 300 + dt_bonus);
}

// Obstacle Spawn Bypass

TEST_CASE("obstacle_spawn: bypassed when song playing", "[rhythm][spawn]") {
    auto reg = make_rhythm_registry();
    make_player(reg);
    int initial = static_cast<int>(reg.view<ObstacleTag>().size());
    for (int i = 0; i < 200; ++i) obstacle_spawn_system(reg, 0.016f);
    CHECK(static_cast<int>(reg.view<ObstacleTag>().size()) == initial);
}

TEST_CASE("obstacle_spawn: works in legacy mode", "[rhythm][spawn]") {
    auto reg = make_registry();
    make_player(reg);
    for (int i = 0; i < 200; ++i) obstacle_spawn_system(reg, 0.016f);
    CHECK(reg.view<ObstacleTag>().size() > 0);
}

// Timing Helpers

TEST_CASE("timing: compute_timing_tier thresholds", "[rhythm][timing]") {
    CHECK(compute_timing_tier(0.0f) == TimingTier::Perfect);
    CHECK(compute_timing_tier(0.10f) == TimingTier::Perfect);
    CHECK(compute_timing_tier(0.25f) == TimingTier::Perfect);
    CHECK(compute_timing_tier(0.26f) == TimingTier::Good);
    CHECK(compute_timing_tier(0.50f) == TimingTier::Good);
    CHECK(compute_timing_tier(0.51f) == TimingTier::Ok);
    CHECK(compute_timing_tier(0.75f) == TimingTier::Ok);
    CHECK(compute_timing_tier(0.76f) == TimingTier::Bad);
    CHECK(compute_timing_tier(1.0f) == TimingTier::Bad);
}

TEST_CASE("timing: timing_multiplier values", "[rhythm][timing]") {
    CHECK(timing_multiplier(TimingTier::Perfect) == 1.50f);
    CHECK(timing_multiplier(TimingTier::Good) == 1.00f);
    CHECK(timing_multiplier(TimingTier::Ok) == 0.50f);
    CHECK(timing_multiplier(TimingTier::Bad) == 0.25f);
}

TEST_CASE("timing: window_scale_for_tier values", "[rhythm][timing]") {
    CHECK(window_scale_for_tier(TimingTier::Perfect) == 1.50f);
    CHECK(window_scale_for_tier(TimingTier::Good) == 1.00f);
    CHECK(window_scale_for_tier(TimingTier::Ok) == 0.75f);
    CHECK(window_scale_for_tier(TimingTier::Bad) == 0.50f);
}

// Window Scaling

TEST_CASE("window_scaling: PERFECT grade shortens remaining window", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    // Put player in Active phase, midway through window
    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.window_timer = song.window_duration * 0.5f; // halfway through
    song.song_time = 5.0f;
    ps.peak_time = 5.0f; // PERFECT timing

    float timer_before = ps.window_timer;
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    CHECK(ps.graded);
    CHECK(ps.window_scale == 1.50f);
    // Perfect: scale > 1.0 means window extends. Timer not advanced.
    CHECK_THAT(ps.window_timer, WithinAbs(timer_before, 0.001f));
}

TEST_CASE("window_scaling: GOOD grade keeps normal window", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.window_timer = song.window_duration * 0.4f;
    song.song_time = 5.0f;
    ps.peak_time = 5.0f + song.half_window * 0.3f; // GOOD timing

    float timer_before = ps.window_timer;
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    CHECK(ps.graded);
    CHECK(ps.window_scale == 1.00f);
    // Good: scale=1.0, no timer change
    CHECK_THAT(ps.window_timer, WithinAbs(timer_before, 0.001f));
}

TEST_CASE("window_scaling: OK grade shortens window", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.window_timer = song.window_duration * 0.3f;
    song.song_time = 5.0f;
    ps.peak_time = 5.0f + song.half_window * 0.6f; // OK timing

    float timer_before = ps.window_timer;
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    CHECK(ps.graded);
    CHECK(ps.window_scale == 0.75f);
    // Ok: scale=0.75, timer advanced by remaining * 0.25
    float remaining = song.window_duration - timer_before;
    float expected_jump = remaining * 0.25f;
    CHECK_THAT(ps.window_timer, WithinAbs(timer_before + expected_jump, 0.001f));
}

TEST_CASE("window_scaling: BAD grade shortens window aggressively", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.window_timer = song.window_duration * 0.2f;
    song.song_time = 5.0f;
    ps.peak_time = 5.0f + song.half_window * 0.8f; // BAD timing

    float timer_before = ps.window_timer;
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    CHECK(ps.graded);
    CHECK(ps.window_scale == 0.50f);
    // Bad: scale=0.50, timer advanced by remaining * 0.50
    float remaining = song.window_duration - timer_before;
    float expected_jump = remaining * 0.50f;
    CHECK_THAT(ps.window_timer, WithinAbs(timer_before + expected_jump, 0.001f));
}

TEST_CASE("window_scaling: second obstacle does not re-scale", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& song = reg.ctx().get<SongState>();

    ps.current = Shape::Circle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
    ps.window_timer = song.window_duration * 0.4f;
    song.song_time = 5.0f;
    ps.peak_time = 5.0f;

    // First obstacle grades PERFECT, jumps timer
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    float timer_after_first = ps.window_timer;
    CHECK(ps.graded);

    // Second obstacle should NOT jump timer again
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    CHECK_THAT(ps.window_timer, WithinAbs(timer_after_first, 0.001f));
}

TEST_CASE("window_scaling: graded resets on new window", "[rhythm][window_scaling]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);

    // Simulate a graded window
    ps.graded = true;
    ps.window_scale = 0.5f;

    // Start a new window via player_action_system
    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    btn.pressed = true;
    btn.shape = Shape::Triangle;
    ps.phase_raw = static_cast<uint8_t>(WindowPhase::Idle);
    player_action_system(reg, 0.016f);

    CHECK_FALSE(ps.graded);
    CHECK(ps.window_scale == 1.0f);
}

// Integration: obstacle arrives on-beat

TEST_CASE("integration: obstacle arrives on-beat within 1 frame", "[rhythm][integration]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.playing = true; song.song_time = 0.0f;
    constexpr float dt = 1.0f / 60.0f;
    float elapsed = 0.0f;
    bool obstacle_at_player = false;
    while (elapsed < 5.0f) {
        song_playback_system(reg, dt);
        beat_scheduler_system(reg, dt);
        scroll_system(reg, dt);
        elapsed += dt;
        auto view = reg.view<ObstacleTag, Position>();
        for (auto [e, pos] : view.each()) {
            // Check arrival at PLAYER_Y (where the beat mathematically lands)
            if (pos.y >= constants::PLAYER_Y) {
                obstacle_at_player = true;
                float beat_time = song.offset + 4 * song.beat_period;
                CHECK_THAT(elapsed, WithinAbs(beat_time, dt + 0.001f));
                break;
            }
        }
        if (obstacle_at_player) break;
    }
    CHECK(obstacle_at_player);
}
