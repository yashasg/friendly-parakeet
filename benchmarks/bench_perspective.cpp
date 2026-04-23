#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_optimizer.hpp>

#include "systems/camera_system.h"
#include "components/shape_vertices.h"
#include "constants.h"

// ── Perspective projection benchmarks ───────────────────────
// Measures the hot-path costs introduced by the isometric perspective
// system: vertex projection, shape drawing, and floor initialization.

TEST_CASE("Bench: camera::project single vertex", "[bench][perspective]") {
    BENCHMARK("project(180, 640)") {
        auto v = camera::project(180.0f, 640.0f);
        Catch::Benchmark::deoptimize_value(v);
        return v;
    };
}

TEST_CASE("Bench: camera::project_x single vertex", "[bench][perspective]") {
    BENCHMARK("project_x(180, 640)") {
        float x = camera::project_x(180.0f, 640.0f);
        Catch::Benchmark::deoptimize_value(x);
        return x;
    };
}

TEST_CASE("Bench: project all 3 lanes at N depths", "[bench][perspective]") {
    BENCHMARK_ADVANCED("3 lanes × 21 floor positions")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            for (int lane = 0; lane < 3; ++lane) {
                float cx = constants::LANE_X[lane];
                for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
                    float cy = constants::FLOOR_Y_START
                        + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
                    sum += camera::project_x(cx, cy);
                }
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: perspective shape vertex projection — Circle", "[bench][perspective]") {
    BENCHMARK_ADVANCED("24-segment circle (project only)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            float r = 32.0f;
            float cx = 360.0f, cy = 500.0f;
            Vector2 centre = camera::project(cx, cy);
            sum += centre.x;
            for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::CIRCLE_SEGMENTS;
                Vector2 v1 = camera::project(
                    cx + shape_verts::CIRCLE[i].x * r,
                    cy + shape_verts::CIRCLE[i].y * r);
                Vector2 v2 = camera::project(
                    cx + shape_verts::CIRCLE[next].x * r,
                    cy + shape_verts::CIRCLE[next].y * r);
                sum += v1.x + v2.x;
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: perspective shape vertex projection — Square", "[bench][perspective]") {
    BENCHMARK_ADVANCED("4-vertex trapezoid (project only)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            float half = 32.0f;
            float cx = 360.0f, cy = 500.0f;
            for (int i = 0; i < 4; ++i) {
                Vector2 v = camera::project(
                    cx + shape_verts::SQUARE[i].x * half,
                    cy + shape_verts::SQUARE[i].y * half);
                sum += v.x;
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: perspective shape vertex projection — Triangle", "[bench][perspective]") {
    BENCHMARK_ADVANCED("3-vertex triangle (project only)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            float half = 32.0f;
            float cx = 360.0f, cy = 500.0f;
            for (int i = 0; i < 3; ++i) {
                Vector2 v = camera::project(
                    cx + shape_verts::TRIANGLE[i].x * half,
                    cy + shape_verts::TRIANGLE[i].y * half);
                sum += v.x;
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: perspective shape vertex projection — Hexagon", "[bench][perspective]") {
    BENCHMARK_ADVANCED("6-segment fan (project only)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            float radius = 64.0f * 0.6f;
            float cx = 360.0f, cy = 500.0f;
            Vector2 centre = camera::project(cx, cy);
            sum += centre.x;
            for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
                int next = (i + 1) % shape_verts::HEX_SEGMENTS;
                Vector2 v1 = camera::project(
                    cx + shape_verts::HEXAGON[i].x * radius,
                    cy + shape_verts::HEXAGON[i].y * radius);
                Vector2 v2 = camera::project(
                    cx + shape_verts::HEXAGON[next].x * radius,
                    cy + shape_verts::HEXAGON[next].y * radius);
                sum += v1.x + v2.x;
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: perspective rect projection (obstacle trapezoid)", "[bench][perspective]") {
    BENCHMARK_ADVANCED("full-width bar (4 projects)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            Vector2 tl = camera::project(0.0f, 300.0f);
            Vector2 tr = camera::project(720.0f, 300.0f);
            Vector2 bl = camera::project(0.0f, 320.0f);
            Vector2 br = camera::project(720.0f, 320.0f);
            float sum = tl.x + tr.x + bl.x + br.x;
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: perspective line projection", "[bench][perspective]") {
    BENCHMARK_ADVANCED("floor connector (2 projects)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            Vector2 a = camera::project(180.0f, 100.0f);
            Vector2 b = camera::project(180.0f, 150.0f);
            float sum = a.x + b.x;
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: floor shape initialisation (flat draw at projected pos)", "[bench][perspective]") {
    BENCHMARK_ADVANCED("3 lanes × 21 shapes (project + depth scale)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            constexpr float vp_y  = constants::VANISHING_POINT_Y;
            constexpr float range = static_cast<float>(constants::SCREEN_H) - vp_y;
            float size = constants::FLOOR_SHAPE_SIZE;
            float half = size / 2.0f;

            for (int lane = 0; lane < 3; ++lane) {
                float cx = constants::LANE_X[lane];
                for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
                    float cy = constants::FLOOR_Y_START
                        + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;

                    Vector2 pc = camera::project(cx, cy);
                    float depth = (cy - vp_y) / range;
                    if (depth < 0.01f) depth = 0.01f;
                    if (depth > 1.5f)  depth = 1.5f;
                    float p_half = half * depth;

                    sum += pc.x + pc.y + p_half;
                }
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: typical obstacle batch projection", "[bench][perspective]") {
    BENCHMARK_ADVANCED("10 obstacles (project vertices only)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            for (int i = 0; i < 10; ++i) {
                float y = constants::SPAWN_Y + static_cast<float>(i) * 120.0f;
                float lx = constants::LANE_X[i % 3] - 60.0f;
                // Rect: 4 projects
                sum += camera::project_x(lx, y);
                sum += camera::project_x(lx + 120.0f, y);
                sum += camera::project_x(lx, y + 80.0f);
                sum += camera::project_x(lx + 120.0f, y + 80.0f);
                // Shape: project centre + vertices
                auto shape = static_cast<Shape>(i % 3);
                float cx = constants::LANE_X[i % 3];
                float cy = y + 40.0f;
                float half = 20.0f;
                int nverts = (shape == Shape::Circle) ? 24
                           : (shape == Shape::Hexagon) ? 6
                           : (shape == Shape::Square) ? 4 : 3;
                sum += camera::project_x(cx, cy);
                for (int v = 0; v < nverts; ++v) {
                    sum += camera::project_x(cx + half, cy + half);
                }
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}

TEST_CASE("Bench: vertex table lookup vs trig (comparison)", "[bench][perspective]") {
    BENCHMARK_ADVANCED("table lookup: 24 circle vertices")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            float r = 32.0f;
            for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
                float wx = 360.0f + shape_verts::CIRCLE[i].x * r;
                float wy = 640.0f + shape_verts::CIRCLE[i].y * r;
                sum += camera::project_x(wx, wy);
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };

    BENCHMARK_ADVANCED("trig: 24 circle vertices (cosf/sinf)")(Catch::Benchmark::Chronometer meter) {
        meter.measure([&] {
            float sum = 0.0f;
            float r = 32.0f;
            for (int i = 0; i < 24; ++i) {
                float angle = static_cast<float>(i) * (2.0f * 3.14159265f / 24.0f);
                float wx = 360.0f + cosf(angle) * r;
                float wy = 640.0f + sinf(angle) * r;
                sum += camera::project_x(wx, wy);
            }
            Catch::Benchmark::deoptimize_value(sum);
            return sum;
        });
    };
}
