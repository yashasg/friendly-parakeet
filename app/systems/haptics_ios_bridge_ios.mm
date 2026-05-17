#import "haptics_ios_bridge.h"

#import <UIKit/UIKit.h>

#include <array>
#include <cstddef>
#include <new>

namespace platform::ios {

// UIKit's `UIImpactFeedbackStyle` declares Light/Medium/Heavy as the contiguous
// integral values 0/1/2 — the only styles we ever pass through this bridge
// (see `kIOSPatternByEvent` below). Future vendor extensions (Soft/Rigid on
// iOS 13+) fall outside this range and collapse to the Light slot via
// `generator_slot_for_style`. Keep this constant in sync with the slot count
// referenced in #1323 if a new style is ever added to the pattern table.
constexpr std::size_t kHapticsGeneratorSlotCount = 3;

struct HapticsIOSState {
    std::array<UIImpactFeedbackGenerator*, kHapticsGeneratorSlotCount> generators{};
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

// Fabian Principle 1 / issue #1323: positional slot lookup replaces a
// label→pointer `switch` over `UIImpactFeedbackStyle`. UIKit guarantees
// `Light=0/Medium=1/Heavy=2` (the only styles we pass); any out-of-range
// vendor extension (`Soft`/`Rigid`, iOS 13+) collapses to the Light slot.
std::size_t generator_slot_for_style(UIImpactFeedbackStyle style) noexcept {
    const auto idx = static_cast<std::size_t>(style);
    return idx < kHapticsGeneratorSlotCount ? idx : 0;
}

UIImpactFeedbackGenerator* generator_for_style(HapticsIOSState& state,
                                                UIImpactFeedbackStyle style) {
    UIImpactFeedbackGenerator*& slot =
        state.generators[generator_slot_for_style(style)];
    if (slot == nil) {
        slot = [[UIImpactFeedbackGenerator alloc] initWithStyle:style];
    }
    return slot;
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
        for (auto& generator : state.generators) {
            destroy_generator(generator);
        }
    }
}

}  // namespace platform::ios
