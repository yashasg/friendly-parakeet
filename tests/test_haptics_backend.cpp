#include <catch2/catch_test_macros.hpp>

#include "components/haptics.h"
#include "platform/haptics_backend.h"

TEST_CASE("haptics backend maps burst patterns for heavy events", "[haptic]") {
    const auto death = platform::haptics::pattern_for_event(HapticEvent::DeathCrash);
    CHECK(death.style == platform::haptics::ImpactStyle::Heavy);
    CHECK(death.pulse_count == 2);

    const auto burnout = platform::haptics::pattern_for_event(HapticEvent::Burnout5_0x);
    CHECK(burnout.style == platform::haptics::ImpactStyle::Heavy);
    CHECK(burnout.pulse_count == 3);
}

TEST_CASE("haptics backend maps light and medium events deterministically", "[haptic]") {
    const auto lane = platform::haptics::pattern_for_event(HapticEvent::LaneSwitch);
    CHECK(lane.style == platform::haptics::ImpactStyle::Light);
    CHECK(lane.pulse_count == 1);

    const auto score = platform::haptics::pattern_for_event(HapticEvent::NewHighScore);
    CHECK(score.style == platform::haptics::ImpactStyle::Medium);
    CHECK(score.pulse_count == 1);
}

TEST_CASE("haptics backend warmup is safe to call repeatedly", "[haptic]") {
    platform::haptics::warmup();
    platform::haptics::warmup();
    SUCCEED("haptic warmup completed");
}
