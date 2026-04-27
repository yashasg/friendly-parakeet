#include "all_systems.h"

// Burnout system removed from gameplay (issue #239).
// Shape changes on beat must not be penalized for obstacle proximity.
// The function is kept as a no-op so existing call sites compile cleanly
// during the transition period.
void burnout_system(entt::registry& /*reg*/, float /*dt*/) {}
