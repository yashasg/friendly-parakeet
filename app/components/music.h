#pragma once

#include <raylib.h>

// Singleton: holds the raylib Music handle and playback state.
// Cold data — read/written 1-2x per frame by song_playback_system only.
struct MusicContext {
    Music stream{};        // raylib Music handle (zero-initialized)
    bool  loaded  = false; // LoadMusicStream succeeded
    bool  started = false; // PlayMusicStream has been called
    float volume  = 0.8f;  // master volume [0.0, 1.0]
};
