#include <catch2/catch_test_macros.hpp>

#include "components/haptics.h"
#include "platform/haptics_backend.h"

#include <entt/entt.hpp>

TEST_CASE("haptics backend maps burst patterns for heavy events", "[haptic]") {
    const auto death = platform::haptics::pattern_for_event(HapticEvent::DeathCrash);
    CHECK(death.style == platform::haptics::ImpactStyle::Heavy);
    CHECK(death.pulse_count == 2);
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
    entt::registry reg;
    platform::haptics::warmup(reg);
    platform::haptics::warmup(reg);
    platform::haptics::shutdown(reg);
    SUCCEED("haptic warmup completed");
}

TEST_CASE("haptics backend lifecycle is idempotent across registry resets", "[haptic][lifecycle]") {
    entt::registry reg;

    platform::haptics::warmup(reg);
    platform::haptics::trigger(reg, HapticEvent::RetryTap);
    platform::haptics::shutdown(reg);
    platform::haptics::shutdown(reg);

    reg = entt::registry{};

    platform::haptics::warmup(reg);
    platform::haptics::trigger(reg, HapticEvent::DeathCrash);
    platform::haptics::shutdown(reg);

    SUCCEED("haptics lifecycle completed across two registry cycles");
}
