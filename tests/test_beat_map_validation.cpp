#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include "test_helpers.h"
#include "entities/beat_map.h"
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

TEST_CASE("validate: split_path invalid lane fails", "[validate][issue932]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({0, ObstacleKind::SplitPath, Shape::Circle, 5, 0});

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

// ── validate_beat_map: active obstacle kind rules ─────────────

TEST_CASE("validate: split_path is supported in active beatmaps", "[validate][kind][issue932]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beats.push_back({4, ObstacleKind::SplitPath, Shape::Circle, 2, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
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

TEST_CASE("timing_multiplier_from_delta_abs: returns correct values per band", "[rhythm_helpers]") {
    // Post-#1202/#1204: tier discriminator eradicated; multiplier ladder is
    // keyed by the absolute delta-seconds. Each canonical sample below sits
    // squarely inside the corresponding former-tier band.
    CHECK(timing_multiplier_from_delta_abs(0.0f)                              == 1.50f);
    CHECK(timing_multiplier_from_delta_abs(kTimingPerfectSeconds + 0.001f)    == 1.00f);
    CHECK(timing_multiplier_from_delta_abs(kTimingGoodSeconds    + 0.001f)    == 0.50f);
    CHECK(timing_multiplier_from_delta_abs(kTimingOkSeconds      + 0.001f)    == 0.25f);
}

TEST_CASE("window_scale_from_delta_abs: returns correct values per band", "[rhythm_helpers]") {
    // Post-#223 inversion: smaller scale = better timing = faster window collapse.
    CHECK(window_scale_from_delta_abs(0.0f)                              == 0.50f);
    CHECK(window_scale_from_delta_abs(kTimingPerfectSeconds + 0.001f)    == 0.75f);
    CHECK(window_scale_from_delta_abs(kTimingGoodSeconds    + 0.001f)    == 1.00f);
    CHECK(window_scale_from_delta_abs(kTimingOkSeconds      + 0.001f)    == 1.00f);
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

TEST_CASE("song_state_compute_derived: invalid timing inputs fall back to finite defaults",
          "[rhythm_helpers][issue1003]") {
    SongState s;
    s.bpm = 0.0f;
    s.lead_beats = 0;

    song_state_compute_derived(s);

    CHECK(s.bpm == 120.0f);
    CHECK(s.lead_beats == 4);
    CHECK(std::isfinite(s.beat_period));
    CHECK(std::isfinite(s.lead_time));
    CHECK(std::isfinite(s.scroll_speed));
    CHECK(s.beat_period > 0.0f);
    CHECK(s.lead_time > 0.0f);
    CHECK(s.scroll_speed > 0.0f);
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

TEST_CASE("parse: non-positive BPM is rejected before deriving beat timing",
          "[parse][beat_times][issue1003]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "invalid_timing_test",
        "bpm": 0,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beat_times": [0.1],
        "beats": [
            { "beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("BPM") != std::string::npos);
    CHECK(map.beats.empty());
}

TEST_CASE("parse: non-positive lead_beats is rejected before runtime timing",
          "[parse][issue1003]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "invalid_timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 0,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("lead_beats") != std::string::npos);
    CHECK(map.beats.empty());
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

TEST_CASE("parse: beat_times must be finite", "[parse][beat_times][issue995]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beat_times": [0.1, 1e39],
        "beats": [
            { "beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("finite") != std::string::npos);
}

TEST_CASE("parse: beat_times must be non-decreasing", "[parse][beat_times][issue995]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beat_times": [1.0, 0.5],
        "beats": [
            { "beat": 0, "kind": "shape_gate", "shape": "circle", "lane": 1 },
            { "beat": 1, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("non-decreasing") != std::string::npos);
}

TEST_CASE("parse: missing selected difficulty falls back to valid difficulty",
          "[parse][difficulty][issue859]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "difficulty_fallback_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "difficulties": {
            "medium": {
                "beats": [
                    { "beat": 2, "kind": "shape_gate", "shape": "circle", "lane": 1 }
                ]
            }
        }
    })";

    CHECK(parse_beat_map(json, map, errors, "hard"));
    CHECK(map.difficulty == "medium");
    REQUIRE(map.beats.size() == 1);
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("falling back to 'medium'") != std::string::npos);
}

TEST_CASE("parse: malformed selected difficulty fails without fallback",
          "[parse][difficulty][issue859]") {
    struct Case {
        const char* difficulty_json;
        const char* expected_message;
    };
    constexpr Case cases[] = {
        {R"("hard": 7)", "Difficulty 'hard' must be an object"},
        {R"("hard": {})", "Difficulty 'hard' must contain a 'beats' array"},
        {R"("hard": { "beats": {} })", "Difficulty 'hard' field 'beats' must be an array"},
    };

    for (const auto& tc : cases) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        const std::string json = std::string(R"({
            "song_id": "malformed_difficulty_test",
            "bpm": 120,
            "offset": 0.0,
            "lead_beats": 4,
            "duration_sec": 60.0,
            "difficulties": {
                )") + tc.difficulty_json + R"(,
                "medium": {
                    "beats": [
                        { "beat": 2, "kind": "shape_gate", "shape": "circle", "lane": 1 }
                    ]
                }
            }
        })";

        INFO("difficulty json: " << tc.difficulty_json);
        CHECK_FALSE(parse_beat_map(json, map, errors, "hard"));
        CHECK(map.beats.empty());
        REQUIRE_FALSE(errors.empty());
        CHECK(errors[0].message.find(tc.expected_message) != std::string::npos);
    }
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

TEST_CASE("parse: shape_gate requires explicit shape", "[parse][shape][issue224]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "missing_shape_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "kind": "shape_gate" }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("Missing required shape") != std::string::npos);
}

TEST_CASE("parse: split_path loads required shape and lane", "[parse][issue932]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "split_path_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "kind": "split_path", "shape": "triangle", "lane": 2 }
        ]
    })";

    CHECK(parse_beat_map(json, map, errors));
    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::SplitPath);
    CHECK(map.beats[0].shape == Shape::Triangle);
    CHECK(map.beats[0].lane == 2);
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

