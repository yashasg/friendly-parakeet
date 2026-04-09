#pragma once

#include <cstdint>

enum class SFX : uint8_t {
    ShapeShift = 0,
    BurnoutBank,
    Crash,
    UITap,
    ChainBonus,
    ZoneRisky,
    ZoneDanger,
    ZoneDead,
    ScorePopup,
    GameStart,
    COUNT
};

struct AudioQueue {
    static constexpr int MAX_QUEUED = 16;
    SFX queue[MAX_QUEUED] = {};
    int count = 0;
};

inline void audio_push(AudioQueue& q, SFX sfx) {
    if (q.count < AudioQueue::MAX_QUEUED) {
        q.queue[q.count++] = sfx;
    }
}

inline void audio_clear(AudioQueue& q) { q.count = 0; }
