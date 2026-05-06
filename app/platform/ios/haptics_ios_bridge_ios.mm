#import "haptics_ios_bridge.h"

#import "../../systems/haptics_runtime.h"

#import <UIKit/UIKit.h>

namespace platform::ios {
namespace {

UIImpactFeedbackStyle to_ios_style(haptics_runtime::ImpactStyle style) {
    switch (style) {
        case haptics_runtime::ImpactStyle::Light:
            return UIImpactFeedbackStyleLight;
        case haptics_runtime::ImpactStyle::Medium:
            return UIImpactFeedbackStyleMedium;
        case haptics_runtime::ImpactStyle::Heavy:
            return UIImpactFeedbackStyleHeavy;
        default:
            return UIImpactFeedbackStyleLight;
    }
}

UIImpactFeedbackGenerator* generator_for_style(haptics_runtime::ImpactStyle style) {
    static UIImpactFeedbackGenerator* light = nil;
    static UIImpactFeedbackGenerator* medium = nil;
    static UIImpactFeedbackGenerator* heavy = nil;

    UIImpactFeedbackGenerator** slot = nullptr;
    switch (style) {
        case haptics_runtime::ImpactStyle::Light:
            slot = &light;
            break;
        case haptics_runtime::ImpactStyle::Medium:
            slot = &medium;
            break;
        case haptics_runtime::ImpactStyle::Heavy:
            slot = &heavy;
            break;
        default:
            slot = &light;
            break;
    }

    if (*slot == nil) {
        *slot = [[UIImpactFeedbackGenerator alloc] initWithStyle:to_ios_style(style)];
    }
    return *slot;
}

}  // namespace

bool haptics_ios_available() noexcept {
    return true;
}

void haptics_ios_warmup() noexcept {
    @autoreleasepool {
        UIImpactFeedbackGenerator* light = generator_for_style(haptics_runtime::ImpactStyle::Light);
        UIImpactFeedbackGenerator* medium = generator_for_style(haptics_runtime::ImpactStyle::Medium);
        UIImpactFeedbackGenerator* heavy = generator_for_style(haptics_runtime::ImpactStyle::Heavy);
        [light prepare];
        [medium prepare];
        [heavy prepare];
    }
}

void haptics_ios_trigger(HapticEvent event) noexcept {
    @autoreleasepool {
        const auto pattern = haptics_runtime::pattern_for_event(event);
        const int pulses = (pattern.pulse_count > 0) ? pattern.pulse_count : 1;
        for (int i = 0; i < pulses; ++i) {
            UIImpactFeedbackGenerator* generator = generator_for_style(pattern.style);
            [generator prepare];
            [generator impactOccurred];
        }
    }
}

}  // namespace platform::ios
