#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_optimizer.hpp>

#include "constants.h"
#include <cmath>

// ── Camera3D rendering benchmarks ───────────────────────────
// With Camera3D, the CPU-side projection math (project/depth) is gone.
// Vertex positions are emitted directly in world space; the GPU handles
// perspective projection via the view-projection matrix.
//
// The remaining CPU cost is generating world-space vertex positions for
// rlVertex3f calls.

TEST_CASE("Bench: circle ring vertex generation (world-space)", "[!benchmark][bench][rendering]") {
    BENCHMARK("24-segment circle trig vertices") {
        constexpr int ring_segments = 24;
        constexpr float tau = 6.28318530717958647692f;
        float sum = 0.0f;
        float cx = 360.0f, cz = 500.0f, r = 32.0f;
        for (int i = 0; i < ring_segments; ++i) {
            float angle = (static_cast<float>(i) / static_cast<float>(ring_segments)) * tau;
            sum += cx + std::cos(angle) * r;
            sum += cz + std::sin(angle) * r;
        }
        Catch::Benchmark::deoptimize_value(sum);
        return sum;
    };
}

TEST_CASE("Bench: floor position computation (3 lanes × 21 positions)", "[!benchmark][bench][rendering]") {
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

TEST_CASE("Bench: polygon vertex generation", "[!benchmark][bench][rendering]") {
    BENCHMARK("6-segment polygon trig vertices") {
        constexpr int sides = 6;
        constexpr float tau = 6.28318530717958647692f;
        float sum = 0.0f;
        float cx = 360.0f, cz = 500.0f, r = 38.4f;
        for (int i = 0; i < sides; ++i) {
            float angle = (static_cast<float>(i) / static_cast<float>(sides)) * tau;
            sum += cx + std::cos(angle) * r;
            sum += cz + std::sin(angle) * r;
        }
        Catch::Benchmark::deoptimize_value(sum);
        return sum;
    };
}