TEST_CASE("parse: blocked lane arrays are rejected from active beatmaps", "[parse][blocked_mask][issue873]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "kind": "shape_gate", "shape": "circle", "lane": 1, "blocked": [0] }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("'blocked' is not supported") != std::string::npos);
}

TEST_CASE("parse: invalid blocked lane values fail before narrowing", "[parse][blocked_mask][issue994]") {
    const char* invalid_blocked_lanes[] = {"-1", "3", "257", "4294967297"};

    for (const char* lane : invalid_blocked_lanes) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        const std::string json = std::string(R"({
            "song_id": "blocked_lane_test",
            "bpm": 120,
            "offset": 0.0,
            "lead_beats": 4,
            "duration_sec": 60.0,
            "beats": [
                { "beat": 2, "kind": "shape_gate", "shape": "circle", "lane": 1, "blocked": [)") + lane + R"(] }
            ]
        })";

        INFO("blocked lane: " << lane);
        CHECK_FALSE(parse_beat_map(json, map, errors));
        REQUIRE_FALSE(errors.empty());
        CHECK(errors[0].message.find("blocked[]") != std::string::npos);
        CHECK(errors[0].message.find("'blocked' is not supported") == std::string::npos);
    }
}

TEST_CASE("parse: valid blocked lane values reach unsupported-field rejection unchanged", "[parse][blocked_mask][issue994]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "blocked_lane_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "kind": "shape_gate", "shape": "circle", "lane": 1, "blocked": [0, 1, 2] }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("'blocked' is not supported") != std::string::npos);
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

TEST_CASE("parse: huge beat index cannot expand derived beat_times", "[parse][beat_times][issue746]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "song_id": "huge_beat_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2147483647, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("derived beat_times") != std::string::npos);
    CHECK(map.beat_times.empty());
}

TEST_CASE("validate: unsupported timing range fails before beat range conversion", "[validate][beat_times]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = std::numeric_limits<float>::max();
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("unsupported beat range") != std::string::npos);
}

TEST_CASE("validate: authored time_sec beyond duration fails", "[validate][beat_times][time_sec]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 3.0f;
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0.5f, true});
    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Square, 1, 3.5f, true});

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
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 1.2f, true});
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 1.1f, true});

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
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0.0f, true});
    map.beats.push_back({1, ObstacleKind::ShapeGate, Shape::Circle, 1, 0.0f, false});
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 1.0f, true});
    map.beats.push_back({3, ObstacleKind::ShapeGate, Shape::Circle, 1,
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
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 1.5f, true});
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Square, 1, 1.4f, true});

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

TEST_CASE("validate: beat_times must be finite", "[validate][beat_times][issue995]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beat_times = {0.0f, std::numeric_limits<float>::infinity()};
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("finite") != std::string::npos);
}

TEST_CASE("validate: beat_times must be non-decreasing", "[validate][beat_times][issue995]") {
    BeatMap map;
    map.bpm = 120.0f;
    map.offset = 0.0f;
    map.lead_beats = 4;
    map.duration = 60.0f;
    map.beat_times = {1.0f, 0.5f};
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({1, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK_FALSE(validate_beat_map(map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].message.find("non-decreasing") != std::string::npos);
}
