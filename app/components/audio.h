#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <magic_enum/magic_enum.hpp>
#include <SDL_mixer.h>

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

static_assert(magic_enum::enum_count<SFX>() == 9,
              "SFX enum changed — update sfx_bank_system.cpp SFX_SPECS and this guard");
static_assert(static_cast<int>(SFX::ShapeShift) == 0,
              "SFX enum must be zero-based for array indexing");

struct AudioQueue {
    static constexpr int MAX_QUEUED = 16;
    SFX queue[MAX_QUEUED] = {};
    int count = 0;

    void push(SFX sfx) noexcept {
        if (count < MAX_QUEUED) {
            queue[count++] = sfx;
        }
    }

    void clear() noexcept { count = 0; }
};

struct SFXPlaybackBackend {
    void (*dispatch)(entt::registry& reg, SFX sfx) = nullptr;
};

struct SFXBank {
    static constexpr int SFX_COUNT = static_cast<int>(magic_enum::enum_count<SFX>());
    Mix_Chunk* sounds[SFX_COUNT]  = {};
    bool  sound_loaded[SFX_COUNT] = {};
    bool  loaded                  = false;
};

struct MusicContext {
    Mix_Music* stream = nullptr;
    bool  loaded  = false;
    bool  repeat = false;
    bool  started = false;
    bool  paused = false;
    bool  playback_confirmed = false;
    std::uint32_t started_ticks_ms = 0;
    std::uint32_t paused_ticks_ms = 0;
    std::uint32_t paused_accumulated_ms = 0;
    std::uint32_t last_start_failure_log_ticks_ms = 0;
    bool  time_override_enabled = false;
    float time_override_seconds = 0.0f;
    float volume  = 0.8f;
};

struct AudioDeviceRuntimeState {
    bool subsystem_started = false;
    bool mixer_ready = false;
    int mix_init_flags = 0;
};
