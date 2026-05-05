#include "music_backend.h"

#include <algorithm>
#include "runtime/runtime_api.h"
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
#include "../platform/sdl2/sdl2_headers.h"
#endif

namespace platform::audio {

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
        TraceLog(LOG_WARNING, "SDL_mixer codec init missing FLAC support flag (got=0x%x): %s",
                 audio_device.mix_init_flags, Mix_GetError());
    }

    if (Mix_OpenAudio(48000, AUDIO_F32SYS, 2, 2048) != 0) {
        TraceLog(LOG_WARNING, "Mix_OpenAudio failed: %s", Mix_GetError());
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

    Music stream = LoadMusicStream(path);
    stream.looping = repeat;
    if (stream.frameCount == 0) return false;

    music.stream = stream;
    music.loaded = true;
    music.started = false;
    music.paused = false;
    music.started_ticks_ms = 0;
    music.paused_ticks_ms = 0;
    music.paused_accumulated_ms = 0;
    music.playback_confirmed = false;
    music.last_start_failure_log_ticks_ms = 0;
    music.time_override_enabled = false;
    music.time_override_seconds = 0.0f;
    set_music_volume(music, music.volume);
    return true;
}

void unload_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
    if (music.started) {
        StopMusicStream(music.stream);
    }
    UnloadMusicStream(music.stream);
    music.stream = Music{};
    music.loaded = false;
    music.started = false;
    music.paused = false;
    music.started_ticks_ms = 0;
    music.paused_ticks_ms = 0;
    music.paused_accumulated_ms = 0;
    music.playback_confirmed = false;
    music.last_start_failure_log_ticks_ms = 0;
    music.time_override_enabled = false;
    music.time_override_seconds = 0.0f;
}

void set_music_volume(MusicContext& music, float volume) noexcept {
    music.volume = clamp_volume(volume);
    if (!music.loaded) return;
    SetMusicVolume(music.stream, music.volume);
}

void update_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (music.time_override_enabled) return;
    UpdateMusicStream(music.stream);
}

void play_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
    if (music.time_override_enabled) {
        music.started = true;
        music.playback_confirmed = true;
        return;
    }
    PlayMusicStream(music.stream);
    const std::uint32_t now = now_ticks_ms();
    const bool confirmed = backend_music_playing();
    if (!confirmed) {
        if (music.last_start_failure_log_ticks_ms == 0u ||
            now - music.last_start_failure_log_ticks_ms >= 1000u) {
            TraceLog(LOG_WARNING, "Music playback failed to start (playing=%d paused=%d): %s",
                     static_cast<int>(
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
                         Mix_PlayingMusic() != 0
#else
                         false
#endif
                     ),
                     static_cast<int>(
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
                         Mix_PausedMusic() != 0
#else
                         false
#endif
                     ),
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
                     Mix_GetError()
#else
                     "no backend"
#endif
            );
            music.last_start_failure_log_ticks_ms = now;
        }
        music.started = false;
        music.playback_confirmed = false;
        return;
    }
    TraceLog(LOG_INFO, "Music playback confirmed");
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
    PauseMusicStream(music.stream);
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
    ResumeMusicStream(music.stream);
    music.playback_confirmed = backend_music_playing();
}

void stop_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (music.time_override_enabled) {
        music.started = false;
        return;
    }
    StopMusicStream(music.stream);
    music.started = false;
    music.paused = false;
    music.playback_confirmed = false;
    music.started_ticks_ms = 0;
    music.paused_ticks_ms = 0;
    music.paused_accumulated_ms = 0;
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

}  // namespace platform::audio
