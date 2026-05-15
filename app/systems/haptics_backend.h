#pragma once

#include "haptics.h"

#include <entt/entt.hpp>

namespace platform::haptics {

enum class ImpactStyle : unsigned char {
    Light = 0,
    Medium,
    Heavy,
};

struct TriggerPattern {
    ImpactStyle style = ImpactStyle::Light;
    int pulse_count = 1;
};

TriggerPattern pattern_for_event(HapticEvent event) noexcept;
void warmup(entt::registry& reg) noexcept;
void trigger(entt::registry& reg, HapticEvent event) noexcept;
void shutdown(entt::registry& reg) noexcept;

}  // namespace platform::haptics
