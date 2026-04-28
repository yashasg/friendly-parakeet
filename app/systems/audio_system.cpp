#include "all_systems.h"
#include "audio_types.h"

void audio_system(entt::registry& reg) {
    auto* audio = reg.ctx().find<AudioQueue>();
    if (!audio) return;

    auto* backend = reg.ctx().find<SFXPlaybackBackend>();
    if (backend && backend->dispatch) {
        for (int i = 0; i < audio->count; ++i) {
            backend->dispatch(reg, audio->queue[i]);
        }
    }

    audio_clear(*audio);
}
