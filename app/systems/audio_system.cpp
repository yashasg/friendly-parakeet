#include "all_systems.h"
#include "../audio/audio_types.h"

#include <raylib.h>

void audio_system(entt::registry& reg) {
    auto* audio = reg.ctx().find<AudioQueue>();
    if (!audio) return;

    auto* bank = reg.ctx().find<SFXBank>();
    if (bank && bank->loaded && IsAudioDeviceReady()) {
        for (int i = 0; i < audio->count; ++i) {
            const int idx = static_cast<int>(audio->queue[i]);
            if (idx < SFXBank::SFX_COUNT && bank->sound_loaded[idx] && IsSoundValid(bank->sounds[idx])) {
                PlaySound(bank->sounds[idx]);
            }
        }
    }

    audio->count = 0;
}
