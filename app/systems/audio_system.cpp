#include "all_systems.h"
#include "audio_routing.h"
#include "audio_events.h"
#include "../components/loaded_sfx.h"

#include <entt/entt.hpp>
#include <raylib.h>

// Walks `view<LoadedSfx>` for a row whose `key` matches the event clip.
// Presence in the storage IS "loaded": the former
// `bank->sound_loaded[idx] && IsSoundValid(bank->sounds[idx])` parallel
// NULL-column + validity gate is eradicated by construction (#1616).
void audio_handle_play_sfx(entt::registry& reg, const PlaySfxEvent& evt) {
    if (!IsAudioDeviceReady()) return;
    for (auto [entity, row] : reg.view<LoadedSfx>().each()) {
        if (row.key == evt.clip) {
            PlaySound(row.sound);
            return;
        }
    }
}

void audio_system(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;
    disp->update<PlaySfxEvent>();
}
