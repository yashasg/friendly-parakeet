#import "haptics_ios_bridge.h"

#import <UIKit/UIKit.h>

#include <array>
#include <cstddef>
#include <new>

namespace platform::ios {
struct HapticsIOSState {
    UIImpactFeedbackGenerator* light = nil;
    UIImpactFeedbackGenerator* medium = nil;
    UIImpactFeedbackGenerator* heavy = nil;
};

namespace {

struct IOSPattern {
    UIImpactFeedbackStyle style = UIImpactFeedbackStyleLight;
    int pulse_count = 1;
};

// Fabian Principle 1 / issue #1316: enum-as-lookup-key into a static table,
// mirroring `pattern_for_event` in `haptics_backend.cpp:47-72` (issue #1288).
// Row order must match the `HapticEvent` declaration in
// `app/systems/haptics.h`; the `static_assert` pins the table size to the
// trailing enumerator (`UIButtonTap`).
IOSPattern ios_pattern_for_event(HapticEvent event) {
    constexpr std::array<IOSPattern, 7> kIOSPatternByEvent{{
        /* ShapeShift   */ IOSPattern{UIImpactFeedbackStyleLight,  1},
        /* LaneSwitch   */ IOSPattern{UIImpactFeedbackStyleLight,  1},
        /* JumpLand     */ IOSPattern{UIImpactFeedbackStyleLight,  1},
        /* DeathCrash   */ IOSPattern{UIImpactFeedbackStyleHeavy,  2},
        /* NewHighScore */ IOSPattern{UIImpactFeedbackStyleMedium, 1},
        /* RetryTap     */ IOSPattern{UIImpactFeedbackStyleLight,  1},
        /* UIButtonTap  */ IOSPattern{UIImpactFeedbackStyleLight,  1},
    }};
    static_assert(kIOSPatternByEvent.size() ==
                  static_cast<std::size_t>(HapticEvent::UIButtonTap) + 1,
                  "kIOSPatternByEvent must cover every HapticEvent enumerator");

    const auto idx = static_cast<std::size_t>(event);
    if (idx >= kIOSPatternByEvent.size()) return IOSPattern{UIImpactFeedbackStyleLight, 1};
    return kIOSPatternByEvent[idx];
}

UIImpactFeedbackGenerator* generator_for_style(HapticsIOSState& state,
                                                UIImpactFeedbackStyle style) {
    UIImpactFeedbackGenerator** slot = nullptr;
    switch (style) {
        case UIImpactFeedbackStyleLight:
            slot = &state.light;
            break;
        case UIImpactFeedbackStyleMedium:
            slot = &state.medium;
            break;
        case UIImpactFeedbackStyleHeavy:
            slot = &state.heavy;
            break;
        default:
            slot = &state.light;
            break;
    }

    if (*slot == nil) {
        *slot = [[UIImpactFeedbackGenerator alloc] initWithStyle:style];
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

HapticsIOSState* haptics_ios_create_state() noexcept {
    return new (std::nothrow) HapticsIOSState{};
}

void haptics_ios_destroy_state(HapticsIOSState*& state) noexcept {
    if (state == nullptr) {
        return;
    }
    haptics_ios_reset(*state);
    delete state;
    state = nullptr;
}

void haptics_ios_warmup(HapticsIOSState& state) noexcept {
    @autoreleasepool {
        [generator_for_style(state, UIImpactFeedbackStyleLight) prepare];
        [generator_for_style(state, UIImpactFeedbackStyleMedium) prepare];
        [generator_for_style(state, UIImpactFeedbackStyleHeavy) prepare];
    }
}

void haptics_ios_trigger(HapticsIOSState& state, HapticEvent event) noexcept {
    @autoreleasepool {
        const auto pattern = ios_pattern_for_event(event);
        const int pulses = (pattern.pulse_count > 0) ? pattern.pulse_count : 1;
        for (int i = 0; i < pulses; ++i) {
            UIImpactFeedbackGenerator* generator = generator_for_style(state, pattern.style);
            [generator prepare];
            [generator impactOccurred];
        }
    }
}

void haptics_ios_reset(HapticsIOSState& state) noexcept {
    @autoreleasepool {
        destroy_generator(state.light);
        destroy_generator(state.medium);
        destroy_generator(state.heavy);
    }
}

}  // namespace platform::ios
