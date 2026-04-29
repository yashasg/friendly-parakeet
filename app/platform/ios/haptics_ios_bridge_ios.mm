#import "haptics_ios_bridge.h"

#import "../haptics_backend.h"

#import <UIKit/UIKit.h>

namespace platform::ios {
namespace {

UIImpactFeedbackStyle to_ios_style(platform::haptics::ImpactStyle style) {
    switch (style) {
        case platform::haptics::ImpactStyle::Light:
            return UIImpactFeedbackStyleLight;
        case platform::haptics::ImpactStyle::Medium:
            return UIImpactFeedbackStyleMedium;
        case platform::haptics::ImpactStyle::Heavy:
            return UIImpactFeedbackStyleHeavy;
        default:
            return UIImpactFeedbackStyleLight;
    }
}

UIImpactFeedbackGenerator* generator_for_style(platform::haptics::ImpactStyle style) {
    static UIImpactFeedbackGenerator* light = nil;
    static UIImpactFeedbackGenerator* medium = nil;
    static UIImpactFeedbackGenerator* heavy = nil;

    UIImpactFeedbackGenerator** slot = nullptr;
    switch (style) {
        case platform::haptics::ImpactStyle::Light:
            slot = &light;
            break;
        case platform::haptics::ImpactStyle::Medium:
            slot = &medium;
            break;
        case platform::haptics::ImpactStyle::Heavy:
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
        UIImpactFeedbackGenerator* light = generator_for_style(platform::haptics::ImpactStyle::Light);
        UIImpactFeedbackGenerator* medium = generator_for_style(platform::haptics::ImpactStyle::Medium);
        UIImpactFeedbackGenerator* heavy = generator_for_style(platform::haptics::ImpactStyle::Heavy);
        [light prepare];
        [medium prepare];
        [heavy prepare];
    }
}

void haptics_ios_trigger(HapticEvent event) noexcept {
    @autoreleasepool {
        const auto pattern = platform::haptics::pattern_for_event(event);
        const int pulses = (pattern.pulse_count > 0) ? pattern.pulse_count : 1;
        for (int i = 0; i < pulses; ++i) {
            UIImpactFeedbackGenerator* generator = generator_for_style(pattern.style);
            [generator prepare];
            [generator impactOccurred];
        }
    }
}

}  // namespace platform::ios
