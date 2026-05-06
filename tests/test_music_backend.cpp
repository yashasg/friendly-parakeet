#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "components/audio.h"
#include "systems/audio_runtime.h"

namespace {

struct MusicTimeOverrideGuard {
    MusicContext& music;
    ~MusicTimeOverrideGuard() {
        audio_runtime::clear_music_time_played_override(music);
    }
};

}  // namespace

TEST_CASE("music_backend: unloaded stream operations are no-ops", "[audio][music_backend]") {
    MusicContext music{};

    audio_runtime::update_music_stream(music);
    audio_runtime::play_music_stream(music);
    audio_runtime::pause_music_stream(music);
    audio_runtime::resume_music_stream(music);
    audio_runtime::stop_music_stream(music);

    CHECK_FALSE(music.loaded);
    CHECK_FALSE(music.started);
    CHECK_THAT(audio_runtime::get_music_time_played(music),
               Catch::Matchers::WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("music_backend: volume is clamped for context stability", "[audio][music_backend]") {
    MusicContext music{};

    audio_runtime::set_music_volume(music, 1.5f);
    CHECK_THAT(music.volume, Catch::Matchers::WithinAbs(1.0f, 0.0001f));

    audio_runtime::set_music_volume(music, -0.2f);
    CHECK_THAT(music.volume, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("music_backend: missing file load fails cleanly", "[audio][music_backend]") {
    MusicContext music{};
    const bool loaded = audio_runtime::load_music_stream(music, "content/audio/__missing__.flac", false);
    CHECK_FALSE(loaded);
    CHECK_FALSE(music.loaded);
    CHECK_FALSE(music.started);
}

TEST_CASE("music_backend: time override enables deterministic sync validation",
          "[audio][music_backend][timing]") {
    MusicContext music{};
    MusicTimeOverrideGuard guard{music};
    music.loaded = true;
    music.started = true;

    audio_runtime::set_music_time_played_override(music, 12.5f);

    CHECK_THAT(audio_runtime::get_music_time_played(music),
               Catch::Matchers::WithinAbs(12.5f, 0.0001f));
}

TEST_CASE("music_backend: playback confirmation follows override playback state",
          "[audio][music_backend][timing]") {
    MusicContext music{};
    MusicTimeOverrideGuard guard{music};
    music.loaded = true;

    CHECK_FALSE(audio_runtime::is_music_playing(music));

    audio_runtime::set_music_time_played_override(music, 1.0f);
    audio_runtime::play_music_stream(music);
    CHECK(audio_runtime::is_music_playing(music));

    audio_runtime::pause_music_stream(music);
    CHECK_FALSE(audio_runtime::is_music_playing(music));
}
