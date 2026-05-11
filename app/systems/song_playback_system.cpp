#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../audio/music_context.h"
#include "../util/settings_persistence.h"
#include <raylib.h>

void song_playback_system(entt::registry& reg, float dt) {
    const auto& gs = reg.ctx().get<GameState>();

    auto* song  = reg.ctx().find<SongState>();
    auto* map   = reg.ctx().find<BeatMap>();
    auto* music = reg.ctx().find<MusicContext>();
    const bool music_loaded = music && music->loaded;

    // Must pump the stream buffer every frame to prevent audio underruns,
    // even when the game is paused.
    if (music_loaded && music->started) {
        UpdateMusicStream(music->stream);
    }

    // ── Music restart request (from enter_playing) ────────────
    if (song && song->restart_music) {
        song->restart_music = false;
        if (music_loaded) {
            StopMusicStream(music->stream);
            PlayMusicStream(music->stream);
            music->started = true;
            music->paused = false;
        }
    }

    // ── Music lifecycle: start / pause / resume / stop ────────
    if (music_loaded && gs.phase == GamePhase::Playing && song && song->playing) {
        if (!music->started) {
            PlayMusicStream(music->stream);
            music->started = true;
            music->paused = false;
        } else if (music->paused) {
            ResumeMusicStream(music->stream);
            music->paused = false;
        }
    } else if (music_loaded && gs.phase == GamePhase::Paused && music->started) {
        if (!music->paused) {
            PauseMusicStream(music->stream);
            music->paused = true;
        }
    } else if (music_loaded &&
               (gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete) &&
               music->started) {
        StopMusicStream(music->stream);
        music->started = false;
        music->paused = false;
    }

    // ── Song time / beat advancement (Playing phase only) ─────
    if (gs.phase != GamePhase::Playing) return;
    if (!song) return;

    if (song->finished) {
        // Keep song_time advancing after playback ends so remaining spawned
        // obstacles can continue scrolling to resolution/despawn.
        song->song_time += dt;
        return;
    }
    if (!song->playing) return;

    // Authoritative clock from audio stream; fall back to dt accumulation
    // when running in silent/test mode (no music loaded).
    if (music_loaded && music->started) {
        song->song_time = GetMusicTimePlayed(music->stream);
    } else {
        song->song_time += dt;
    }

    const auto* settings = reg.ctx().find<SettingsState>();
    const float audio_offset_sec = settings ? settings::audio_offset_seconds(*settings) : 0.0f;

    // Current beat (non-decreasing)
    if (map && !map->beat_times.empty()) {
        while ((song->current_beat + 1) >= 0 &&
               static_cast<size_t>(song->current_beat + 1) < map->beat_times.size() &&
               song->song_time >=
                   map->beat_times[static_cast<size_t>(song->current_beat + 1)] + audio_offset_sec) {
            ++song->current_beat;
        }
    } else if (song->beat_period > 0.0f && song->song_time >= song->offset + audio_offset_sec) {
        int beat = static_cast<int>(
            (song->song_time - (song->offset + audio_offset_sec)) / song->beat_period);
        if (beat > song->current_beat) {
            song->current_beat = beat;
        }
    }

    // Song end detection
    if (song->song_time >= song->duration_sec) {
        song->finished = true;
        song->playing  = false;
        if (music_loaded && music->started) {
            StopMusicStream(music->stream);
            music->started = false;
            music->paused = false;
        }
    }
}
