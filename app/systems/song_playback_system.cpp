#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/music.h"
#include "../entities/beat_map.h"
#include "../entities/settings.h"
#include <raylib.h>

void song_playback_system(entt::registry& reg, float dt) {
    auto& ctx = reg.ctx();

    auto* song  = ctx.find<SongState>();
    auto* map   = find_beat_map(reg);
    auto* music = ctx.find<MusicContext>();
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
    // Per Fabian's existential processing (issue #1202/#1204, PR F), each
    // former `gs.phase == GamePhase::X` branch dispatches on the per-phase
    // ctx-tag mirror. The mirror invariant (`tests/test_game_phase_tags.cpp`)
    // guarantees exactly one `GamePhase*Tag` is present at a time, so these
    // branches stay mutually exclusive without an `else` chain.
    const bool playing_phase       = ctx.contains<GamePhasePlayingTag>();
    const bool paused_phase        = ctx.contains<GamePhasePausedTag>();
    const bool game_over_phase     = ctx.contains<GamePhaseGameOverTag>();
    const bool song_complete_phase = ctx.contains<GamePhaseSongCompleteTag>();

    if (music_loaded && playing_phase && song && song->playing) {
        if (!music->started) {
            PlayMusicStream(music->stream);
            music->started = true;
            music->paused = false;
        } else if (music->paused) {
            ResumeMusicStream(music->stream);
            music->paused = false;
        }
    } else if (music_loaded && paused_phase && music->started) {
        if (!music->paused) {
            PauseMusicStream(music->stream);
            music->paused = true;
        }
    } else if (music_loaded &&
               (game_over_phase || song_complete_phase) &&
               music->started) {
        StopMusicStream(music->stream);
        music->started = false;
        music->paused = false;
    }

    // ── Song time / beat advancement (Playing phase only) ─────
    if (!playing_phase) return;
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

    const auto* settings = find_settings_state(reg);
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
