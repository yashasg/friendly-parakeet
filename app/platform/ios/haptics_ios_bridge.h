#pragma once

#include "../../components/haptics.h"

namespace platform::ios {

struct HapticsIOSState;

bool haptics_ios_available() noexcept;
HapticsIOSState* haptics_ios_create_state() noexcept;
void haptics_ios_destroy_state(HapticsIOSState*& state) noexcept;
void haptics_ios_warmup(HapticsIOSState& state) noexcept;
void haptics_ios_trigger(HapticsIOSState& state, HapticEvent event) noexcept;
void haptics_ios_reset(HapticsIOSState& state) noexcept;

}  // namespace platform::ios
