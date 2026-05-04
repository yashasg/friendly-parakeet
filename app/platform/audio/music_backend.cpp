#include "music_backend.h"

#include <algorithm>
#include <cstdint>
#include <vector>
#include "platform/runtime_api.h"

#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
#include "../sdl2/sdl2_headers.h"
#endif
namespace platform::audio {

namespace {

float clamp_volume(float volume) noexcept {
    return std::clamp(volume, 0.0f, 1.0f);
}

#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
struct SdlMusicStreamState {
    std::vector<std::uint8_t> pcm_base_bytes{};
    std::vector<std::uint8_t> pcm_playback_bytes{};
    float bytes_per_second = 0.0f;
    float duration_seconds = 0.0f;
    bool repeat = false;
};

struct SdlAudioState {
    bool subsystem_started = false;
    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec spec{};
    const MusicContext* owner = nullptr;
    SdlMusicStreamState stream{};
};

struct MusicTimeOverride {
    bool enabled = false;
    float seconds = 0.0f;
};

SdlAudioState& sdl_audio_state() noexcept {
    static SdlAudioState state;
    return state;
}

MusicTimeOverride& music_time_override() noexcept {
    static MusicTimeOverride state;
    return state;
}

[[nodiscard]] bool decode_audio_file(const char* path,
                                     std::vector<float>& out_pcm,
                                     unsigned int& out_channels,
                                     unsigned int& out_sample_rate) noexcept {
    Wave wave = LoadWave(path);
    if (wave.frameCount == 0 || wave.channels == 0 || wave.sampleRate == 0) return false;
    float* samples = LoadWaveSamples(wave);
    if (!samples) {
        UnloadWave(wave);
        return false;
    }
    const size_t sample_count = static_cast<size_t>(wave.frameCount) * static_cast<size_t>(wave.channels);
    out_pcm.assign(samples, samples + sample_count);
    out_channels = static_cast<unsigned int>(wave.channels);
    out_sample_rate = static_cast<unsigned int>(wave.sampleRate);
    UnloadWaveSamples(samples);
    UnloadWave(wave);
    return !out_pcm.empty();
}

[[nodiscard]] bool convert_to_device_pcm(const SDL_AudioSpec& spec,
                                         const std::vector<float>& decoded_pcm,
                                         unsigned int source_channels,
                                         unsigned int source_sample_rate,
                                         std::vector<std::uint8_t>& out_pcm) noexcept {
    if (decoded_pcm.empty()) return false;

    SDL_AudioStream* stream = SDL_NewAudioStream(AUDIO_F32SYS,
                                                 static_cast<Uint8>(source_channels),
                                                 static_cast<int>(source_sample_rate),
                                                 spec.format,
                                                 spec.channels,
                                                 spec.freq);
    if (!stream) return false;

    const int input_bytes = static_cast<int>(decoded_pcm.size() * sizeof(float));
    if (SDL_AudioStreamPut(stream, decoded_pcm.data(), input_bytes) != 0 ||
        SDL_AudioStreamFlush(stream) != 0) {
        SDL_FreeAudioStream(stream);
        return false;
    }

    const int available = SDL_AudioStreamAvailable(stream);
    if (available <= 0) {
        SDL_FreeAudioStream(stream);
        return false;
    }

    out_pcm.resize(static_cast<size_t>(available));
    const int copied = SDL_AudioStreamGet(stream, out_pcm.data(), available);
    SDL_FreeAudioStream(stream);
    if (copied <= 0) return false;
    out_pcm.resize(static_cast<size_t>(copied));
    return true;
}

void rebuild_playback_pcm(SdlMusicStreamState& stream, float volume, SDL_AudioFormat format) {
    stream.pcm_playback_bytes.resize(stream.pcm_base_bytes.size(), 0);
    if (stream.pcm_base_bytes.empty()) return;
    SDL_MixAudioFormat(stream.pcm_playback_bytes.data(),
                       stream.pcm_base_bytes.data(),
                       format,
                       static_cast<Uint32>(stream.pcm_base_bytes.size()),
                       static_cast<int>(clamp_volume(volume) * static_cast<float>(SDL_MIX_MAXVOLUME)));
}

void reset_loaded_stream(MusicContext& music) noexcept {
    auto& state = sdl_audio_state();
    if (state.owner != &music) return;
    if (state.device != 0) SDL_ClearQueuedAudio(state.device);
    state.owner = nullptr;
    state.stream = SdlMusicStreamState{};
    music.loaded = false;
    music.started = false;
}
#else
struct MusicTimeOverride {
    bool enabled = false;
    float seconds = 0.0f;
};

MusicTimeOverride& music_time_override() noexcept {
    static MusicTimeOverride state;
    return state;
}
#endif

}  // namespace

void init_audio_device() noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.device != 0) return;

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0u) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
        state.subsystem_started = true;
    }

    SDL_AudioSpec requested{};
    requested.freq = 48000;
    requested.format = AUDIO_F32SYS;
    requested.channels = 2;
    requested.samples = 2048;

    state.device = SDL_OpenAudioDevice(nullptr, 0, &requested, &state.spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (state.device == 0 && state.subsystem_started) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        state.subsystem_started = false;
    }
#else
    if (IsAudioDeviceReady()) return;
    InitAudioDevice();
#endif
}

void shutdown_audio_device() noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.device != 0) {
        SDL_CloseAudioDevice(state.device);
        state.device = 0;
    }
    state.owner = nullptr;
    state.stream = SdlMusicStreamState{};
    if (state.subsystem_started) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        state.subsystem_started = false;
    }
#else
    if (!IsAudioDeviceReady()) return;
    CloseAudioDevice();
