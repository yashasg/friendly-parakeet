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

TEST_CASE("beat_scheduler: uses beat_times array for arrival time when present",
          "[beat_scheduler][beat_times]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

    song.offset = 0.0f;
    song.bpm = 120.0f;
    song_state_compute_derived(song);
    map.beat_times = {0.3f, 0.85f, 1.47f};
    map.beats.push_back({2, ObstacleKind::ShapeGate, Shape::Circle, 1, 0});

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

TEST_CASE("beat_scheduler: spawns multiple beats when time is past all", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

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

TEST_CASE("beat_scheduler: spawns all queued beats from BeatMap entries", "[beat_scheduler]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    auto& map = reg.ctx().get<BeatMap>();

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
        ++obstacle_count;
        if (obs.kind == ObstacleKind::ShapeGate) ++shape_gate_count;
    }
    CHECK(obstacle_count == 3);
    CHECK(shape_gate_count == 1);
    CHECK(song.next_spawn_idx == 3);
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
    auto view = reg.view<ObstacleTag, WorldTransform>();
    for (auto [e, wt] : view.each()) {
        CHECK(wt.position.y > constants::SPAWN_Y);
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

    auto view = reg.view<ObstacleTag, MotionVelocity>();
    for (auto [e, vel] : view.each()) {
        CHECK(vel.value.y == song.scroll_speed);
        CHECK(vel.value.x == 0.0f);
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

    auto view = reg.view<ObstacleTag, SDL_Color>();
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

    auto view = reg.view<ObstacleTag, SDL_Color>();
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

    auto view = reg.view<ObstacleTag, SDL_Color>();
    for (auto [e, dc] : view.each()) {
        CHECK(dc.r == 100);
        CHECK(dc.g == 255);
        CHECK(dc.b == 100);
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
    auto& map = reg.ctx().get<BeatMap>();

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
