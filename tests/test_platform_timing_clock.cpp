#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "timing/clock.h"

#include <cmath>

namespace {

struct ClockOverrideGuard {
    ~ClockOverrideGuard() {
        platform::timing::clear_now_seconds_override();
    }
};

}  // namespace

TEST_CASE("timing_clock: override returns exact injected value", "[timing][clock]") {
    ClockOverrideGuard guard;
    platform::timing::set_now_seconds_override(42.25);

    CHECK_THAT(platform::timing::now_seconds(),
               Catch::Matchers::WithinAbs(42.25, 0.000001));
}

TEST_CASE("timing_clock: real clock returns finite non-decreasing values", "[timing][clock]") {
    ClockOverrideGuard guard;
    const double t0 = platform::timing::now_seconds();
    const double t1 = platform::timing::now_seconds();

    CHECK(std::isfinite(t0));
    CHECK(std::isfinite(t1));
    CHECK(t1 >= t0);
}
