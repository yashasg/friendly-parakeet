#include "all_systems.h"
#include "audio_routing.h"
#include "audio_events.h"
#include "haptics.h"
#include "haptics_backend.h"
#include "../entities/settings.h"

void haptic_handle_play(entt::registry& reg, const PlayHapticEvent& evt) {
    auto* settings = find_settings_state(reg);
    if (settings && !settings->haptics_enabled) return;
    platform::haptics::trigger(reg, evt.evt);
}

void haptic_system(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;
    disp->update<PlayHapticEvent>();
}

