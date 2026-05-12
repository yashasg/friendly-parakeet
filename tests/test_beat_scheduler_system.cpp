#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "entities/beat_map.h"

// ── beat_scheduler_system: basic spawning ────────────────────

TEST_CASE("beat_scheduler: no spawn when not Playing", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;

    auto& map = beat_map(reg);
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    auto& song = reg.ctx().get<SongState>();
    song.song_time = 10.0f;

    tick_playing_systems(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("beat_scheduler: no spawn when SongState absent", "[beat_scheduler]") {
    auto reg = make_registry();
    // No SongState or BeatMap

    beat_scheduler_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("beat_scheduler: no spawn when song not playing", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<SongState>().playing = false;

    auto& map = beat_map(reg);
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    beat_scheduler_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("beat_scheduler: no spawn when beat map is empty", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 10.0f;

    beat_scheduler_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("beat_scheduler: spawns ShapeGate when time passes spawn_time", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    // Add a beat at index 0
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    // The spawn_time = offset + beat_index * beat_period - lead_time
    // For beat 0: spawn_time = 0 + 0 * 0.5 - 2.0 = -2.0
    // So any song_time >= -2.0 should trigger spawn
    song.song_time = 0.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 1);

    // Verify it's a ShapeGate with correct components
    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape>();
    for (auto [e, obs, rs] : view.each()) {
        (void)obs;
        CHECK_FALSE(reg.all_of<BlockedLanes>(e));
        CHECK_FALSE(reg.all_of<RequiredLane>(e));
        CHECK(rs.shape == Shape::Circle);
    }
}

TEST_CASE("beat_scheduler: defaults invalid ShapeGate lane to center lane", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 9, 0});
    song.song_time = 0.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, WorldTransform>();
    REQUIRE(view.size_hint() == 1);
    for (auto [e, wt] : view.each()) {
        (void)e;
        CHECK_THAT(wt.position.x, Catch::Matchers::WithinAbs(constants::LANE_X[1], 0.01f));
    }
}

TEST_CASE("beat_scheduler: does not spawn before spawn_time", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    // Beat at index 20 — spawn_time = 0 + 20 * 0.5 - 2.0 = 8.0
    map.beats.push_back({20, ObstacleKind::ShapeGate, Shape::Square, 1, 0});

    song.song_time = 5.0f;  // Before spawn_time of 8.0
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("beat_scheduler: increments next_spawn_idx after spawn", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    CHECK(song.next_spawn_idx == 1);
}

TEST_CASE("beat_scheduler: uses beat_times array for arrival time when present",
          "[beat_scheduler][beat_times]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    song.offset = 0.0f;
    song.bpm = 120.0f;
    song_state_compute_derived(song);
    map.beat_times = {0.3f, 0.85f, 1.47f};
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    song.song_time = 0.0f;
    song.next_spawn_idx = 0;
    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    REQUIRE(view.size_hint() == 1);
    for (auto [entity, bi] : view.each()) {
        (void)entity;
        CHECK_THAT(bi.arrival_time, Catch::Matchers::WithinAbs(1.47f, 0.001f));
    }
}

TEST_CASE("beat_scheduler: uses BeatEntry time_sec for arrival time when authored",
          "[beat_scheduler][beat_times][issue404]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    song.offset = 0.0f;
    song.bpm = 120.0f;
    song_state_compute_derived(song);
    map.beat_times = {0.3f, 0.85f, 1.47f};

    BeatEntry authored_entry;
    authored_entry.beat_index = 2;
    authored_entry.kind = ObstacleKind::ShapeGate;
    authored_entry.shape = Shape::Circle;
    authored_entry.lane = 1;
    authored_entry.time_sec = 1.33f;
    authored_entry.has_time_sec = true;
    map.beats.push_back(authored_entry);

    song.song_time = 0.0f;
    song.next_spawn_idx = 0;
    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    REQUIRE(view.size_hint() == 1);
    for (auto [entity, bi] : view.each()) {
        (void)entity;
        CHECK_THAT(bi.arrival_time, Catch::Matchers::WithinAbs(1.33f, 0.001f));
        CHECK_THAT(bi.spawn_time, Catch::Matchers::WithinAbs(1.33f - song.lead_time, 0.001f));
    }
}

TEST_CASE("beat_scheduler: falls back to beat_times when BeatEntry time_sec is absent",
          "[beat_scheduler][beat_times][issue404]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    song.offset = 0.0f;
    song.bpm = 120.0f;
    song_state_compute_derived(song);
    map.beat_times = {0.3f, 0.85f, 1.47f};
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0, 99.0f, false});

    song.song_time = 30.0f;
    song.next_spawn_idx = 0;
    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    REQUIRE(view.size_hint() == 1);
    for (auto [entity, bi] : view.each()) {
        (void)entity;
        CHECK_THAT(bi.arrival_time, Catch::Matchers::WithinAbs(1.47f, 0.001f));
    }
}

TEST_CASE("beat_scheduler: same beat_index honors authored time_sec ordering",
          "[beat_scheduler][beat_times][issue442]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    std::vector<BeatMapError> errors;
    const std::string json = R"({
        "song_id": "timing_test",
        "bpm": 120,
        "offset": 0.0,
        "lead_beats": 4,
        "duration_sec": 60.0,
        "beats": [
            { "beat": 2, "time_sec": 1.7, "kind": "shape_gate", "shape": "square", "lane": 1 },
            { "beat": 2, "time_sec": 1.3, "kind": "shape_gate", "shape": "circle", "lane": 1 }
        ]
    })";

    REQUIRE(parse_beat_map(json, map, errors));
    REQUIRE(errors.empty());

    // Spawn window reached for 1.3s arrival (spawn at -0.7), but not yet for
    // 1.7s arrival (spawn at -0.3).
    song.song_time = -0.5f;
    song.next_spawn_idx = 0;
    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo, RequiredShape>();
    REQUIRE(view.size_hint() == 1);
    for (auto [entity, bi, rs] : view.each()) {
        (void)entity;
        CHECK(rs.shape == Shape::Circle);
        CHECK_THAT(bi.arrival_time, Catch::Matchers::WithinAbs(1.3f, 0.001f));
    }
}

