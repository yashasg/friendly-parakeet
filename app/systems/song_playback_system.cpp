#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/music.h"
#include "../session_logger.h"
#include <raylib.h>

void song_playback_system(entt::registry& reg, float dt) {
    const auto& gs = reg.ctx().get<GameState>();

    auto* song  = reg.ctx().find<SongState>();
    auto* music = reg.ctx().find<MusicContext>();

    // Must pump the stream buffer every frame to prevent audio underruns,
    // even when the game is paused.
    if (music && music->loaded && music->started) {
        UpdateMusicStream(music->stream);
    }

    // ── Music restart request (from enter_playing) ────────────
    if (song && song->restart_music) {
        song->restart_music = false;
        if (music && music->loaded) {
            StopMusicStream(music->stream);
            PlayMusicStream(music->stream);
            music->started = true;
        }
    }

    // ── Music lifecycle: start / pause / resume / stop ────────
    if (music && music->loaded) {
        if (gs.phase == GamePhase::Playing && song && song->playing) {
            if (!music->started) {
                PlayMusicStream(music->stream);
                music->started = true;
            } else if (gs.previous_phase == GamePhase::Paused) {
                ResumeMusicStream(music->stream);
            }
        } else if (gs.phase == GamePhase::Paused && music->started) {
            PauseMusicStream(music->stream);
        } else if ((gs.phase == GamePhase::GameOver || gs.phase == GamePhase::SongComplete)
                   && music->started) {
            StopMusicStream(music->stream);
            music->started = false;
        }
    }

    // ── Song time / beat advancement (Playing phase only) ─────
    if (gs.phase != GamePhase::Playing) return;
    if (!song || !song->playing || song->finished) return;

    // Authoritative clock from audio stream; fall back to dt accumulation
    // when running in silent/test mode (no music loaded).
    if (music && music->loaded && music->started) {
        song->song_time = GetMusicTimePlayed(music->stream);
    } else {
        song->song_time += dt;
    }

    // Current beat (non-decreasing)
    if (song->beat_period > 0.0f && song->song_time >= song->offset) {
        int beat = static_cast<int>((song->song_time - song->offset) / song->beat_period);
        if (beat > song->current_beat) {
            song->current_beat = beat;

            auto* log = reg.ctx().find<SessionLog>();
            if (log && log->file) {
                session_log_write(*log, song->song_time, "GAME",
                    "BEAT %d", beat);
            }
        }
    }

    // Song end detection
    if (song->song_time >= song->duration_sec) {
        song->finished = true;
        song->playing  = false;
        if (music && music->loaded && music->started) {
            StopMusicStream(music->stream);
            music->started = false;
        }
    }
}
