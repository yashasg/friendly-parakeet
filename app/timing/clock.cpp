#include "clock.h"

#if defined(SHAPESHIFTER_BACKEND_SDL2)
#include "platform/sdl2/sdl2_headers.h"
#else
#include "runtime/runtime_api.h"
#endif

namespace platform::timing {

namespace {

struct ClockOverride {
    bool enabled = false;
    double seconds = 0.0;
};

ClockOverride& override_state() noexcept {
    static ClockOverride state;
    return state;
}

}  // namespace

[[nodiscard]] double now_seconds() noexcept {
    const auto& state = override_state();
    if (state.enabled) return state.seconds;

#if defined(SHAPESHIFTER_BACKEND_SDL2)
    const auto frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    if (frequency <= 0.0) return 0.0;
    return static_cast<double>(SDL_GetPerformanceCounter()) / frequency;
#else
    return GetTime();
#endif
}

void set_now_seconds_override(double seconds) noexcept {
    auto& state = override_state();
    state.enabled = true;
    state.seconds = seconds;
}

void clear_now_seconds_override() noexcept {
    auto& state = override_state();
    state.enabled = false;
    state.seconds = 0.0;
}

}  // namespace platform::timing
