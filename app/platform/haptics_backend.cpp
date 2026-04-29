#include "haptics_backend.h"

#include "ios/haptics_ios_bridge.h"

#include <magic_enum/magic_enum.hpp>
#include <raylib.h>

namespace platform::haptics {
namespace {

void trace_haptic_event(const char* prefix, HapticEvent event) {
    const auto event_name = magic_enum::enum_name(event);
    if (event_name.empty()) {
        TraceLog(LOG_DEBUG, "%sUnknown", prefix);
        return;
    }
    TraceLog(LOG_DEBUG, "%s%.*s", prefix, static_cast<int>(event_name.size()), event_name.data());
}

}  // namespace

TriggerPattern pattern_for_event(HapticEvent event) noexcept {
    switch (event) {
        case HapticEvent::LaneSwitch:
        case HapticEvent::UIButtonTap:
            return {ImpactStyle::Light, 1};
        case HapticEvent::ShapeShift:
        case HapticEvent::JumpLand:
        case HapticEvent::Burnout1_5x:
        case HapticEvent::RetryTap:
            return {ImpactStyle::Light, 1};
        case HapticEvent::Burnout2_0x:
        case HapticEvent::Burnout3_0x:
        case HapticEvent::NewHighScore:
            return {ImpactStyle::Medium, 1};
        case HapticEvent::Burnout4_0x:
        case HapticEvent::NearMiss:
            return {ImpactStyle::Heavy, 1};
        case HapticEvent::Burnout5_0x:
            return {ImpactStyle::Heavy, 3};
        case HapticEvent::DeathCrash:
            return {ImpactStyle::Heavy, 2};
        default:
            return {ImpactStyle::Light, 1};
    }
}

void warmup() noexcept {
#if defined(PLATFORM_IOS)
    if (platform::ios::haptics_ios_available()) {
        platform::ios::haptics_ios_warmup();
    }
#endif
}

void trigger(HapticEvent event) noexcept {
#if defined(PLATFORM_IOS)
    if (platform::ios::haptics_ios_available()) {
        platform::ios::haptics_ios_trigger(event);
        return;
    }
    trace_haptic_event("HAPTIC [iOS-stub]: ", event);
#else
    trace_haptic_event("HAPTIC: ", event);
#endif
}

}  // namespace platform::haptics
