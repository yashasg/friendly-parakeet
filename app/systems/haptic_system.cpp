#include "all_systems.h"
#include "../components/haptics.h"
#include "../components/settings.h"
#include <magic_enum/magic_enum.hpp>
#include <raylib.h>

// ── Platform notes ────────────────────────────────────────────────────────────
// Desktop / WASM: no platform vibration API — events are logged at DEBUG level
//   so they remain deterministic and observable in tests without real hardware.
// iOS: TODO(Hockney) — add UIImpactFeedbackGenerator bridge in a platform .mm
//   file and call it from the #if defined(PLATFORM_IOS) block below.
//   Do NOT add a silent fallback; unsupported platforms must be explicit here.
// ─────────────────────────────────────────────────────────────────────────────

static void trace_haptic_event(const char* prefix, HapticEvent e) {
    const auto event_name = magic_enum::enum_name(e);
    if (event_name.empty()) {
        TraceLog(LOG_DEBUG, "%sUnknown", prefix);
        return;
    }
    TraceLog(LOG_DEBUG, "%s%.*s", prefix, static_cast<int>(event_name.size()), event_name.data());
}

void haptic_system(entt::registry& reg) {
    auto* hq = reg.ctx().find<HapticQueue>();
    if (!hq || hq->count == 0) return;

    for (int i = 0; i < hq->count; ++i) {
        HapticEvent e = hq->queue[i];

#if defined(PLATFORM_IOS)
        // TODO(Hockney): call UIImpactFeedbackGenerator bridge here.
        // Example: haptic_ios_trigger(e);
        // Until implemented, log only — do NOT silently swallow the event.
        trace_haptic_event("HAPTIC [iOS-stub]: ", e);
#else
        // Desktop / WASM: log only — no platform vibration API available.
        trace_haptic_event("HAPTIC: ", e);
#endif
    }

    haptic_clear(*hq);
}
