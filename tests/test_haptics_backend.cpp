#include <catch2/catch_test_macros.hpp>

#include "components/haptics.h"
#include "systems/haptics_runtime.h"

TEST_CASE("haptics backend maps burst patterns for heavy events", "[haptic]") {
    const auto death = haptics_runtime::pattern_for_event(HapticEvent::DeathCrash);
    CHECK(death.style == haptics_runtime::ImpactStyle::Heavy);
    CHECK(death.pulse_count == 2);

    const auto burnout = haptics_runtime::pattern_for_event(HapticEvent::Burnout5_0x);
    CHECK(burnout.style == haptics_runtime::ImpactStyle::Heavy);
    CHECK(burnout.pulse_count == 3);
}

TEST_CASE("haptics backend maps light and medium events deterministically", "[haptic]") {
    const auto lane = haptics_runtime::pattern_for_event(HapticEvent::LaneSwitch);
    CHECK(lane.style == haptics_runtime::ImpactStyle::Light);
    CHECK(lane.pulse_count == 1);

    const auto score = haptics_runtime::pattern_for_event(HapticEvent::NewHighScore);
    CHECK(score.style == haptics_runtime::ImpactStyle::Medium);
    CHECK(score.pulse_count == 1);
}

TEST_CASE("haptics backend warmup is safe to call repeatedly", "[haptic]") {
    haptics_runtime::warmup();
    haptics_runtime::warmup();
    SUCCEED("haptic warmup completed");
}
