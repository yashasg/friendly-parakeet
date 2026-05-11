#pragma once

#include <raylib.h>

// Singleton: holds the raylib Music handle and playback state.
// Cold data — read/written 1-2x per frame by song_playback_system only.
struct MusicContext {
    Music stream{};        // raylib Music handle (zero-initialized)
    bool  loaded  = false; // LoadMusicStream succeeded
    bool  started = false; // PlayMusicStream has been called
    bool  paused  = false; // PauseMusicStream called; consumed by one-shot resume
    float volume  = 0.8f;  // master volume [0.0, 1.0]
};

inline bool music_stream_is_valid(Music stream) {
    return IsMusicValid(stream);
}

inline bool music_stream_may_own_resources(const Music& stream) {
    return stream.ctxData != nullptr || stream.stream.buffer != nullptr;
}

inline void unload_music_stream_resources(Music& stream) {
    if (music_stream_may_own_resources(stream)) {
        UnloadMusicStream(stream);
    }
    stream = Music{};
}
