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

    void push(SFX sfx) {
        if (count < MAX_QUEUED) {
            queue[count++] = sfx;
        }
    }

    void clear() { count = 0; }
};