TEST_CASE("beat_scheduler: spawns multiple beats when time is past all", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({2, ObstacleKind::LaneBlock, Shape::Circle, 1, 0b001});
    map.beats.push_back({4, ObstacleKind::ComboGate, Shape::Square, 1, 0b100});

    song.song_time = 30.0f;  // Well past all spawn times
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 3);
    CHECK(song.next_spawn_idx == 3);
}

// ── beat_scheduler: obstacle types ───────────────────────────

TEST_CASE("beat_scheduler: spawns LaneBlock with blocked_mask", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::LaneBlock, Shape::Circle, 1, 0b010});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, BlockedLanes>();
    for (auto [e, obs, bl] : view.each()) {
        (void)obs;
        CHECK_FALSE(reg.all_of<RequiredShape>(e));
        CHECK_FALSE(reg.all_of<RequiredLane>(e));
        CHECK(bl.mask == 0b010);
    }
}

TEST_CASE("beat_scheduler: spawns all queued beats from BeatMap entries", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::LaneBlock, Shape::Circle, 1, 0b010});
    map.beats.push_back({0, ObstacleKind::ComboGate, Shape::Triangle, 1, 0b001});
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Square, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto obstacle_view = reg.view<ObstacleTag, Obstacle>();
    int obstacle_count = 0;
    int shape_gate_count = 0;
    for (auto [e, obs] : obstacle_view.each()) {
        (void)obs;
        ++obstacle_count;
        const ObstacleKind kind = obstacle_kind_from_components(
            reg.all_of<RequiredShape>(e),
            reg.all_of<BlockedLanes>(e),
            reg.all_of<RequiredLane>(e));
        if (kind == ObstacleKind::ShapeGate) ++shape_gate_count;
    }
    CHECK(obstacle_count == 3);
    CHECK(shape_gate_count == 1);
    CHECK(song.next_spawn_idx == 3);
}

TEST_CASE("beat_scheduler: spawns ComboGate with shape and blocked lanes", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ComboGate, Shape::Triangle, 1, 0b101});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape, BlockedLanes>();
    for (auto [e, obs, rs, bl] : view.each()) {
        (void)obs;
        CHECK_FALSE(reg.all_of<RequiredLane>(e));
        CHECK(rs.shape == Shape::Triangle);
        CHECK(bl.mask == 0b101);
    }
}

