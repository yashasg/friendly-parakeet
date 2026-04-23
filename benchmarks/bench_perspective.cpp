#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_optimizer.hpp>

#include "components/shape_vertices.h"
#include "constants.h"

// ── Camera3D rendering benchmarks ───────────────────────────
// With Camera3D, the CPU-side projection math (project/depth) is gone.
// Vertex positions are emitted directly in world space; the GPU handles
// perspective projection via the view-projection matrix.
//
// The remaining CPU cost is iterating the vertex tables (shape_verts)
// to compute world-space vertex positions for rlVertex3f calls.

TEST_CASE("Bench: circle vertex table iteration (world-space)", "[bench][rendering]") {
    BENCHMARK("24-segment circle vertices") {
        float sum = 0.0f;
        float cx = 360.0f, cz = 500.0f, r = 32.0f;
        for (int i = 0; i < shape_verts::CIRCLE_SEGMENTS; ++i) {
            sum += cx + shape_verts::CIRCLE[i].x * r;
            sum += cz + shape_verts::CIRCLE[i].y * r;
        }
        Catch::Benchmark::deoptimize_value(sum);
        return sum;
    };
}

TEST_CASE("Bench: floor position computation (3 lanes × 21 positions)", "[bench][rendering]") {
    BENCHMARK("3 lanes × 21 floor positions (world-space)") {
        float sum = 0.0f;
        for (int lane = 0; lane < 3; ++lane) {
            float cx = constants::LANE_X[lane];
            for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
                float cz = constants::FLOOR_Y_START
                    + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
                sum += cx;
                sum += cz;
            }
        }
        Catch::Benchmark::deoptimize_value(sum);
        return sum;
    };
}

TEST_CASE("Bench: hexagon vertex table iteration", "[bench][rendering]") {
    BENCHMARK("6-segment hexagon vertices") {
        float sum = 0.0f;
        float cx = 360.0f, cz = 500.0f, r = 38.4f;
        for (int i = 0; i < shape_verts::HEX_SEGMENTS; ++i) {
            sum += cx + shape_verts::HEXAGON[i].x * r;
            sum += cz + shape_verts::HEXAGON[i].y * r;
        }
        Catch::Benchmark::deoptimize_value(sum);
        return sum;
    };
}
