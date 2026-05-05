#pragma once

#include "music_context.h"
#include "audio_runtime_state.h"

namespace platform::audio {

[[nodiscard]] bool init_audio_device(AudioDeviceRuntimeState& audio_device) noexcept;
void shutdown_audio_device(AudioDeviceRuntimeState& audio_device) noexcept;
[[nodiscard]] bool is_audio_device_ready(const AudioDeviceRuntimeState& audio_device) noexcept;

[[nodiscard]] bool load_music_stream(MusicContext& music, const char* path, bool repeat) noexcept;
void unload_music_stream(MusicContext& music) noexcept;
void set_music_volume(MusicContext& music, float volume) noexcept;
void update_music_stream(MusicContext& music) noexcept;
void play_music_stream(MusicContext& music) noexcept;
void pause_music_stream(MusicContext& music) noexcept;
void resume_music_stream(MusicContext& music) noexcept;
void stop_music_stream(MusicContext& music) noexcept;
[[nodiscard]] bool is_music_playing(const MusicContext& music) noexcept;
[[nodiscard]] float get_music_time_played(const MusicContext& music) noexcept;
void set_music_time_played_override(MusicContext& music, float seconds) noexcept;
void clear_music_time_played_override(MusicContext& music) noexcept;

}  // namespace platform::audio
