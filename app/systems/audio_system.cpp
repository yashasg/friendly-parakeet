#include "all_systems.h"
#include "../components/audio.h"

void audio_system(entt::registry& reg) {
    auto* audio = reg.ctx().find<AudioQueue>();
    if (!audio) return;

    // Future: iterate audio->queue[0..count-1], call PlaySound() for each SFX.
    // For now: drain the queue so stale events don't accumulate.
    audio_clear(*audio);
}
