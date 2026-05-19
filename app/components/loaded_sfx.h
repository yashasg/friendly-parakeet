#pragma once

#include "audio.h"

#include <raylib.h>

// One row per currently-loaded SFX (issue #1616 / Fabian Principle 3).
//
// Presence in `reg.view<LoadedSfx>()` IS membership in "loaded sfx" — the
// former `SFXBank` (app/systems/sfx_bank_resources.h, removed) carried a
// `Sound sounds[SFX_COUNT]` array column plus a parallel `bool
// sound_loaded[SFX_COUNT]` NULL-column cursor, both eradicated by this
// normalization. Sounds that fail `IsSoundValid` at load time never
// produce a row, so the emit site no longer needs a validity gate
// (presence ⇒ valid).
//
// Precedent shape (#1554 / #1560 / #1562 / #1611 / #1612):
//   fixed-size `T arr[N]` columns are the static-cap equivalent of
//   `std::vector<T>` and fail 1NF for the same reason. Each (Sound, SFX)
//   pair becomes a row in its own table, with the registry singleton as
//   the implicit foreign key.
//
// RAII: `Sound` wraps raylib's audio buffers via raw pointers, so this
// component owns its handle and unloads on destruction (or move-from).
// Copying is disallowed — only one row may own a given `Sound`. Walks
// via `view<LoadedSfx>` are read-only; mutating the storage must go
// through `reg.create()` / `reg.destroy()` so the destructor fires.
struct LoadedSfx {
    SFX   key{};
    Sound sound{};

    LoadedSfx() noexcept = default;
    LoadedSfx(SFX k, Sound s) noexcept : key{k}, sound{s} {}

    LoadedSfx(const LoadedSfx&)            = delete;
    LoadedSfx& operator=(const LoadedSfx&) = delete;

    LoadedSfx(LoadedSfx&& other) noexcept
        : key{other.key}, sound{other.sound} {
        other.sound = Sound{};
    }

    LoadedSfx& operator=(LoadedSfx&& other) noexcept {
        if (this != &other) {
            if (IsSoundValid(sound)) UnloadSound(sound);
            key         = other.key;
            sound       = other.sound;
            other.sound = Sound{};
        }
        return *this;
    }

    ~LoadedSfx() {
        if (IsSoundValid(sound)) UnloadSound(sound);
    }
};
