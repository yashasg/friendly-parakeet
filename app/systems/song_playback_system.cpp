#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../audio/music_context.h"
#include "../platform/audio/music_backend.h"

void song_playback_system(entt::registry& reg, float dt) {
    const auto& gs = reg.ctx().get<GameState>();

    auto* song  = reg.ctx().find<SongState>();
    auto* map   = reg.ctx().find<BeatMap>();
    auto* music = reg.ctx().find<MusicContext>();
    const bool music_loaded = music && music->loaded;

    // Must pump the stream buffer every frame to prevent audio underruns,
    // even when the game is paused.
    if (music_loaded && music->started) {
        platform::audio::update_music_stream(*music);
    }

    // ── Music restart request (from enter_playing) ────────────
    if (song && song->restart_music) {
        song->restart_music = false;
        if (music_loaded) {
            platform::audio::stop_music_stream(*music);
            platform::audio::play_music_stream(*music);
        }
    }

    // ── Music lifecycle: start / pause / resume / stop ────────
    if (music_loaded && gs.phase == GamePhase::Playing && song && song->playing) {
        if (!music->started) {
            platform::audio::play_music_stream(*music);
        } else if (gs.previous_phase == GamePhase::Paused) {
            platform::audio::resume_music_stream(*music);
        }
    } else if (music_loaded && gs.phase == GamePhase::Paused && music->started) {
        platform::audio::pause_music_stream(*music);
    } else if (music_loaded &&
               (gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete) &&
               music->started) {
        platform::audio::stop_music_stream(*music);
    }

    // ── Song time / beat advancement (Playing phase only) ─────
    if (gs.phase != GamePhase::Playing) return;
    if (!song || !song->playing || song->finished) return;

    // Authoritative clock from audio stream; fall back to dt accumulation
    // when running in silent/test mode (no music loaded).
    if (music_loaded && music->started) {
        song->song_time = platform::audio::get_music_time_played(*music);
    } else {
        song->song_time += dt;
    }

    // Current beat (non-decreasing)
    if (map && !map->beat_times.empty()) {
        while ((song->current_beat + 1) >= 0 &&
               static_cast<size_t>(song->current_beat + 1) < map->beat_times.size() &&
               song->song_time >= map->beat_times[static_cast<size_t>(song->current_beat + 1)]) {
            ++song->current_beat;
        }
    } else if (song->beat_period > 0.0f && song->song_time >= song->offset) {
        int beat = static_cast<int>((song->song_time - song->offset) / song->beat_period);
        if (beat > song->current_beat) {
            song->current_beat = beat;
        }
    }

    // Song end detection
    if (song->song_time >= song->duration_sec) {
        song->finished = true;
        song->playing  = false;
        if (music_loaded && music->started) {
            platform::audio::stop_music_stream(*music);
        }
    }
}
