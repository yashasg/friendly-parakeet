#include "all_systems.h"
#include "../components/audio.h"
#include "../components/registry_context.h"

void audio_system(entt::registry& reg) {
    auto* audio = registry_ctx_find<AudioQueue>(reg);
    if (!audio) return;

    auto* backend = registry_ctx_find<SFXPlaybackBackend>(reg);
    if (backend && backend->dispatch) {
        for (int i = 0; i < audio->count; ++i) {
            backend->dispatch(reg, audio->queue[i]);
        }
    }

    audio->clear();
}
