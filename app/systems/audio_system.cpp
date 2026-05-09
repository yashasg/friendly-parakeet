#include "all_systems.h"
#include "../audio/audio_routing.h"
#include "../audio/audio_types.h"
#include "../components/audio_events.h"

#include <raylib.h>

void audio_handle_play_sfx(entt::registry& reg, const PlaySfxEvent& evt) {
    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank || !bank->loaded || !IsAudioDeviceReady()) return;
    const int idx = static_cast<int>(evt.clip);
    if (idx < SFXBank::SFX_COUNT && bank->sound_loaded[idx] && IsSoundValid(bank->sounds[idx])) {
        PlaySound(bank->sounds[idx]);
    }
}

void audio_system(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;
    disp->update<PlaySfxEvent>();
}

