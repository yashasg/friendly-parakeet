#include "haptics_backend.h"

#include "ios/haptics_ios_bridge.h"

#include <magic_enum/magic_enum.hpp>
#include <raylib.h>

namespace platform::haptics {
namespace {

#if defined(PLATFORM_IOS)
struct HapticsRuntimeState {
    platform::ios::HapticsIOSState* ios = nullptr;
};
#endif

void trace_haptic_event(const char* prefix, HapticEvent event) {
    const auto event_name = magic_enum::enum_name(event);
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
    switch (event) {
        case HapticEvent::LaneSwitch:
        case HapticEvent::UIButtonTap:
            return {ImpactStyle::Light, 1};
        case HapticEvent::ShapeShift:
        case HapticEvent::JumpLand:
        case HapticEvent::RetryTap:
            return {ImpactStyle::Light, 1};
        case HapticEvent::NewHighScore:
            return {ImpactStyle::Medium, 1};
        case HapticEvent::DeathCrash:
            return {ImpactStyle::Heavy, 2};
        default:
            return {ImpactStyle::Light, 1};
    }
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
