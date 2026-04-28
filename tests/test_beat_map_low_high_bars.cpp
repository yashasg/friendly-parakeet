// Regression tests for issue #125: low_bar and high_bar in beatmap pipeline.
//
// Verifies that the C++ loader / validator fully support both kinds:
//   - JSON parsing succeeds without shape or lane fields
//   - Correct ObstacleKind enum values are produced
//   - validate_beat_map passes for maps containing these kinds
//   - Nested-difficulty format (used by level_designer.py output) works
//
// Does NOT test level_designer.py output directly — that is covered by
// tools/validate_beatmap_bars.py (Python acceptance script).

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "util/beat_map_loader.h"
#include "components/beat_map.h"
#include "components/obstacle.h"

// ── Helpers ─────────────────────────────────────────────────────────

// Minimal valid beatmap header as a raw-string prefix.
static const char* HEADER = R"({
    "song_id": "test_bars",
    "title": "Bar Test",
    "bpm": 120.0,
    "offset": 0.0,
    "lead_beats": 4,
    "duration_sec": 60.0
)";

static std::string flat_json(const std::string& beats_array_body) {
    return std::string(HEADER) + ", \"beats\": [" + beats_array_body + "]\n}";
}

static std::string hard_json(const std::string& beats_array_body) {
    return std::string(HEADER)
        + R"(, "difficulties": { "hard": { "beats": [)"
        + beats_array_body
        + "] } }\n}";
}

// ── Parse: low_bar ───────────────────────────────────────────────────

TEST_CASE("parse: low_bar entry succeeds without shape or lane", "[low_high_bar][parse]") {
    std::string json = flat_json(R"({ "beat": 8, "kind": "low_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::LowBar);
    CHECK(map.beats[0].beat_index == 8);
}

TEST_CASE("parse: low_bar kind string maps to ObstacleKind::LowBar", "[low_high_bar][parse]") {
    std::string json = flat_json(R"({ "beat": 4, "kind": "low_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    parse_beat_map(json, map, errors);

    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::LowBar);
}

// ── Parse: high_bar ──────────────────────────────────────────────────

TEST_CASE("parse: high_bar entry succeeds without shape or lane", "[low_high_bar][parse]") {
    std::string json = flat_json(R"({ "beat": 12, "kind": "high_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::HighBar);
    CHECK(map.beats[0].beat_index == 12);
}

TEST_CASE("parse: high_bar kind string maps to ObstacleKind::HighBar", "[low_high_bar][parse]") {
    std::string json = flat_json(R"({ "beat": 16, "kind": "high_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    parse_beat_map(json, map, errors);

    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::HighBar);
}

// ── Validate: low_bar / high_bar pass validation ─────────────────────

TEST_CASE("validate: map with only low_bar entries passes", "[low_high_bar][validate]") {
    BeatMap map;
    map.bpm        = 120.0f;
    map.offset     = 0.0f;
    map.lead_beats = 4;
    map.duration   = 60.0f;
    map.beats.push_back({8,  ObstacleKind::LowBar, Shape::Circle, 1, 0});
    map.beats.push_back({16, ObstacleKind::LowBar, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

TEST_CASE("validate: map with only high_bar entries passes", "[low_high_bar][validate]") {
    BeatMap map;
    map.bpm        = 120.0f;
    map.offset     = 0.0f;
    map.lead_beats = 4;
    map.duration   = 60.0f;
    map.beats.push_back({8,  ObstacleKind::HighBar, Shape::Circle, 1, 0});
    map.beats.push_back({16, ObstacleKind::HighBar, Shape::Circle, 1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

TEST_CASE("validate: mixed shape_gate + low_bar + high_bar passes", "[low_high_bar][validate]") {
    BeatMap map;
    map.bpm        = 120.0f;
    map.offset     = 0.0f;
    map.lead_beats = 4;
    map.duration   = 60.0f;
    map.beats.push_back({4,  ObstacleKind::ShapeGate, Shape::Circle,   1, 0});
    map.beats.push_back({8,  ObstacleKind::LowBar,    Shape::Circle,   1, 0});
    map.beats.push_back({12, ObstacleKind::HighBar,   Shape::Circle,   1, 0});
    map.beats.push_back({16, ObstacleKind::ShapeGate, Shape::Square,   1, 0});

    std::vector<BeatMapError> errors;
    CHECK(validate_beat_map(map, errors));
    CHECK(errors.empty());
}

// ── Parse: nested difficulties format (level_designer.py output shape) ──

TEST_CASE("parse: hard difficulty nesting with low_bar and high_bar", "[low_high_bar][parse]") {
    std::string json = hard_json(
        R"({ "beat": 4, "kind": "shape_gate", "shape": "square", "lane": 1 },)"
        R"({ "beat": 8, "kind": "low_bar" },)"
        R"({ "beat": 12, "kind": "high_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors, "hard");

    REQUIRE(ok);
    // A difficulty-fallback warning is not an error; actual parse errors would fail.
    REQUIRE(map.beats.size() == 3);
    CHECK(map.beats[0].kind == ObstacleKind::ShapeGate);
    CHECK(map.beats[1].kind == ObstacleKind::LowBar);
    CHECK(map.beats[2].kind == ObstacleKind::HighBar);
}

// ── Regression: bars do NOT require a lane field ─────────────────────

TEST_CASE("parse: low_bar with explicit lane field is still accepted", "[low_high_bar][parse]") {
    // level_designer.py does not emit a lane field for bars, but if one were
    // present it must not cause a parse failure.
    std::string json = flat_json(R"({ "beat": 8, "kind": "low_bar", "lane": 1 })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    CHECK(map.beats[0].kind == ObstacleKind::LowBar);
}

TEST_CASE("parse: high_bar with explicit lane field is still accepted", "[low_high_bar][parse]") {
    std::string json = flat_json(R"({ "beat": 8, "kind": "high_bar", "lane": 1 })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    CHECK(map.beats[0].kind == ObstacleKind::HighBar);
}
