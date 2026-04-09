#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("difficulty: speed starts at 1.0x", "[difficulty]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    CHECK(config.speed_multiplier == 1.0f);
}

TEST_CASE("difficulty: speed ramps over time", "[difficulty]") {
    auto reg = make_registry();

    // Simulate 60 seconds
    for (int i = 0; i < 3600; ++i) {
        difficulty_system(reg, 1.0f / 60.0f);
    }

    auto& config = reg.ctx().get<DifficultyConfig>();
    CHECK(config.speed_multiplier > 1.5f);
    CHECK(config.scroll_speed > constants::BASE_SCROLL_SPEED);
}

TEST_CASE("difficulty: speed caps at 3.0x", "[difficulty]") {
    auto reg = make_registry();

    // Simulate 300 seconds (well past the cap)
    for (int i = 0; i < 300; ++i) {
        difficulty_system(reg, 1.0f);
    }

    auto& config = reg.ctx().get<DifficultyConfig>();
    CHECK(config.speed_multiplier == 3.0f);
}

TEST_CASE("difficulty: spawn interval shrinks", "[difficulty]") {
    auto reg = make_registry();
    float initial = reg.ctx().get<DifficultyConfig>().spawn_interval;

    for (int i = 0; i < 3600; ++i) {
        difficulty_system(reg, 1.0f / 60.0f);
    }

    CHECK(reg.ctx().get<DifficultyConfig>().spawn_interval < initial);
}

TEST_CASE("difficulty: spawn interval has floor", "[difficulty]") {
    auto reg = make_registry();

    for (int i = 0; i < 300; ++i) {
        difficulty_system(reg, 1.0f);
    }

    CHECK(reg.ctx().get<DifficultyConfig>().spawn_interval >= constants::MIN_SPAWN_INT);
}

TEST_CASE("difficulty: burnout window shrinks", "[difficulty]") {
    auto reg = make_registry();

    for (int i = 0; i < 3600; ++i) {
        difficulty_system(reg, 1.0f / 60.0f);
    }

    CHECK(reg.ctx().get<DifficultyConfig>().burnout_window_scale < 1.0f);
}

TEST_CASE("difficulty: elapsed time accumulates", "[difficulty]") {
    auto reg = make_registry();

    difficulty_system(reg, 1.0f);
    difficulty_system(reg, 1.0f);
    difficulty_system(reg, 1.0f);

    CHECK(reg.ctx().get<DifficultyConfig>().elapsed == 3.0f);
}
