#include "audio_runtime.h"

#include <algorithm>
#include <cmath>
#include <SDL.h>
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
#include <SDL_mixer.h>
#endif

namespace audio_runtime {

namespace {

float clamp_volume(float volume) noexcept {
    return std::clamp(volume, 0.0f, 1.0f);
}

std::uint32_t now_ticks_ms() noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    return SDL_GetTicks();
#else
    return 0u;
#endif
}

[[nodiscard]] bool backend_music_playing() noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    return Mix_PlayingMusic() != 0 && Mix_PausedMusic() == 0;
#else
    return false;
#endif
}

void reset_music_playback_state(MusicContext& music) noexcept {
    music.started = false;
    music.paused = false;
    music.playback_confirmed = false;
    music.last_start_failure_log_ticks_ms = 0;
}

void reset_music_timing_state(MusicContext& music) noexcept {
    music.started_ticks_ms = 0;
    music.paused_ticks_ms = 0;
    music.paused_accumulated_ms = 0;
    music.time_override_enabled = false;
    music.time_override_seconds = 0.0f;
}

}  // namespace

[[nodiscard]] bool init_audio_device(AudioDeviceRuntimeState& audio_device) noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    if (audio_device.mixer_ready) return true;

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0u) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
        audio_device.subsystem_started = true;
    }

    constexpr int required_flags = MIX_INIT_FLAC;
    audio_device.mix_init_flags = Mix_Init(required_flags);
    if ((audio_device.mix_init_flags & required_flags) != required_flags) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_mixer codec init missing FLAC support flag (got=0x%x): %s",
                    audio_device.mix_init_flags, Mix_GetError());
    }

    if (Mix_OpenAudio(48000, AUDIO_F32SYS, 2, 2048) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mix_OpenAudio failed: %s", Mix_GetError());
        shutdown_audio_device(audio_device);
        return false;
    }

    Mix_AllocateChannels(32);
    audio_device.mixer_ready = true;
    return true;
#else
    (void)audio_device;
    return true;
#endif
}

void shutdown_audio_device(AudioDeviceRuntimeState& audio_device) noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    if (audio_device.mixer_ready) {
        Mix_HaltMusic();
        Mix_CloseAudio();
        audio_device.mixer_ready = false;
    }
    if (audio_device.mix_init_flags != 0) {
        Mix_Quit();
        audio_device.mix_init_flags = 0;
    }
    if (audio_device.subsystem_started) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audio_device.subsystem_started = false;
    }
#else
    (void)audio_device;
#endif
}

[[nodiscard]] bool is_audio_device_ready(const AudioDeviceRuntimeState& audio_device) noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    return audio_device.mixer_ready;
#else
    (void)audio_device;
    return true;
#endif
}

[[nodiscard]] bool load_music_stream(MusicContext& music, const char* path, bool repeat) noexcept {
    if (!path) return false;

    unload_music_stream(music);

#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    Mix_Music* stream = Mix_LoadMUS(path);
    if (stream == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mix_LoadMUS failed for '%s': %s", path, Mix_GetError());
        return false;
    }
    music.stream = stream;
#else
    (void)repeat;
    return false;
#endif
    music.loaded = true;
    music.repeat = repeat;
    reset_music_playback_state(music);
    reset_music_timing_state(music);
    set_music_volume(music, music.volume);
    return true;
}

void unload_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
    if (music.started) {
        stop_music_stream(music);
    }
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    if (music.stream != nullptr) {
        Mix_FreeMusic(music.stream);
    }
#endif
    music.stream = nullptr;
    music.loaded = false;
    music.repeat = false;
    reset_music_playback_state(music);
    reset_music_timing_state(music);
}

void set_music_volume(MusicContext& music, float volume) noexcept {
    music.volume = clamp_volume(volume);
    if (!music.loaded || music.stream == nullptr) return;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    const auto mixer_volume = static_cast<int>(std::lround(music.volume * static_cast<float>(MIX_MAX_VOLUME)));
    Mix_VolumeMusic(std::clamp(mixer_volume, 0, MIX_MAX_VOLUME));
#endif
}

void update_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (music.time_override_enabled) return;
    (void)music;
}

void play_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
    if (music.time_override_enabled) {
        music.started = true;
        music.playback_confirmed = true;
        return;
    }
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    const int loop_count = music.repeat ? -1 : 0;
    if (Mix_PlayMusic(music.stream, loop_count) != 0) {
        const std::uint32_t now = now_ticks_ms();
        if (music.last_start_failure_log_ticks_ms == 0u ||
            now - music.last_start_failure_log_ticks_ms >= 1000u) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mix_PlayMusic failed: %s", Mix_GetError());
            music.last_start_failure_log_ticks_ms = now;
        }
        music.started = false;
        music.playback_confirmed = false;
        return;
    }
#else
    return;
#endif
    const std::uint32_t now = now_ticks_ms();
    const bool confirmed = backend_music_playing();
    if (!confirmed) {
        if (music.last_start_failure_log_ticks_ms == 0u ||
            now - music.last_start_failure_log_ticks_ms >= 1000u) {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Music playback failed to start (playing=%d paused=%d): %s",
                        static_cast<int>(Mix_PlayingMusic() != 0),
                        static_cast<int>(Mix_PausedMusic() != 0),
                        Mix_GetError());
#endif
            music.last_start_failure_log_ticks_ms = now;
        }
        music.started = false;
        music.playback_confirmed = false;
        return;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Music playback confirmed");
    music.started = true;
    music.playback_confirmed = true;
    music.paused = false;
    music.started_ticks_ms = now;
    music.paused_ticks_ms = 0;
    music.paused_accumulated_ms = 0;
}

void pause_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (music.paused) return;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    Mix_PauseMusic();
#endif
    music.paused = true;
    music.paused_ticks_ms = now_ticks_ms();
}

void resume_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (!music.paused) return;
    const std::uint32_t now = now_ticks_ms();
    if (music.paused_ticks_ms != 0u && now >= music.paused_ticks_ms) {
        music.paused_accumulated_ms += now - music.paused_ticks_ms;
    }
    music.paused_ticks_ms = 0;
    music.paused = false;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    Mix_ResumeMusic();
#endif
    music.playback_confirmed = backend_music_playing();
}

void stop_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (music.time_override_enabled) {
        music.started = false;
        return;
    }
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    Mix_HaltMusic();
#endif
    reset_music_playback_state(music);
    reset_music_timing_state(music);
}

[[nodiscard]] bool is_music_playing(const MusicContext& music) noexcept {
    if (!music.loaded || !music.started || music.paused) return false;
    if (music.time_override_enabled) return music.playback_confirmed;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    return backend_music_playing();
#else
    return music.playback_confirmed;
#endif
}

[[nodiscard]] float get_music_time_played(const MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return 0.0f;
    if (music.time_override_enabled) return std::max(0.0f, music.time_override_seconds);
    if (music.started_ticks_ms == 0u) return 0.0f;
    const std::uint32_t now = music.paused ? music.paused_ticks_ms : now_ticks_ms();
    if (now <= music.started_ticks_ms + music.paused_accumulated_ms) return 0.0f;
    return static_cast<float>(now - music.started_ticks_ms - music.paused_accumulated_ms) / 1000.0f;
}

void set_music_time_played_override(MusicContext& music, float seconds) noexcept {
    music.time_override_enabled = true;
    music.time_override_seconds = seconds;
}

void clear_music_time_played_override(MusicContext& music) noexcept {
    music.time_override_enabled = false;
    music.time_override_seconds = 0.0f;
}

}  // namespace audio_runtime
