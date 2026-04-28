#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <raylib.h>

#include "util/file_logger.h"

#include <cstdio>
#include <cstring>

static const char* BENCH_LOG = "bench_logger.log";

static void cleanup_bench_log() {
    std::remove(BENCH_LOG);
}

TEST_CASE("Bench: file_logger single TraceLog call", "[bench]") {
    file_logger_init(BENCH_LOG);

    BENCHMARK("single TraceLog") {
        TraceLog(LOG_INFO, "Player scored %d points at position (%.1f, %.1f)", 42, 360.0f, 500.0f);
    };

    file_logger_shutdown();
    cleanup_bench_log();
}

TEST_CASE("Bench: file_logger burst (10 calls)", "[bench]") {
    file_logger_init(BENCH_LOG);

    BENCHMARK("10 TraceLog calls") {
        for (int i = 0; i < 10; ++i) {
            TraceLog(LOG_INFO, "Frame %d entity %d pos=(%.1f,%.1f)", 1000 + i, i, 100.0f * i, 200.0f);
        }
    };

    file_logger_shutdown();
    cleanup_bench_log();
}

TEST_CASE("Bench: file_logger burst (100 calls)", "[bench]") {
    file_logger_init(BENCH_LOG);

    BENCHMARK("100 TraceLog calls") {
        for (int i = 0; i < 100; ++i) {
            TraceLog(LOG_WARNING, "Collision check entity=%d shape=%d lane=%d", i, i % 3, i % 3);
        }
    };

    file_logger_shutdown();
    cleanup_bench_log();
}

TEST_CASE("Bench: file_logger vs no-op baseline", "[bench]") {
    // Baseline: raylib default logging (stdout only)
    SetTraceLogCallback(nullptr);
    SetTraceLogLevel(LOG_NONE);

    BENCHMARK("baseline (logging disabled)") {
        TraceLog(LOG_INFO, "This should be suppressed %d", 42);
    };

    // File logger active
    SetTraceLogLevel(LOG_INFO);
    file_logger_init(BENCH_LOG);

    BENCHMARK("file logger active") {
        TraceLog(LOG_INFO, "This goes to file %d", 42);
    };

    file_logger_shutdown();
    cleanup_bench_log();
}
