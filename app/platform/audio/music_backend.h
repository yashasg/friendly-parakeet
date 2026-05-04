#pragma once

#include "../../audio/music_context.h"

namespace platform::audio {

void init_audio_device() noexcept;
void shutdown_audio_device() noexcept;
[[nodiscard]] bool is_audio_device_ready() noexcept;

[[nodiscard]] bool load_music_stream(MusicContext& music, const char* path, bool repeat) noexcept;
void unload_music_stream(MusicContext& music) noexcept;
void set_music_volume(MusicContext& music, float volume) noexcept;
void update_music_stream(MusicContext& music) noexcept;
void play_music_stream(MusicContext& music) noexcept;
void pause_music_stream(MusicContext& music) noexcept;
void resume_music_stream(MusicContext& music) noexcept;
void stop_music_stream(MusicContext& music) noexcept;
[[nodiscard]] float get_music_time_played(const MusicContext& music) noexcept;
void set_music_time_played_override(float seconds) noexcept;
void clear_music_time_played_override() noexcept;

}  // namespace platform::audio
