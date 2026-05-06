#pragma once

#include "../components/haptics.h"

namespace haptics_runtime {

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
void warmup() noexcept;
void trigger(HapticEvent event) noexcept;

}  // namespace haptics_runtime
