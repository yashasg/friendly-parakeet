#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <magic_enum/magic_enum.hpp>
#include <raylib.h>

enum class SFX : uint8_t {
    ShapeShift = 0,
    Crash,
    UITap,
    ChainBonus,
    ZoneRisky,
    ZoneDanger,
    ZoneDead,
    ScorePopup,
    GameStart,
};

// Compile-time guard: SFX values must be contiguous and zero-based so that
// static arrays indexed by static_cast<int>(sfx) are always in-bounds.
static_assert(magic_enum::enum_count<SFX>() == 9,
              "SFX enum changed — update sfx_bank.cpp SFX_SPECS and this guard");
static_assert(static_cast<int>(SFX::ShapeShift) == 0,
              "SFX enum must be zero-based for array indexing");

struct AudioQueue {
    static constexpr int MAX_QUEUED = 16;
    SFX queue[MAX_QUEUED] = {};
    int count = 0;
};

// Optional dispatch backend.  Tests can inject this to capture SFX calls;
// production code wires it to PlaySound().  If absent, the queue is drained
// silently (headless / test mode).
//
// The registry is passed explicitly at the call site (audio_system) so that
// no raw pointer to the registry is stored inside this struct.
struct SFXPlaybackBackend {
    void (*dispatch)(entt::registry& reg, SFX sfx) = nullptr;
};

// Resident sound bank initialised by sfx_bank_init (app/audio/sfx_bank.cpp).
struct SFXBank {
    static constexpr int SFX_COUNT = static_cast<int>(magic_enum::enum_count<SFX>());
    Sound sounds[SFX_COUNT]       = {};
    bool  sound_loaded[SFX_COUNT] = {};
    bool  loaded                  = false;
};
