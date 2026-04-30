#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "util/beat_map_loader.h"
#include "components/beat_map.h"
#include "components/obstacle.h"

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

TEST_CASE("parse: low_bar entry is ignored while parse succeeds", "[low_high_bar][parse]") {
    std::string json = flat_json(R"({ "beat": 8, "kind": "low_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    CHECK(map.beats.empty());
}

TEST_CASE("parse: high_bar entry is ignored while parse succeeds", "[low_high_bar][parse]") {
    std::string json = flat_json(R"({ "beat": 12, "kind": "high_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    CHECK(map.beats.empty());
}

TEST_CASE("parse: mixed shape_gate + low/high bars keeps only non-bar obstacles", "[low_high_bar][parse]") {
    std::string json = flat_json(
        R"({ "beat": 4, "kind": "shape_gate", "shape": "triangle", "lane": 2 },)"
        R"({ "beat": 8, "kind": "low_bar" },)"
        R"({ "beat": 12, "kind": "high_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::ShapeGate);
    CHECK(map.beats[0].shape == Shape::Triangle);
    CHECK(map.beats[0].lane == 2);
}

TEST_CASE("parse: nested hard difficulty drops low/high bars", "[low_high_bar][parse]") {
    std::string json = hard_json(
        R"({ "beat": 4, "kind": "shape_gate", "shape": "square", "lane": 1 },)"
        R"({ "beat": 8, "kind": "low_bar" },)"
        R"({ "beat": 12, "kind": "high_bar" })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors, "hard");

    REQUIRE(ok);
    REQUIRE(map.beats.size() == 1);
    CHECK(map.beats[0].kind == ObstacleKind::ShapeGate);
}

TEST_CASE("parse: low/high bars with explicit lane are ignored", "[low_high_bar][parse]") {
    std::string json = flat_json(
        R"({ "beat": 8, "kind": "low_bar", "lane": 1 },)"
        R"({ "beat": 12, "kind": "high_bar", "lane": 2 })");

    BeatMap map;
    std::vector<BeatMapError> errors;
    bool ok = parse_beat_map(json, map, errors);

    REQUIRE(ok);
    CHECK(errors.empty());
    CHECK(map.beats.empty());
}
