#include "all_systems.h"
#include "../audio/audio_routing.h"
#include "../components/audio_events.h"
#include "../components/haptics.h"
#include "../platform/haptics_backend.h"
#include "../util/settings.h"

void haptic_handle_play(entt::registry& reg, const PlayHapticEvent& evt) {
    auto* settings = reg.ctx().find<SettingsState>();
    if (settings && !settings->haptics_enabled) return;
    platform::haptics::trigger(reg, evt.evt);
}

void haptic_system(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;
    disp->update<PlayHapticEvent>();
}

