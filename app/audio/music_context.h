#pragma once

#include <raylib.h>

inline bool music_stream_has_unloadable_resources(const Music& music) {
    return music.ctxData != nullptr || music.stream.buffer != nullptr;
}

inline bool music_stream_is_playable(const Music& music) {
    return IsMusicValid(music) && music.stream.buffer != nullptr;
}

// Singleton: holds the raylib Music handle and playback state.
// Cold data — read/written 1-2x per frame by song_playback_system only.
struct MusicContext {
    Music stream{};        // raylib Music handle (zero-initialized)
    bool  loaded  = false; // LoadMusicStream succeeded
    bool  started = false; // PlayMusicStream has been called
    bool  paused  = false; // PauseMusicStream called; consumed by one-shot resume
    float volume  = 0.8f;  // master volume [0.0, 1.0]
};
