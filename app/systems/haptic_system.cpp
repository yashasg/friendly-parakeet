#include "all_systems.h"
#include "../components/haptics.h"
#include "../components/settings.h"
#include <raylib.h>

// ── Platform notes ────────────────────────────────────────────────────────────
// Desktop / WASM: no platform vibration API — events are logged at DEBUG level
//   so they remain deterministic and observable in tests without real hardware.
// iOS: TODO(Hockney) — add UIImpactFeedbackGenerator bridge in a platform .mm
//   file and call it from the #if defined(PLATFORM_IOS) block below.
//   Do NOT add a silent fallback; unsupported platforms must be explicit here.
// ─────────────────────────────────────────────────────────────────────────────

static const char* haptic_event_name(HapticEvent e) {
    switch (e) {
        case HapticEvent::ShapeShift:  return "ShapeShift";
        case HapticEvent::LaneSwitch:  return "LaneSwitch";
        case HapticEvent::JumpLand:    return "JumpLand";
        case HapticEvent::Burnout1_5x: return "Burnout1_5x";
        case HapticEvent::Burnout2_0x: return "Burnout2_0x";
        case HapticEvent::Burnout3_0x: return "Burnout3_0x";
        case HapticEvent::Burnout4_0x: return "Burnout4_0x";
        case HapticEvent::Burnout5_0x: return "Burnout5_0x";
        case HapticEvent::NearMiss:    return "NearMiss";
        case HapticEvent::DeathCrash:  return "DeathCrash";
        case HapticEvent::NewHighScore: return "NewHighScore";
        case HapticEvent::RetryTap:    return "RetryTap";
        case HapticEvent::UIButtonTap: return "UIButtonTap";
    }
    return "Unknown";
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
        TraceLog(LOG_DEBUG, "HAPTIC [iOS-stub]: %s", haptic_event_name(e));
#else
        // Desktop / WASM: log only — no platform vibration API available.
        TraceLog(LOG_DEBUG, "HAPTIC: %s", haptic_event_name(e));
#endif
    }

    haptic_clear(*hq);
}
