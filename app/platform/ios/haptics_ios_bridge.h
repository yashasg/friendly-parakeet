#pragma once

#include "../../components/haptics.h"

namespace platform::ios {

bool haptics_ios_available() noexcept;
void haptics_ios_trigger(HapticEvent event) noexcept;

}  // namespace platform::ios
