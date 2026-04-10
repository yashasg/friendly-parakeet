#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <entt/entt.hpp>

#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
#include "components/obstacle_data.h"
#include "components/input.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/burnout.h"
#include "components/difficulty.h"
#include "components/rendering.h"
#include "components/lifetime.h"
#include "components/particle.h"
#include "components/audio.h"
#include "constants.h"
#include "systems/all_systems.h"

// ── Helpers ─────────────────────────────────────────────────

static entt::registry make_bench_registry() {
    entt::registry reg;
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<GestureResult>();
    reg.ctx().emplace<ShapeButtonEvent>();
    reg.ctx().emplace<GameState>(GameState{
        GamePhase::Playing, GamePhase::Playing, 0.0f, false, GamePhase::Playing, 0.0f
    });
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<BurnoutState>();
    reg.ctx().emplace<DifficultyConfig>();
    reg.ctx().emplace<AudioQueue>();
    return reg;
}

static entt::entity make_bench_player(entt::registry& reg) {
    auto p = reg.create();
    reg.emplace<PlayerTag>(p);
    reg.emplace<Position>(p, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<PlayerShape>(p);
    reg.emplace<Lane>(p);
    reg.emplace<VerticalState>(p);
    reg.emplace<DrawColor>(p, uint8_t{80}, uint8_t{180}, uint8_t{255}, uint8_t{255});
    reg.emplace<DrawSize>(p, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(p, Layer::Game);
    return p;
}

static void spawn_obstacles(entt::registry& reg, int count) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    for (int i = 0; i < count; ++i) {
        auto obs = reg.create();
        reg.emplace<ObstacleTag>(obs);
        float y = constants::SPAWN_Y + static_cast<float>(i) * 80.0f;
        reg.emplace<Position>(obs, constants::LANE_X[i % 3], y);
        reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
        auto shape = static_cast<Shape>(i % 3);
        reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate, int16_t{200});
        reg.emplace<RequiredShape>(obs, shape);
        reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
        reg.emplace<DrawLayer>(obs, Layer::Game);
        reg.emplace<DrawColor>(obs, uint8_t{255}, uint8_t{255}, uint8_t{255}, uint8_t{255});
    }
}

static void spawn_particles(entt::registry& reg, int count) {
    for (int i = 0; i < count; ++i) {
        auto p = reg.create();
        reg.emplace<ParticleTag>(p);
        reg.emplace<Position>(p, 360.0f, 500.0f);
        reg.emplace<Velocity>(p, static_cast<float>(i % 50 - 25), -100.0f);
        reg.emplace<Lifetime>(p, 0.6f, 0.6f);
        reg.emplace<ParticleData>(p, 4.0f);
        reg.emplace<DrawColor>(p, uint8_t{255}, uint8_t{100}, uint8_t{50}, uint8_t{255});
        reg.emplace<DrawLayer>(p, Layer::Effects);
    }
}

// ── Benchmarks ──────────────────────────────────────────────

constexpr float DT = 1.0f / 60.0f;

TEST_CASE("Bench: scroll_system", "[bench]") {
    BENCHMARK_ADVANCED("10 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 10);
        meter.measure([&] { scroll_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("100 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 100);
        meter.measure([&] { scroll_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("1000 entities")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 1000);
        meter.measure([&] { scroll_system(reg, DT); });
    };
}

TEST_CASE("Bench: burnout_system", "[bench]") {
    BENCHMARK_ADVANCED("1 obstacle")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 1);
        meter.measure([&] { burnout_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("10 obstacles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 10);
        meter.measure([&] { burnout_system(reg, DT); });
    };
    BENCHMARK_ADVANCED("100 obstacles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 100);
        meter.measure([&] { burnout_system(reg, DT); });
    };
}

TEST_CASE("Bench: collision_system", "[bench]") {
    BENCHMARK_ADVANCED("1 obstacle at player")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        auto obs = reg.create();
        reg.emplace<ObstacleTag>(obs);
        reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
        reg.emplace<Velocity>(obs, 0.0f, 400.0f);
        reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate, int16_t{200});
        reg.emplace<RequiredShape>(obs, Shape::Circle);
        meter.measure([&] {
            if (reg.all_of<ScoredTag>(obs)) reg.remove<ScoredTag>(obs);
            reg.ctx().get<GameState>().transition_pending = false;
            collision_system(reg, DT);
        });
    };
    BENCHMARK_ADVANCED("10 obstacles scattered")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 10);
        meter.measure([&] {
            reg.ctx().get<GameState>().transition_pending = false;
            collision_system(reg, DT);
        });
    };
}

TEST_CASE("Bench: difficulty_system", "[bench]") {
    BENCHMARK_ADVANCED("single tick")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        meter.measure([&] { difficulty_system(reg, DT); });
    };
}

