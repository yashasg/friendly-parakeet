// Web input-policy accessor.
//
// The WebInputPolicy struct is owned by input_system.cpp and lives in
// reg.ctx() so a single per-session "is this browser touch-capable?" flag
// can be reused across systems and screen controllers (e.g. the tutorial
// dodge-hint copy in #513). This header only exposes a read-only accessor;
// the underlying struct deliberately stays internal so callers cannot
// mutate the latched capability bit.

#ifndef SHAPESHIFTER_WEB_INPUT_POLICY_H
#define SHAPESHIFTER_WEB_INPUT_POLICY_H

#include <entt/entt.hpp>

// Returns true iff the current build is PLATFORM_WEB AND
// navigator.maxTouchPoints reported a touch surface at startup. On native
// builds the helper returns false — callers must combine this with their
// own #ifdef PLATFORM_HAS_KEYBOARD checks if they need a "this device
// cannot type" answer (e.g. iOS).
bool web_input_touch_capable(const entt::registry& reg);

#endif // SHAPESHIFTER_WEB_INPUT_POLICY_H
