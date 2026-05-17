// Tutorial "DODGE LANES" hint copy — runtime selection.
//
// Issue #513: per-platform copy used to be picked at compile time via
// `#ifdef PLATFORM_HAS_KEYBOARD`, but PLATFORM_HAS_KEYBOARD is defined for
// both Desktop and Web (CMakeLists.txt), so a touch-only mobile browser
// was being told keyboard-only copy even though the runtime accepts swipes
// (#499). Selection now happens at runtime in
// `app/systems/tutorial_dodge_hint_bind_system.cpp` via this pure helper,
// so the call site is unit-testable without an OpenGL context.

#ifndef SHAPESHIFTER_TUTORIAL_DODGE_HINT_H
#define SHAPESHIFTER_TUTORIAL_DODGE_HINT_H

#include <raylib.h>

inline const char* tutorial_dodge_hint_text(bool prefer_touch) {
    return prefer_touch ? "SWIPE LEFT OR RIGHT" : "DODGE: A/D OR ARROWS";
}

// Bounds for the dodge-hint label — must match the (110, 710, 500, 32)
// rectangle authored in content/ui/screens/tutorial.rgl for both
// DodgeHintDesktop (id 007) and DodgeHintWeb (id 008).
inline Rectangle tutorial_dodge_hint_bounds(Vector2 anchor) {
    return Rectangle{anchor.x + 110.0f, anchor.y + 710.0f, 500.0f, 32.0f};
}

#endif // SHAPESHIFTER_TUTORIAL_DODGE_HINT_H