TEST_CASE("Bench: scoring_system", "[bench]") {
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
            reg.emplace<Position>(obs, constants::LANE_X[1], constants::PLAYER_Y);
            reg.emplace<Velocity>(obs, 0.0f, 400.0f);
            reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate, int16_t{200});
            reg.emplace<ScoredTag>(obs);
            reg.emplace<DrawLayer>(obs, Layer::Game);
            reg.emplace<DrawColor>(obs, uint8_t{255}, uint8_t{255}, uint8_t{255}, uint8_t{255});
        }
        meter.measure([&] { scoring_system(reg, DT); });
    };
}

TEST_CASE("Bench: gesture_system", "[bench]") {
    BENCHMARK_ADVANCED("swipe classification")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        auto& input = reg.ctx().get<InputState>();
        input.touch_up = true;
        input.start_x = 100.0f; input.start_y = 400.0f;
        input.end_x = 300.0f;   input.end_y = 400.0f;
        input.duration = 0.1f;
        meter.measure([&] { gesture_system(reg, DT); });
    };
}

TEST_CASE("Bench: player_action + movement", "[bench]") {
    BENCHMARK_ADVANCED("shape change + lane switch")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        auto& btn = reg.ctx().get<ShapeButtonEvent>();
        btn.pressed = true;
        btn.shape = Shape::Triangle;
        auto& gesture = reg.ctx().get<GestureResult>();
        gesture.gesture = SwipeGesture::SwipeRight;
        meter.measure([&] {
            player_action_system(reg, DT);
            player_movement_system(reg, DT);
        });
    };
}

TEST_CASE("Bench: lifetime_system", "[bench]") {
    BENCHMARK_ADVANCED("50 particles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_particles(reg, 50);
        meter.measure([&] { lifetime_system(reg, DT); });
    };
}

TEST_CASE("Bench: particle_system", "[bench]") {
    BENCHMARK_ADVANCED("50 particles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_particles(reg, 50);
        meter.measure([&] { particle_system(reg, DT); });
    };
}

TEST_CASE("Bench: cleanup_system", "[bench]") {
    BENCHMARK_ADVANCED("10 obstacles (none past threshold)")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        spawn_obstacles(reg, 10);
        meter.measure([&] { cleanup_system(reg, DT); });
    };
}

TEST_CASE("Bench: full frame (typical)", "[bench]") {
    BENCHMARK_ADVANCED("6 obstacles + 20 particles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 6);
        spawn_particles(reg, 20);
        meter.measure([&] {
            gesture_system(reg, DT);
            game_state_system(reg, DT);
            player_action_system(reg, DT);
            player_movement_system(reg, DT);
            difficulty_system(reg, DT);
            scroll_system(reg, DT);
            burnout_system(reg, DT);
            collision_system(reg, DT);
            scoring_system(reg, DT);
            lifetime_system(reg, DT);
            particle_system(reg, DT);
            cleanup_system(reg, DT);
        });
    };
}

TEST_CASE("Bench: full frame (stress)", "[bench]") {
    BENCHMARK_ADVANCED("50 obstacles + 50 particles")(Catch::Benchmark::Chronometer meter) {
        auto reg = make_bench_registry();
        make_bench_player(reg);
        spawn_obstacles(reg, 50);
        spawn_particles(reg, 50);
        meter.measure([&] {
            gesture_system(reg, DT);
            game_state_system(reg, DT);
            player_action_system(reg, DT);
            player_movement_system(reg, DT);
            difficulty_system(reg, DT);
            scroll_system(reg, DT);
            burnout_system(reg, DT);
            collision_system(reg, DT);
            scoring_system(reg, DT);
            lifetime_system(reg, DT);
            particle_system(reg, DT);
            cleanup_system(reg, DT);
        });
    };
}
