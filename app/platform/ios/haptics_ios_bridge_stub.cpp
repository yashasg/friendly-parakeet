#include "haptics_ios_bridge.h"

namespace platform::ios {

bool haptics_ios_available() noexcept {
    return false;
}

void haptics_ios_warmup() noexcept {}

void haptics_ios_trigger(HapticEvent) noexcept {}

}  // namespace platform::ios
