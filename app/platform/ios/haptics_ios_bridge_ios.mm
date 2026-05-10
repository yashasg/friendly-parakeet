#import "haptics_ios_bridge.h"

#import "../haptics_backend.h"

#import <UIKit/UIKit.h>

namespace platform::ios {
namespace {

struct HapticsIOSState {
    UIImpactFeedbackGenerator* light = nil;
    UIImpactFeedbackGenerator* medium = nil;
    UIImpactFeedbackGenerator* heavy = nil;
};

HapticsIOSState& haptics_state() {
    static HapticsIOSState state;
    return state;
}

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

UIImpactFeedbackGenerator* generator_for_style(HapticsIOSState& state,
                                                platform::haptics::ImpactStyle style) {
    UIImpactFeedbackGenerator** slot = nullptr;
    switch (style) {
        case platform::haptics::ImpactStyle::Light:
            slot = &state.light;
            break;
        case platform::haptics::ImpactStyle::Medium:
            slot = &state.medium;
            break;
        case platform::haptics::ImpactStyle::Heavy:
            slot = &state.heavy;
            break;
        default:
            slot = &state.light;
            break;
    }

    if (*slot == nil) {
        *slot = [[UIImpactFeedbackGenerator alloc] initWithStyle:to_ios_style(style)];
    }
    return *slot;
}

void destroy_generator(UIImpactFeedbackGenerator*& generator) {
    if (generator == nil) {
        return;
    }
#if !__has_feature(objc_arc)
    [generator release];
#endif
    generator = nil;
}

}  // namespace

bool haptics_ios_available() noexcept {
    return true;
}

void haptics_ios_warmup() noexcept {
    @autoreleasepool {
        auto& state = haptics_state();
        [generator_for_style(state, platform::haptics::ImpactStyle::Light) prepare];
        [generator_for_style(state, platform::haptics::ImpactStyle::Medium) prepare];
        [generator_for_style(state, platform::haptics::ImpactStyle::Heavy) prepare];
    }
}

void haptics_ios_trigger(HapticEvent event) noexcept {
    @autoreleasepool {
        auto& state = haptics_state();
        const auto pattern = platform::haptics::pattern_for_event(event);
        const int pulses = (pattern.pulse_count > 0) ? pattern.pulse_count : 1;
        for (int i = 0; i < pulses; ++i) {
            UIImpactFeedbackGenerator* generator = generator_for_style(state, pattern.style);
            [generator prepare];
            [generator impactOccurred];
        }
    }
}

void haptics_ios_reset() noexcept {
    @autoreleasepool {
        auto& state = haptics_state();
        destroy_generator(state.light);
        destroy_generator(state.medium);
        destroy_generator(state.heavy);
    }
}

}  // namespace platform::ios
