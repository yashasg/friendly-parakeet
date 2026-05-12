#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include "test_helpers.h"
#include "util/beat_map_loader.h"
#include "util/rhythm_math.h"

namespace {

struct TempBeatMapFile {
    explicit TempBeatMapFile(const char* name, const std::string& json)
        : path(std::filesystem::temp_directory_path() / name) {
        std::filesystem::remove(path);
        std::ofstream out(path);
        REQUIRE(out.good());
        out << json;
    }

    ~TempBeatMapFile() {
        std::filesystem::remove(path);
    }

    std::filesystem::path path;
};

}  // namespace

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

TEST_CASE("validate: anchored small negative offset passes", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = -0.1f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

TEST_CASE("validate: offset below anchored negative floor fails", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = -0.11f;
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

TEST_CASE("validate: negative beat index fails", "[validate][issue132]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({-1, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("non-negative") != std::string::npos);
}

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

TEST_CASE("validate: duplicate beat indices pass when timing order is non-decreasing", "[validate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
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

// ── validate_beat_map: ComboGate blocked_mask rules ──────────

TEST_CASE("validate: combo_gate blocked_mask 0 fails", "[validate][combo_gate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ComboGate, Shape::Circle, 1, 0x00});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("block at least one lane") != std::string::npos);
}

TEST_CASE("validate: combo_gate blocked_mask 0x07 fails", "[validate][combo_gate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ComboGate, Shape::Circle, 1, 0x07});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("leave at least one lane open") != std::string::npos);
}

TEST_CASE("validate: combo_gate blocked_mask 0x01 passes", "[validate][combo_gate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ComboGate, Shape::Circle, 1, 0x01});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

TEST_CASE("validate: combo_gate blocked_mask 0x06 passes", "[validate][combo_gate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ComboGate, Shape::Circle, 1, 0x06});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

TEST_CASE("validate: combo_gate blocked_mask 0x05 passes", "[validate][combo_gate]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ComboGate, Shape::Circle, 1, 0x05});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

TEST_CASE("validate: lane_block blocked_mask must use exactly one lane bit", "[validate][lane_block]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::LaneBlock, Shape::Circle, 1, 0x03});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("exactly one lane bit") != std::string::npos);
}

TEST_CASE("validate: blocked_mask lane bits outside 0..2 fail", "[validate][blocked_mask]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::ComboGate, Shape::Circle, 1, 0x09});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("bits 0..2") != std::string::npos);
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

TEST_CASE("timing_multiplier: returns correct values", "[rhythm_helpers]") {
    CHECK(timing_multiplier(TimingTier::Perfect) == 1.50f);
    CHECK(timing_multiplier(TimingTier::Good) == 1.00f);
    CHECK(timing_multiplier(TimingTier::Ok) == 0.50f);
    CHECK(timing_multiplier(TimingTier::Bad) == 0.25f);
}

TEST_CASE("window_scale_for_tier: returns correct values", "[rhythm_helpers]") {
    // Post-#223 inversion: smaller scale = better timing = faster window collapse.
    CHECK(window_scale_for_tier(TimingTier::Perfect) == 0.50f);
    CHECK(window_scale_for_tier(TimingTier::Good) == 0.75f);
    CHECK(window_scale_for_tier(TimingTier::Ok) == 1.00f);
    CHECK(window_scale_for_tier(TimingTier::Bad) == 1.00f);
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

    // Window is fixed by timing policy to +/-150ms, capped by BPM ceiling.
    CHECK_THAT(s.window_duration, Catch::Matchers::WithinAbs(0.3f, 0.001f));
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

    CHECK_THAT(s.scroll_speed, Catch::Matchers::WithinAbs(constants::APPROACH_DIST / 2.0f, 1.0f));
}

// ── load_validation_constants: path resolution ───────────────

TEST_CASE("load_validation_constants: default paths load shipped defaults", "[validate][constants]") {
    ValidationConstants vc = load_validation_constants("");
    CHECK(vc.bpm_min == 60.0f);
    CHECK(vc.bpm_max == 300.0f);
    CHECK(vc.offset_min == -0.1f);
    CHECK(vc.offset_max == 5.0f);
    CHECK(vc.lead_beats_min == 2);
    CHECK(vc.lead_beats_max == 8);
    CHECK(vc.min_shape_change_gap == 3);
}

TEST_CASE("load_validation_constants: explicit app_dir takes precedence over CWD fallback", "[validate][constants]") {
    const std::filesystem::path root = "test_validation_constants_app_dir";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "content");
    {
        std::ofstream out(root / "content" / "constants.json");
        REQUIRE(out.good());
        out << R"({"validation":{"bpm_min":72,"bpm_max":240,"offset_max":2.5,"min_shape_change_gap":4}})";
    }

    ValidationConstants vc = load_validation_constants(root.string());
    CHECK(vc.bpm_min == 72.0f);
    CHECK(vc.bpm_max == 240.0f);
    CHECK(vc.offset_min == -0.1f);
    CHECK(vc.offset_max == 2.5f);
    CHECK(vc.lead_beats_min == 2);
    CHECK(vc.lead_beats_max == 8);
    CHECK(vc.min_shape_change_gap == 4);

    std::filesystem::remove_all(root);
}

TEST_CASE("load_validation_constants: bad app_dir falls back to CWD path", "[validate][constants]") {
    // A nonsense app_dir that doesn't contain a constants.json; must not throw or crash.
    ValidationConstants vc = load_validation_constants("/nonexistent_dir_xyz/");
    CHECK(vc.bpm_min == 60.0f);
    CHECK(vc.bpm_max == 300.0f);
}

TEST_CASE("load_and_validate_beat_map rejects invalid BPM at load time", "[load][validate][issue128]") {
    TempBeatMapFile file("shapeshifter_invalid_bpm_beatmap.json", R"({
        "song_id": "invalid_bpm",
        "bpm": 30,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    CHECK_FALSE(load_and_validate_beat_map(file.path.string(), map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("BPM") != std::string::npos);
    CHECK(map.beats.empty());
}

TEST_CASE("load_and_validate_beat_map rejects negative beat_index at load time",
          "[load][validate][issue128]") {
    TempBeatMapFile file("shapeshifter_negative_beat_beatmap.json", R"({
        "song_id": "negative_beat",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": -1, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    CHECK_FALSE(load_and_validate_beat_map(file.path.string(), map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("Beat index") != std::string::npos);
    CHECK(map.beats.empty());
}

TEST_CASE("load_and_validate_beat_map accepts valid beatmaps at load time",
          "[load][validate][issue128]") {
    TempBeatMapFile file("shapeshifter_valid_beatmap.json", R"({
        "song_id": "valid_load",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    REQUIRE(load_and_validate_beat_map(file.path.string(), map, errors));
    CHECK(errors.empty());
    REQUIRE(map.beats.size() == 1);
    CHECK(map.song_id == "valid_load");
}

TEST_CASE("validate_beat_map explicit constants: custom BPM range is respected", "[validate][constants]") {
    ValidationConstants vc;
    vc.bpm_min = 80.0f;
    vc.bpm_max = 200.0f;

    BeatMap map;
    map.bpm = 70.0f;  // below custom min, above compiled-in default min
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors, vc));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("BPM") != std::string::npos);
}

TEST_CASE("validate_beat_map explicit constants: BPM within custom range passes", "[validate][constants]") {
    ValidationConstants vc;
    vc.bpm_min = 80.0f;
    vc.bpm_max = 200.0f;

    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors, vc));
    CHECK(errors.empty());
}

TEST_CASE("validate_beat_map explicit constants: custom shape gap is respected", "[validate][constants]") {
    ValidationConstants vc;
    vc.min_shape_change_gap = 5;  // stricter than default 3

    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    // 3 beats apart — passes default rule but must fail the stricter custom rule
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({3, ObstacleKind::ShapeGate, Shape::Square, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors, vc));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("beats apart") != std::string::npos);
}

TEST_CASE("parse: beat_times and per-obstacle time_sec are loaded", "[parse][beat_times]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beat_times": [0.1, 0.7, 1.4],
        "beats": [
            { "beat": 2, "time_sec": 1.4, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 1);
    REQUIRE(map.beat_times.size() == 3);
    CHECK_THAT(map.beats[0].time_sec, Catch::Matchers::WithinAbs(1.4f, 0.001f));
    CHECK(map.beats[0].has_time_sec);
}

TEST_CASE("parse: beat without time_sec falls back to beat_times", "[parse][beat_times]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beat_times": [0.1, 0.7, 1.4],
        "beats": [
            { "beat": 2, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 1);
    CHECK_FALSE(map.beats[0].has_time_sec);
    CHECK_THAT(map.beats[0].time_sec, Catch::Matchers::WithinAbs(1.4f, 0.001f));
}

TEST_CASE("parse: non-numeric time_sec fails", "[parse][beat_times][time_sec]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beat_times": [0.1, 0.7, 1.4],
        "beats": [
            { "beat": 2, "time_sec": "1.4", "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("'time_sec'") != std::string::npos);
}

TEST_CASE("parse: shape-bearing obstacles require explicit shape", "[parse][shape][issue224]") {
    const char* shape_bearing_kinds[] = {"shape_gate", "combo_gate", "split_path"};

    for (const char* kind : shape_bearing_kinds) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        const std::string blocked = std::string(kind) == "combo_gate" ? R"(, "blocked": [1])" : "";
        const std::string json = std::string(R"({
            "song_id": "missing_shape_test",
            "bpm": 120,
            "offset": 0.0,
            "lead_beats": 4,
            "duration_sec": 60.0,
            "beats": [
                { "beat": 2, "kind": ")") + kind + R"(")" + blocked + R"( }
            ]
        })";

        CHECK_FALSE(parse_beat_map(json, map, errors));
        REQUIRE_FALSE(errors.empty());
        CHECK(errors[0].message.find("Missing required shape") != std::string::npos);
    }
}

TEST_CASE("parse: lane_block does not require shape", "[parse][shape][issue224]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "lane_block_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "kind": "lane_block", "blocked": [1] }
        ]
    })";

    CHECK(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::LaneBlock);
}

TEST_CASE("parse: same beat_index entries are ordered by resolved time_sec", "[parse][beat_times][ordering]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beat_times": [0.1, 0.7, 1.4],
        "beats": [
            { "beat": 2, "time_sec": 1.6, "kind": "shape_gate", "shape": "square", "lane": 1 },
            { "beat": 2, "kind": "shape_gate", "shape": "triangle", "lane": 1 },
            { "beat": 2, "time_sec": 1.3, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 3);
    CHECK_THAT(map.beats[0].time_sec, Catch::Matchers::WithinAbs(1.3f, 0.001f));
    CHECK_THAT(map.beats[1].time_sec, Catch::Matchers::WithinAbs(1.4f, 0.001f));
    CHECK_THAT(map.beats[2].time_sec, Catch::Matchers::WithinAbs(1.6f, 0.001f));
}

TEST_CASE("parse: same beat_index entries keep authored order for identical timing", "[parse][beat_times][ordering]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "time_sec": 1.4, "kind": "shape_gate", "shape": "square", "lane": 1 },
            { "beat": 2, "time_sec": 1.4, "kind": "shape_gate", "shape": "triangle", "lane": 1 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 2);
    CHECK(map.beats[0].shape == Shape::Square);
    CHECK(map.beats[1].shape == Shape::Triangle);
}

TEST_CASE("parse: invalid blocked lane index fails", "[parse][blocked_mask]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "kind": "lane_block", "blocked": [0, 3] }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("range [0, 2]") != std::string::npos);
}

TEST_CASE("parse: invalid scalar lane values fail before narrowing", "[parse][lane]") {
    const char* invalid_lanes[] = {"-1", "3", "257", "2147483648"};

    for (const char* lane : invalid_lanes) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        const std::string json = std::string(R"({
            "song_id": "lane_test",
            "bpm": 120,
            "offset": 0.0,
            "lead_beats": 4,
            "duration_sec": 60.0,
            "beats": [
                { "beat": 2, "kind": "shape_gate", "shape": "circle", "lane": )") + lane + R"( }
            ]
        })";

        CHECK_FALSE(parse_beat_map(json, map, errors));
        REQUIRE_FALSE(errors.empty());
        CHECK(errors[0].message.find("'lane'") != std::string::npos);
    }
}

