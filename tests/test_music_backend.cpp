#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "audio/music_context.h"
#include "platform/audio/music_backend.h"

namespace {

struct MusicTimeOverrideGuard {
    ~MusicTimeOverrideGuard() {
        platform::audio::clear_music_time_played_override();
    }
};

}  // namespace

TEST_CASE("music_backend: unloaded stream operations are no-ops", "[audio][music_backend]") {
    MusicContext music{};

    platform::audio::update_music_stream(music);
    platform::audio::play_music_stream(music);
    platform::audio::pause_music_stream(music);
    platform::audio::resume_music_stream(music);
    platform::audio::stop_music_stream(music);

    CHECK_FALSE(music.loaded);
    CHECK_FALSE(music.started);
    CHECK_THAT(platform::audio::get_music_time_played(music),
               Catch::Matchers::WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("music_backend: volume is clamped for context stability", "[audio][music_backend]") {
    MusicContext music{};

    platform::audio::set_music_volume(music, 1.5f);
    CHECK_THAT(music.volume, Catch::Matchers::WithinAbs(1.0f, 0.0001f));

    platform::audio::set_music_volume(music, -0.2f);
    CHECK_THAT(music.volume, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("music_backend: missing file load fails cleanly", "[audio][music_backend]") {
    MusicContext music{};
    const bool loaded = platform::audio::load_music_stream(music, "content/audio/__missing__.flac", false);
    CHECK_FALSE(loaded);
    CHECK_FALSE(music.loaded);
    CHECK_FALSE(music.started);
}

TEST_CASE("music_backend: time override enables deterministic sync validation",
          "[audio][music_backend][timing]") {
    MusicTimeOverrideGuard guard;
    MusicContext music{};
    music.loaded = true;
    music.started = true;

    platform::audio::set_music_time_played_override(12.5f);

    CHECK_THAT(platform::audio::get_music_time_played(music),
               Catch::Matchers::WithinAbs(12.5f, 0.0001f));
}
