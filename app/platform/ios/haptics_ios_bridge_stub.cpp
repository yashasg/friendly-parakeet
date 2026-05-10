#include "haptics_ios_bridge.h"

#include <new>

namespace platform::ios {

struct HapticsIOSState {};

bool haptics_ios_available() noexcept {
    return false;
}

HapticsIOSState* haptics_ios_create_state() noexcept {
    return new (std::nothrow) HapticsIOSState{};
}

void haptics_ios_destroy_state(HapticsIOSState*& state) noexcept {
    if (state == nullptr) {
        return;
    }
    delete state;
    state = nullptr;
}

void haptics_ios_warmup(HapticsIOSState&) noexcept {}

void haptics_ios_trigger(HapticsIOSState&, HapticEvent) noexcept {}

void haptics_ios_reset(HapticsIOSState&) noexcept {}

}  // namespace platform::ios
