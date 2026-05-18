#pragma once

#include <raylib.h>

// Audio runtime resource owner: holds the raylib Music handle and playback
// state for the active song. Lives in registry context (not on an entity)
// and is read/written 1–2× per frame by song_playback_system only.
//
// Lives beside its owning system (not under `app/components/`) because this
// is a ctx-singleton RAII wrapper, not entity-owned plain data —
// `app/components/` stays reserved for atomic, queryable entity-owned tables
// per .squad/decisions.md §"app/components parallel audit verdict (consolidated)".

inline bool music_stream_is_playable(const Music& music) {
    return IsMusicValid(music) && music.stream.buffer != nullptr;
}

struct MusicContext {
    Music stream{};        // raylib Music handle (zero-initialized)
    bool  loaded  = false; // LoadMusicStream succeeded
    bool  started = false; // PlayMusicStream has been called
    bool  paused  = false; // PauseMusicStream called; consumed by one-shot resume
    float volume  = 0.8f;  // master volume [0.0, 1.0]

    MusicContext() = default;
    MusicContext(const MusicContext&) = delete;
    MusicContext& operator=(const MusicContext&) = delete;
    MusicContext(MusicContext&& other) noexcept;
    MusicContext& operator=(MusicContext&& other) noexcept;
    ~MusicContext();

    void release();
};

inline bool music_stream_may_own_resources(const Music& stream) {
    return stream.ctxData != nullptr || stream.stream.buffer != nullptr;
}

inline void unload_music_stream_resources(Music& stream) {
    if (music_stream_may_own_resources(stream)) {
        if (IsMusicValid(stream)) {
            StopMusicStream(stream);
        }
        UnloadMusicStream(stream);
    }
    stream = Music{};
}

inline void MusicContext::release() {
    unload_music_stream_resources(stream);
    loaded = false;
    started = false;
    paused = false;
}

inline MusicContext::~MusicContext() { release(); }

inline MusicContext::MusicContext(MusicContext&& other) noexcept
    : stream{other.stream},
      loaded{other.loaded},
      started{other.started},
      paused{other.paused},
      volume{other.volume}
{
    other.stream = {};
    other.loaded = false;
    other.started = false;
    other.paused = false;
}

inline MusicContext& MusicContext::operator=(MusicContext&& other) noexcept {
    if (this != &other) {
        release();
        stream = other.stream;
        loaded = other.loaded;
        started = other.started;
        paused = other.paused;
        volume = other.volume;
        other.stream = {};
        other.loaded = false;
        other.started = false;
        other.paused = false;
    }
    return *this;
}
