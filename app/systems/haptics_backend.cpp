#include "haptics_backend.h"

#include "haptics_ios_bridge.h"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace platform::haptics {
namespace {

#if defined(PLATFORM_IOS)
struct HapticsRuntimeState {
    platform::ios::HapticsIOSState* ios = nullptr;
};
#endif

void trace_haptic_event(const char* prefix, HapticEvent event) {
    const auto event_name = to_string(event);
    if (event_name.empty()) {
        TraceLog(LOG_DEBUG, "%sUnknown", prefix);
        return;
    }
    TraceLog(LOG_DEBUG, "%s%.*s", prefix, static_cast<int>(event_name.size()), event_name.data());
}

#if defined(PLATFORM_IOS)
HapticsRuntimeState& runtime_state(entt::registry& reg) {
    auto* state = reg.ctx().find<HapticsRuntimeState>();
    if (!state) {
        state = &reg.ctx().emplace<HapticsRuntimeState>();
    }
    return *state;
}

platform::ios::HapticsIOSState* ensure_ios_state(HapticsRuntimeState& state) {
    if (state.ios == nullptr) {
        state.ios = platform::ios::haptics_ios_create_state();
    }
    return state.ios;
}
#endif

}  // namespace

TriggerPattern pattern_for_event(HapticEvent event) noexcept {
    // Fabian Principle 1 / issue #1288: enum-as-lookup-key into a static
    // table. The former `switch` mapped each `HapticEvent` value to per-row
    // data; that data table is now indexed by the enum ordinal directly so
    // there is no central switch on the discriminator.
    //
    // Row order must match the `HapticEvent` declaration in
    // `app/systems/haptics.h`; a static_assert below pins the table size to
    // the trailing enumerator (`UIButtonTap`).
    constexpr std::array<TriggerPattern, 7> kPatternByEvent{{
        /* ShapeShift   */ TriggerPattern{ImpactStyle::Light,  1},
        /* LaneSwitch   */ TriggerPattern{ImpactStyle::Light,  1},
        /* JumpLand     */ TriggerPattern{ImpactStyle::Light,  1},
        /* DeathCrash   */ TriggerPattern{ImpactStyle::Heavy,  2},
        /* NewHighScore */ TriggerPattern{ImpactStyle::Medium, 1},
        /* RetryTap     */ TriggerPattern{ImpactStyle::Light,  1},
        /* UIButtonTap  */ TriggerPattern{ImpactStyle::Light,  1},
    }};
    static_assert(kPatternByEvent.size() ==
                  static_cast<std::size_t>(HapticEvent::UIButtonTap) + 1,
                  "kPatternByEvent must cover every HapticEvent enumerator");

    const auto idx = static_cast<std::size_t>(event);
    if (idx >= kPatternByEvent.size()) return TriggerPattern{ImpactStyle::Light, 1};
    return kPatternByEvent[idx];
}

void warmup(entt::registry& reg) noexcept {
#if defined(PLATFORM_IOS)
    if (!platform::ios::haptics_ios_available()) {
        return;
    }
    auto& state = runtime_state(reg);
    if (auto* ios_state = ensure_ios_state(state)) {
        platform::ios::haptics_ios_warmup(*ios_state);
    }
#else
    (void)reg;
#endif
}

void trigger(entt::registry& reg, HapticEvent event) noexcept {
#if defined(PLATFORM_IOS)
    if (platform::ios::haptics_ios_available()) {
        auto& state = runtime_state(reg);
        if (auto* ios_state = ensure_ios_state(state)) {
            platform::ios::haptics_ios_trigger(*ios_state, event);
            return;
        }
    }
    trace_haptic_event("HAPTIC [iOS-stub]: ", event);
#else
    (void)reg;
    trace_haptic_event("HAPTIC: ", event);
#endif
}

void shutdown(entt::registry& reg) noexcept {
#if defined(PLATFORM_IOS)
    if (!platform::ios::haptics_ios_available()) {
        return;
    }
    if (auto* state = reg.ctx().find<HapticsRuntimeState>()) {
        platform::ios::haptics_ios_destroy_state(state->ios);
    }
#else
    (void)reg;
#endif
}

}  // namespace platform::haptics
