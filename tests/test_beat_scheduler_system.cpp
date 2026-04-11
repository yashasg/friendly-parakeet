#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── beat_scheduler_system: basic spawning ────────────────────

TEST_CASE("beat_scheduler: no spawn when not Playing", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;

    auto& map = reg.ctx().get<BeatMap>();
    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

    auto& song = reg.ctx().get<SongState>();
    song.song_time = 10.0f;

    beat_scheduler_system(reg, 0.016f);

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

    auto& map = reg.ctx().get<BeatMap>();
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
    auto& map = reg.ctx().get<BeatMap>();

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
        CHECK(obs.kind == ObstacleKind::ShapeGate);
        CHECK(rs.shape == Shape::Circle);
    }
}

TEST_CASE("beat_scheduler: does not spawn before spawn_time", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

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
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    CHECK(song.next_spawn_idx == 1);
}

TEST_CASE("beat_scheduler: spawns multiple beats when time is past all", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    map.beats.push_back({2, ObstacleKind::LowBar, Shape::Circle, 1, 0});
    map.beats.push_back({4, ObstacleKind::HighBar, Shape::Circle, 1, 0});

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
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::LaneBlock, Shape::Circle, 1, 0b010});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, BlockedLanes>();
    for (auto [e, obs, bl] : view.each()) {
        CHECK(obs.kind == ObstacleKind::LaneBlock);
        CHECK(bl.mask == 0b010);
    }
}

TEST_CASE("beat_scheduler: spawns LowBar with RequiredVAction Jumping", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::LowBar, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, RequiredVAction>();
    for (auto [e, obs, rva] : view.each()) {
        CHECK(obs.kind == ObstacleKind::LowBar);
        CHECK(rva.action == VMode::Jumping);
    }
}

TEST_CASE("beat_scheduler: spawns HighBar with RequiredVAction Sliding", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::HighBar, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, RequiredVAction>();
    for (auto [e, obs, rva] : view.each()) {
        CHECK(obs.kind == ObstacleKind::HighBar);
        CHECK(rva.action == VMode::Sliding);
    }
}

TEST_CASE("beat_scheduler: spawns ComboGate with shape and blocked lanes", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ComboGate, Shape::Triangle, 1, 0b101});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape, BlockedLanes>();
    for (auto [e, obs, rs, bl] : view.each()) {
        CHECK(obs.kind == ObstacleKind::ComboGate);
        CHECK(rs.shape == Shape::Triangle);
        CHECK(bl.mask == 0b101);
    }
}

TEST_CASE("beat_scheduler: spawns SplitPath with shape and required lane", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::SplitPath, Shape::Square, 2, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape, RequiredLane>();
    for (auto [e, obs, rs, rl] : view.each()) {
        CHECK(obs.kind == ObstacleKind::SplitPath);
        CHECK(rs.shape == Shape::Square);
        CHECK(rl.lane == 2);
    }
}

TEST_CASE("beat_scheduler: all spawned obstacles have BeatInfo", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

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
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    // Obstacle should spawn below SPAWN_Y to compensate for late spawn.
    // start_y = SPAWN_Y + overshoot * scroll_speed
    auto view = reg.view<ObstacleTag, Position>();
    for (auto [e, pos] : view.each()) {
        CHECK(pos.y > constants::SPAWN_Y);
    }
}

TEST_CASE("beat_scheduler: obstacles have velocity matching scroll_speed", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Velocity>();
    for (auto [e, vel] : view.each()) {
        CHECK(vel.dy == song.scroll_speed);
        CHECK(vel.dx == 0.0f);
    }
}

TEST_CASE("beat_scheduler: ShapeGate Circle has blue color", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, DrawColor>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 80);
        CHECK(dc.g == 200);
        CHECK(dc.b == 255);
    }
}

TEST_CASE("beat_scheduler: ShapeGate Square has red color", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Square, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, DrawColor>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 255);
        CHECK(dc.g == 100);
        CHECK(dc.b == 100);
    }
}

TEST_CASE("beat_scheduler: ShapeGate Triangle has green color", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    map.beats.push_back({0, ObstacleKind::ShapeGate, Shape::Triangle, 1, 0});
    song.song_time = 10.0f;
    song.next_spawn_idx = 0;

    beat_scheduler_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, DrawColor>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 100);
        CHECK(dc.g == 255);
        CHECK(dc.b == 100);
    }
}
