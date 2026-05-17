#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <entt/entt.hpp>

#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
#include "systems/input.h"
#include "systems/input_events.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/rendering.h"
#include "components/particle.h"
#include "components/rhythm.h"
#include "systems/audio_routing.h"
#include "constants.h"
#include "systems/all_systems.h"
#include "systems/game_phase_transition.h"
#include "systems/input_routing.h"
#include "util/shape_tag.h"

// ── Helpers ─────────────────────────────────────────────────

static entt::registry make_bench_registry() {
    entt::registry reg;
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<entt::dispatcher>();
    wire_input_dispatcher(reg);
    wire_audio_haptic_dispatcher(reg);
    reg.ctx().emplace<GameState>(GameState{ 0.0f });
    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<SongState>();
    runtime_system_scratch_init(reg);
    return reg;
}

static entt::entity make_bench_player(entt::registry& reg) {
    auto p = reg.create();
    reg.emplace<PlayerTag>(p);
    reg.emplace<WorldPosition>(p, WorldPosition{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<PlayerShape>(p);
    reg.emplace<ShapeWindow>(p);
    reg.emplace<Lane>(p);
    // Grounded = absence of Jumping/Sliding (#1202/#1204).
    reg.emplace<Color>(p, Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(p, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    return p;
}

static void spawn_obstacles(entt::registry& reg, int count) {
    const auto& song = reg.ctx().get<SongState>();
    for (int i = 0; i < count; ++i) {
        auto obs = reg.create();
        reg.emplace<ObstacleTag>(obs);
        float y = constants::SPAWN_Y + static_cast<float>(i) * 80.0f;
        reg.emplace<WorldPosition>(obs, WorldPosition{{constants::LANE_X[i % 3], y}});
        reg.emplace<Vector2>(obs, Vector2{0.0f, song.scroll_speed});
        auto shape = static_cast<Shape>(i % 3);
        reg.emplace<Obstacle>(obs, int16_t{200});
        set_required_shape_tag(reg, obs, shape);
        reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
        reg.emplace<Color>(obs, Color{255, 255, 255, 255});
    }
}

// Spawns obstacles with the production scroll_system archetype:
// WorldPosition + Vector2 (freeplay non-beat path, exclude BeatInfo).
static void spawn_scroll_obstacles(entt::registry& reg, int count) {
    const auto& song = reg.ctx().get<SongState>();
    for (int i = 0; i < count; ++i) {
        auto obs = reg.create();
        reg.emplace<ObstacleTag>(obs);
        float y = constants::SPAWN_Y + static_cast<float>(i) * 80.0f;
        reg.emplace<WorldPosition>(obs, WorldPosition{{0.0f, y}});
        reg.emplace<Vector2>(obs, Vector2{0.0f, song.scroll_speed});
        reg.emplace<Obstacle>(obs, int16_t{200});
    }
}

// Spawns particles using the production archetype: WorldPosition+Vector2.
// These are processed by motion_system's motion_view and rendered
// by camera_system via ParticleTag+WorldPosition.
static void spawn_particles(entt::registry& reg, int count) {
    for (int i = 0; i < count; ++i) {
        auto p = reg.create();
        reg.emplace<ParticleTag>(p);
        reg.emplace<WorldPosition>(p, WorldPosition{{360.0f, 500.0f}});
        reg.emplace<Vector2>(p, Vector2{static_cast<float>(i % 50 - 25), -100.0f});
        reg.emplace<ParticleData>(p, 4.0f, 0.6f, 0.6f);
        reg.emplace<Color>(p, Color{255, 100, 50, 255});
    }
}

// ── Benchmarks ──────────────────────────────────────────────

constexpr float DT = 1.0f / 60.0f;

TEST_CASE("Bench: scroll_system", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("10 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_scroll_obstacles(reg, 10);
        meter.measure([&] { scroll_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("100 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_scroll_obstacles(reg, 100);
        meter.measure([&] { scroll_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("1000 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_scroll_obstacles(reg, 1000);
        meter.measure([&] { scroll_system(reg, DT); });
    };
}

TEST_CASE("Bench: collision_system", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("1 obstacle at player")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        auto obs = reg.create();
        reg.emplace<ObstacleTag>(obs);
        reg.emplace<WorldPosition>(obs, WorldPosition{{constants::LANE_X[1], constants::PLAYER_Y}});
        reg.emplace<Vector2>(obs, Vector2{0.0f, 400.0f});
        reg.emplace<Obstacle>(obs, int16_t{200});
        set_required_shape_tag(reg, obs, Shape::Circle);
        meter.measure([&] {
            if (reg.all_of<ScoredTag>(obs)) reg.remove<ScoredTag>(obs);
            clear_next_phase_tags(reg);
            collision_system(reg, DT);
        });
    };
    BENCHMARK_ADVANCED("10 obstacles scattered")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 10);
        meter.measure([&] {
            clear_next_phase_tags(reg);
            collision_system(reg, DT);
        });
    };
}

TEST_CASE("Bench: scoring_system", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("no scored obstacles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 10);
        meter.measure([&] { scoring_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("5 scored obstacles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        for (int i = 0; i < 5; ++i) {
            auto obs = reg.create();
            reg.emplace<ObstacleTag>(obs);
            reg.emplace<WorldPosition>(obs, WorldPosition{{constants::LANE_X[1], constants::PLAYER_Y}});
            reg.emplace<Vector2>(obs, Vector2{0.0f, 400.0f});
            reg.emplace<Obstacle>(obs, int16_t{200});
            reg.emplace<ScoredTag>(obs);
            reg.emplace<Color>(obs, Color{255, 255, 255, 255});
        }
        meter.measure([&] { scoring_system(reg, DT); });
    };
}

TEST_CASE("Bench: player_input + movement", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("shape change + lane switch")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);

        meter.measure([&] {
            player_input_handle_press_triangle(reg, ShapePressTriangleEvent{});
            player_input_handle_go_right(reg, GoRightEvent{});
            player_movement_system(reg, DT);
        });
    };
}

TEST_CASE("Bench: particle_system", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("50 particles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_particles(reg, 50);
        meter.measure([&] { particle_system(reg, DT); });
    };
}

TEST_CASE("Bench: obstacle_despawn_system", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("10 obstacles (none past threshold)")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 10);
        meter.measure([&] { obstacle_despawn_system(reg, DT); });
    };
}

TEST_CASE("Bench: motion_system", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("10 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 10);
        meter.measure([&] { motion_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("100 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 100);
        meter.measure([&] { motion_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("1000 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 1000);
        meter.measure([&] { motion_system(reg, DT); });
    };
}

TEST_CASE("Bench: full frame (typical)", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("6 obstacles + 20 particles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 6);
        spawn_particles(reg, 20);
        meter.measure([&] {
            game_state_system(reg, DT);
            player_movement_system(reg, DT);
            scroll_system(reg, DT);
            motion_system(reg, DT);
            collision_system(reg, DT);
            scoring_system(reg, DT);
            particle_system(reg, DT);
            obstacle_despawn_system(reg, DT);
        });
    };
}

TEST_CASE("Bench: full frame (stress)", "[!benchmark][bench]") {
    BENCHMARK_ADVANCED("50 obstacles + 50 particles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 50);
        spawn_particles(reg, 50);
        meter.measure([&] {
            game_state_system(reg, DT);
            player_movement_system(reg, DT);
            scroll_system(reg, DT);
            motion_system(reg, DT);
            collision_system(reg, DT);
            scoring_system(reg, DT);
            particle_system(reg, DT);
            obstacle_despawn_system(reg, DT);
        });
    };
}
