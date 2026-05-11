#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "audio/music_context.h"

namespace {

Music make_nominal_music_without_buffer() {
    Music music{};
    music.ctxData = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1));
    music.frameCount = 1024;
    music.stream.sampleRate = 44100;
    music.stream.sampleSize = 16;
    music.stream.channels = 2;
    return music;
}

} // namespace

TEST_CASE("music context: playable stream requires raylib validity and an audio buffer", "[audio]") {
    Music music = make_nominal_music_without_buffer();

    CHECK(IsMusicValid(music));
    CHECK_FALSE(music_stream_is_playable(music));

    music.stream.buffer = reinterpret_cast<decltype(music.stream.buffer)>(static_cast<std::uintptr_t>(0x1));

    CHECK(music_stream_is_playable(music));
}

TEST_CASE("music context: rejected partial streams are recognized for unload", "[audio]") {
    Music music{};
    CHECK_FALSE(music_stream_has_unloadable_resources(music));

    music.ctxData = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1));
    CHECK(music_stream_has_unloadable_resources(music));

    music.ctxData = nullptr;
    music.stream.buffer = reinterpret_cast<decltype(music.stream.buffer)>(static_cast<std::uintptr_t>(0x1));
    CHECK(music_stream_has_unloadable_resources(music));
}
