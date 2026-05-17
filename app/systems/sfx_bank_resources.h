#pragma once

#include "../components/audio.h"  // SFX enum (drives SFX_COUNT)

#include <raylib.h>

// Audio runtime resource owner: holds the resident raylib Sound handles for
// each `SFX`. Lives in registry context (not on an entity), populated by
// `sfx_bank_init` and torn down by `sfx_bank_unload` / the destructor.
//
// Relocated out of `app/components/audio.h` (issue #1358, follow-up to
// #1351) because this is a ctx-singleton RAII wrapper, not entity-owned
// plain data — `app/components/` stays reserved for atomic, queryable
// entity-owned tables per .squad/decisions.md §"app/components parallel
// audit verdict (consolidated)".
struct SFXBank {
    static constexpr int SFX_COUNT = static_cast<int>(SFX::Count);
    Sound sounds[SFX_COUNT]       = {};
    bool  sound_loaded[SFX_COUNT] = {};
    bool  loaded                  = false;

    SFXBank() = default;
    SFXBank(const SFXBank&) = delete;
    SFXBank& operator=(const SFXBank&) = delete;
    SFXBank(SFXBank&& other) noexcept;
    SFXBank& operator=(SFXBank&& other) noexcept;
    ~SFXBank();

    void release();
};
