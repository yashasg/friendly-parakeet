#pragma once

#include "audio_types.h"

inline void audio_push(AudioQueue& q, SFX sfx) {
    if (q.count < AudioQueue::MAX_QUEUED) {
        q.queue[q.count++] = sfx;
    }
}

inline void audio_clear(AudioQueue& q) { q.count = 0; }
