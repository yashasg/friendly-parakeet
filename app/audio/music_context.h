#pragma once

#include "platform/runtime_api.h"

// Singleton: holds the runtime Music handle and playback state.
// Cold data — read/written 1-2x per frame by song_playback_system only.
struct MusicContext {
    Music stream{};        // runtime Music handle (zero-initialized)
    bool  loaded  = false; // LoadMusicStream succeeded
    bool  started = false; // PlayMusicStream has been called
    float volume  = 0.8f;  // master volume [0.0, 1.0]
};