#endif
}

[[nodiscard]] bool is_audio_device_ready() noexcept {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    return sdl_audio_state().device != 0;
#else
    return IsAudioDeviceReady();
#endif
}

[[nodiscard]] bool load_music_stream(MusicContext& music, const char* path, bool repeat) noexcept {
    if (!path || !is_audio_device_ready()) return false;

#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    reset_loaded_stream(music);

    std::vector<float> decoded_pcm;
    unsigned int source_channels = 0;
    unsigned int source_sample_rate = 0;
    if (!decode_audio_file(path, decoded_pcm, source_channels, source_sample_rate)) return false;

    std::vector<std::uint8_t> converted_pcm;
    if (!convert_to_device_pcm(state.spec, decoded_pcm, source_channels, source_sample_rate, converted_pcm)) {
        return false;
    }

    const float bytes_per_second = static_cast<float>(state.spec.freq * state.spec.channels) *
                                   static_cast<float>(SDL_AUDIO_BITSIZE(state.spec.format) / 8);
    if (bytes_per_second <= 0.0f || converted_pcm.empty()) return false;

    state.owner = &music;
    state.stream.pcm_base_bytes = std::move(converted_pcm);
    state.stream.bytes_per_second = bytes_per_second;
    state.stream.duration_seconds =
        static_cast<float>(state.stream.pcm_base_bytes.size()) / state.stream.bytes_per_second;
    state.stream.repeat = repeat;
    rebuild_playback_pcm(state.stream, music.volume, state.spec.format);
    SDL_ClearQueuedAudio(state.device);

    music.loaded = true;
    music.started = false;
    set_music_volume(music, music.volume);
    return true;
#else
    Music stream = LoadMusicStream(path);
    stream.looping = repeat;
    if (stream.frameCount == 0) return false;

    music.stream = stream;
    music.loaded = true;
    music.started = false;
    SetMusicVolume(music.stream, clamp_volume(music.volume));
    return true;
#endif
}

void unload_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    reset_loaded_stream(music);
#else
    if (music.started) {
        StopMusicStream(music.stream);
    }
    UnloadMusicStream(music.stream);
    music.loaded = false;
    music.started = false;
#endif
}

void set_music_volume(MusicContext& music, float volume) noexcept {
    music.volume = clamp_volume(volume);
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.device == 0) return;
    if (state.owner == &music && music.loaded && !music.started) {
        rebuild_playback_pcm(state.stream, music.volume, state.spec.format);
    }
#else
    if (!music.loaded) return;
    SetMusicVolume(music.stream, music.volume);
#endif
}

void update_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (music_time_override().enabled) return;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.owner != &music || state.device == 0) return;
    if (SDL_GetQueuedAudioSize(state.device) != 0) return;
    if (state.stream.repeat && !state.stream.pcm_playback_bytes.empty()) {
        SDL_QueueAudio(state.device,
                       state.stream.pcm_playback_bytes.data(),
                       static_cast<Uint32>(state.stream.pcm_playback_bytes.size()));
        SDL_PauseAudioDevice(state.device, 0);
        return;
    }
    music.started = false;
#else
    UpdateMusicStream(music.stream);
#endif
}

void play_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
    if (music_time_override().enabled) {
        music.started = true;
        return;
    }
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.owner != &music || state.device == 0 || state.stream.pcm_playback_bytes.empty()) return;
    SDL_ClearQueuedAudio(state.device);
    SDL_QueueAudio(state.device,
                   state.stream.pcm_playback_bytes.data(),
                   static_cast<Uint32>(state.stream.pcm_playback_bytes.size()));
    SDL_PauseAudioDevice(state.device, 0);
    music.started = true;
#else
    PlayMusicStream(music.stream);
    music.started = true;
#endif
}

void pause_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.owner != &music || state.device == 0) return;
    SDL_PauseAudioDevice(state.device, 1);
#else
    PauseMusicStream(music.stream);
#endif
}

void resume_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.owner != &music || state.device == 0) return;
    SDL_PauseAudioDevice(state.device, 0);
#else
    ResumeMusicStream(music.stream);
#endif
}

void stop_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    if (music_time_override().enabled) {
        music.started = false;
        return;
    }
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    auto& state = sdl_audio_state();
    if (state.owner == &music && state.device != 0) {
        SDL_ClearQueuedAudio(state.device);
        SDL_PauseAudioDevice(state.device, 1);
    }
    music.started = false;
#else
    StopMusicStream(music.stream);
    music.started = false;
#endif
}

[[nodiscard]] float get_music_time_played(const MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return 0.0f;
    const auto& override = music_time_override();
    if (override.enabled) return std::max(0.0f, override.seconds);
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    const auto& state = sdl_audio_state();
    if (state.owner != &music || state.device == 0 || state.stream.bytes_per_second <= 0.0f) return 0.0f;
    const float queued_bytes = static_cast<float>(SDL_GetQueuedAudioSize(state.device));
    const float played_bytes = std::clamp(static_cast<float>(state.stream.pcm_playback_bytes.size()) - queued_bytes,
                                          0.0f,
                                          static_cast<float>(state.stream.pcm_playback_bytes.size()));
    return played_bytes / state.stream.bytes_per_second;
#else
    return GetMusicTimePlayed(music.stream);
#endif
}

void set_music_time_played_override(float seconds) noexcept {
    auto& override = music_time_override();
    override.enabled = true;
    override.seconds = seconds;
}

void clear_music_time_played_override() noexcept {
    auto& override = music_time_override();
    override.enabled = false;
    override.seconds = 0.0f;
}

}  // namespace platform::audio
