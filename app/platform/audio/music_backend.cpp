#include "music_backend.h"

#include <raylib.h>
#include <algorithm>

namespace platform::audio {

namespace {

float clamp_volume(float volume) noexcept {
    return std::clamp(volume, 0.0f, 1.0f);
}

}  // namespace

void init_audio_device() noexcept {
    if (IsAudioDeviceReady()) return;
    InitAudioDevice();
}

void shutdown_audio_device() noexcept {
    if (!IsAudioDeviceReady()) return;
    CloseAudioDevice();
}

[[nodiscard]] bool is_audio_device_ready() noexcept {
    return IsAudioDeviceReady();
}

[[nodiscard]] bool load_music_stream(MusicContext& music, const char* path, bool repeat) noexcept {
    if (!path || !is_audio_device_ready()) return false;

    Music stream = LoadMusicStream(path);
    stream.looping = repeat;
    if (stream.frameCount == 0) return false;

    music.stream = stream;
    music.loaded = true;
    music.started = false;
    SetMusicVolume(music.stream, clamp_volume(music.volume));
    return true;
}

void unload_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
    if (music.started) {
        StopMusicStream(music.stream);
    }
    UnloadMusicStream(music.stream);
    music.loaded = false;
    music.started = false;
}

void set_music_volume(MusicContext& music, float volume) noexcept {
    music.volume = clamp_volume(volume);
    if (!music.loaded) return;
    SetMusicVolume(music.stream, music.volume);
}

void update_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    UpdateMusicStream(music.stream);
}

void play_music_stream(MusicContext& music) noexcept {
    if (!music.loaded) return;
    PlayMusicStream(music.stream);
    music.started = true;
}

void pause_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    PauseMusicStream(music.stream);
}

void resume_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    ResumeMusicStream(music.stream);
}

void stop_music_stream(MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return;
    StopMusicStream(music.stream);
    music.started = false;
}

[[nodiscard]] float get_music_time_played(const MusicContext& music) noexcept {
    if (!music.loaded || !music.started) return 0.0f;
    return GetMusicTimePlayed(music.stream);
}

}  // namespace platform::audio