TEST_CASE("validate: authored time_sec beyond duration fails", "[validate][beat_times][time_sec]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 3.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 0.5f, true});
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Square, 1, 0, 3.5f, true});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("<= duration_sec") != std::string::npos);
}

TEST_CASE("validate: authored time_sec must be non-decreasing", "[validate][beat_times][time_sec]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 1.2f, true});
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 1.1f, true});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("non-decreasing") != std::string::npos);
}

TEST_CASE("validate: mixed authored time_sec edge cases are checked", "[validate][beat_times][time_sec]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 10.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 0.0f, true});
    map.beats.push_back({1, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 0.0f, false});
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 1.0f, true});
    map.beats.push_back({3, ObstacleKind::ShapeGate, Shape::Circle, 1, 0,
                         std::numeric_limits<float>::quiet_NaN(), true});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("finite") != std::string::npos);
}

TEST_CASE("validate: same beat_index requires non-decreasing resolved time", "[validate][beat_times][ordering]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 10.0f;
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 1.5f, true});
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Square, 1, 0, 1.4f, true});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("beat index") != std::string::npos);
}

TEST_CASE("validate: beat index out of range for beat_times fails", "[validate][beat_times]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beat_times = {0.0f, 0.5f}; // valid indices: 0..1
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("out of range for beat_times") != std::string::npos);
}
