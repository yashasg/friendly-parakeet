#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/registry_context.h"
#include "../components/rhythm.h"
#include "../components/audio.h"
#include "../systems/audio_runtime.h"

void song_playback_system(entt::registry& reg, float dt) {
    const auto& gs = registry_ctx_get<GameState>(reg);

    auto* song = registry_ctx_find<SongState>(reg);
    auto* map = registry_ctx_find<BeatMap>(reg);
    auto* music = registry_ctx_find<MusicContext>(reg);
    const bool music_loaded = music && music->loaded;

    // Must pump the stream buffer every frame to prevent audio underruns,
    // even when the game is paused.
    if (music_loaded && music->started) {
        audio_runtime::update_music_stream(*music);
    }

    // ── Music restart request (from enter_playing) ────────────
    if (song && song->restart_music) {
        song->restart_music = false;
        if (music_loaded) {
            audio_runtime::stop_music_stream(*music);
            audio_runtime::play_music_stream(*music);
        }
    }

    // ── Music lifecycle: start / pause / resume / stop ────────
    if (music_loaded && is_playing_phase(gs.phase) && song && song->playing) {
        if (!music->started) {
            audio_runtime::play_music_stream(*music);
        } else if (gs.previous_phase == GamePhase::Paused) {
            audio_runtime::resume_music_stream(*music);
        }
    } else if (music_loaded && gs.phase == GamePhase::Paused && music->started) {
        audio_runtime::pause_music_stream(*music);
    } else if (music_loaded &&
               (gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete) &&
               music->started) {
        audio_runtime::stop_music_stream(*music);
    }

    // ── Song time / beat advancement (Playing phase only) ─────
    if (!is_playing_phase(gs.phase)) return;
    if (!song || !song->playing || song->finished) return;

    // Authoritative clock from audio stream; fall back to dt accumulation
    // when running in silent/test mode (no music loaded).
    if (music_loaded && music->started) {
        song->song_time = audio_runtime::get_music_time_played(*music);
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
        if (song->current_beat >= 0 &&
            static_cast<size_t>(song->current_beat) < map->beat_times.size()) {
            song->current_beat_time = map->beat_times[static_cast<size_t>(song->current_beat)];
        } else {
            song->current_beat_time = song->offset;
        }
    } else if (song->beat_period > 0.0f && song->song_time >= song->offset) {
        int beat = static_cast<int>((song->song_time - song->offset) / song->beat_period);
        if (beat > song->current_beat) {
            song->current_beat = beat;
        }
        song->current_beat_time = song->offset +
                                  static_cast<float>(song->current_beat) * song->beat_period;
    } else {
        song->current_beat_time = song->offset;
    }

    // Song end detection
    if (song->song_time >= song->duration_sec) {
        song->finished = true;
        song->playing  = false;
        if (music_loaded && music->started) {
            audio_runtime::stop_music_stream(*music);
        }
    }
}