TEST_CASE("beat_scheduler: spawns SplitPath with shape and required lane", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::SplitPath, Shape::Square, 2, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape, RequiredLane>();
    for (auto [e, obs, rs, rl] : view.each()) {
        (void)obs;
        CHECK_FALSE(reg.all_of<BlockedLanes>(e));
        CHECK(rs.shape == Shape::Square);
        CHECK(rl.lane == 2);
    }
}

TEST_CASE("beat_scheduler: all spawned obstacles have BeatInfo", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({4, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 30.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    int count = 0;
    for (auto [e, bi] : view.each()) {
        CHECK(bi.beat_index == 4);
        CHECK(bi.arrival_time > 0.0f);
        ++count;
    }
    CHECK(count == 1);
}

TEST_CASE("beat_scheduler: obstacles spawn with overshoot compensation", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    // Obstacle should spawn below SPAWN_Y to compensate for late spawn.
    // start_y = SPAWN_Y + overshoot * scroll_speed
    auto view = reg.view<ObstacleTag, WorldTransform>();
    for (auto [e, wt] : view.each()) {
        CHECK(wt.position.y > constants::SPAWN_Y);
    }
}

TEST_CASE("beat_scheduler: rhythm obstacles omit MotionVelocity", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, BeatInfo>();
    REQUIRE(std::distance(view.begin(), view.end()) == 1);
    for (auto [e, info] : view.each()) {
        (void)info;
        CHECK_FALSE(reg.all_of<MotionVelocity>(e));
    }
    auto invalid_view = reg.view<ObstacleTag, BeatInfo, MotionVelocity>();
    CHECK(std::distance(invalid_view.begin(), invalid_view.end()) == 0);
}

TEST_CASE("beat_scheduler: ShapeGate Circle has blue color", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Color>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 80);
        CHECK(dc.g == 200);
        CHECK(dc.b == 255);
    }
}

TEST_CASE("beat_scheduler: ShapeGate Square has red color", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Square, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Color>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 255);
        CHECK(dc.g == 100);
        CHECK(dc.b == 100);
    }
}

TEST_CASE("beat_scheduler: ShapeGate Triangle has green color", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Triangle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Color>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 100);
        CHECK(dc.g == 255);
        CHECK(dc.b == 100);
    }
}

TEST_CASE("beat_scheduler: ShapeGate Hexagon has hexagon color", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Hexagon, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Color>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 80);
        CHECK(dc.g == 180);
        CHECK(dc.b == 255);
    }
}

TEST_CASE("beat_scheduler: clamped late-spawn stores adjusted spawn_time in BeatInfo", "[beat_scheduler]") {
    // When song_time overshoots spawn_time by so much that start_y would exceed
    // max_start_y (PLAYER_Y), the position is clamped.
    // The effective spawn_time stored in BeatInfo must be adjusted so that
    // scroll_system (pos.y = SPAWN_Y + (song_time - spawn_time) * scroll_speed)
    // reproduces the clamped start_y on the same frame, preventing it from
    // snapping past the clamp on the very first tick.
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = beat_map(reg);

    // Use beat 0 whose spawn_time is deeply in the past
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    // Set song_time so the overshoot pushes start_y well past PLAYER_Y
    song.song_time = 100.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    float max_start_y = constants::PLAYER_Y;

    // Position must be clamped
    auto pview = reg.view<ObstacleTag, WorldTransform>();
    for (auto [e, wt] : pview.each()) {
        CHECK_THAT(wt.position.y, Catch::Matchers::WithinAbs(max_start_y, 0.01f));
    }

    // BeatInfo.spawn_time must reflect the adjusted value so scroll_system
    // reproduces max_start_y at song_time
    auto bview = reg.view<ObstacleTag, BeatInfo>();
    for (auto [e, bi] : bview.each()) {
        float recomputed_y = constants::SPAWN_Y
            + (song.song_time - bi.spawn_time) * song.scroll_speed;
        CHECK_THAT(recomputed_y, Catch::Matchers::WithinAbs(max_start_y, 0.01f));
    }
}
