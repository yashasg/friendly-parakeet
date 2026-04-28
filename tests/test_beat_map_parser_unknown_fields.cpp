#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "util/beat_map_loader.h"

// Regression tests for #133: unknown kind/shape strings must be reported
// with beat index and value rather than silently defaulting.

// ── Unknown obstacle kind ─────────────────────────────────────

TEST_CASE("parse: unknown kind reports error with beat index and value", "[parse][kind][regression]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
        "beats": [
            { "beat": 5, "kind": "typo_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].beat_index == 5);
    CHECK(errors[0].message.find("typo_gate") != std::string::npos);
    CHECK(errors[0].message.find("5") != std::string::npos);
}

TEST_CASE("parse: unknown kind does not silently become ShapeGate", "[parse][kind][regression]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
        "beats": [
            { "beat": 3, "kind": "bad_kind", "shape": "circle", "lane": 1 }
        ]
    })";

    bool ok = parse_beat_map(json, map, errors);
    CHECK_FALSE(ok);
    // Beat must not have been silently added with a default kind
    CHECK(map.beats.empty());
}

TEST_CASE("parse: unknown kind at non-zero beat includes correct beat index", "[parse][kind][regression]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
        "beats": [
            { "beat": 42, "kind": "unknown_obstacle", "shape": "circle", "lane": 0 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].beat_index == 42);
    CHECK(errors[0].message.find("unknown_obstacle") != std::string::npos);
}

// ── Unknown shape string ──────────────────────────────────────

TEST_CASE("parse: unknown shape reports error with beat index and value", "[parse][shape][regression]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
        "beats": [
            { "beat": 7, "kind": "shape_gate", "shape": "hexagon", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].beat_index == 7);
    CHECK(errors[0].message.find("hexagon") != std::string::npos);
    CHECK(errors[0].message.find("7") != std::string::npos);
}

TEST_CASE("parse: unknown shape does not silently become Circle", "[parse][shape][regression]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "kind": "shape_gate", "shape": "star", "lane": 1 }
        ]
    })";

    bool ok = parse_beat_map(json, map, errors);
    CHECK_FALSE(ok);
    CHECK(map.beats.empty());
}

// ── Valid beatmaps remain error-free ──────────────────────────

TEST_CASE("parse: all valid kinds parse without errors", "[parse][kind]") {
    const std::vector<std::string> valid_kinds = {
        "shape_gate", "lane_block", "low_bar", "high_bar",
        "combo_gate", "split_path", "lane_push_left", "lane_push_right"
    };

    for (const auto& kind : valid_kinds) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        std::string json = R"({
            "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
            "beats": [
                { "beat": 4, "kind": ")" + kind + R"(", "lane": 1 }
            ]
        })";
        INFO("Testing kind: " << kind);
        CHECK(parse_beat_map(json, map, errors));
        CHECK(map.beats.size() == 1);
    }
}

TEST_CASE("parse: all valid shapes parse without errors", "[parse][shape]") {
    const std::vector<std::string> valid_shapes = {"circle", "square", "triangle"};

    for (const auto& shape : valid_shapes) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        std::string json = R"({
            "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
            "beats": [
                { "beat": 4, "kind": "shape_gate", "shape": ")" + shape + R"(", "lane": 1 }
            ]
        })";
        INFO("Testing shape: " << shape);
        CHECK(parse_beat_map(json, map, errors));
        CHECK(map.beats.size() == 1);
    }
}

// ── Mixed: valid entries before/after bad entry ───────────────

TEST_CASE("parse: bad kind in middle fails whole parse", "[parse][kind][regression]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
        "beats": [
            { "beat": 4,  "kind": "shape_gate",  "shape": "circle",   "lane": 1 },
            { "beat": 8,  "kind": "bad_obstacle", "shape": "circle",  "lane": 1 },
            { "beat": 12, "kind": "shape_gate",  "shape": "triangle", "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].beat_index == 8);
    CHECK(errors[0].message.find("bad_obstacle") != std::string::npos);
}

TEST_CASE("parse: bad shape in middle fails whole parse", "[parse][shape][regression]") {
    BeatMap map;
    std::vector<BeatMapError> errors;
    std::string json = R"({
        "bpm": 120, "offset": 0.0, "lead_beats": 4, "duration_sec": 60.0,
        "beats": [
            { "beat": 4,  "kind": "shape_gate", "shape": "circle",   "lane": 1 },
            { "beat": 8,  "kind": "shape_gate", "shape": "pentagon", "lane": 1 },
            { "beat": 12, "kind": "shape_gate", "shape": "square",   "lane": 1 }
        ]
    })";

    CHECK_FALSE(parse_beat_map(json, map, errors));
    REQUIRE_FALSE(errors.empty());
    CHECK(errors[0].beat_index == 8);
    CHECK(errors[0].message.find("pentagon") != std::string::npos);
}
