#pragma once

#include <raylib.h>

// Audio runtime resource owner: holds the raylib Music handle for the
// currently-loaded song. Lives in registry context (not on an entity)
// and is read/written 1–2× per frame by song_playback_system only.
//
// Lives beside its owning system (not under `app/components/`) because this
// is a ctx-singleton RAII wrapper, not entity-owned plain data —
// `app/components/` stays reserved for atomic, queryable entity-owned tables
// per .squad/decisions.md §"app/components parallel audit verdict (consolidated)".
//
// Per Fabian Principle 3 (.squad/decisions.md § 9 — no NULL columns) the
// former `loaded` / `started` / `paused` parallel-bool NULL-column gates
// over `stream` were eradicated in issue #1618:
//   - `loaded`  → presence of `MusicContext` ctx singleton IS "stream loaded"
//   - `started` → presence of `MusicPlayingTag` ctx tag IS "PlayMusicStream() in flight"
//   - `paused`  → presence of `MusicPausedTag`  ctx tag IS "PauseMusicStream() in flight"
// This mirrors the precedent set by PR #1617 (`SFXBank::loaded`) and
// PRs in this cycle (#1621 `TextContext`, #1622 `TestPlayerState::active`).

inline bool music_stream_is_playable(const Music& music) {
    return IsMusicValid(music) && music.stream.buffer != nullptr;
}

struct MusicContext {
    Music stream{};        // raylib Music handle (zero-initialized)
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
}

inline MusicContext::~MusicContext() { release(); }

inline MusicContext::MusicContext(MusicContext&& other) noexcept
    : stream{other.stream},
      volume{other.volume}
{
    other.stream = {};
}

inline MusicContext& MusicContext::operator=(MusicContext&& other) noexcept {
    if (this != &other) {
        release();
        stream = other.stream;
        volume = other.volume;
        other.stream = {};
    }
    return *this;
}
