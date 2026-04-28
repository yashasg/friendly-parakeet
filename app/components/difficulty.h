#pragma once

// ── DifficultyConfig (singleton, lives in registry context) ─────────────────
// Emplaced in game_loop.cpp; reset by setup_play_session() (play_session.cpp)
// at the start of each session.
//
// Not used in rhythm mode — difficulty_system skips updates when SongState is
// playing (scroll_speed is BPM-derived from SongState in that mode).
struct DifficultyConfig {
    // ── Session-init fields (set by setup_play_session) ─────────────────────
    float speed_multiplier     = 1.0f;   // starts at 1; ramped per-frame by difficulty_system
    float scroll_speed         = 400.0f; // pixels/s; ramped per-frame by difficulty_system
    float spawn_interval       = 1.8f;   // seconds between spawns; ramped by difficulty_system
    float spawn_timer          = 1.0f;   // countdown to next spawn; decremented by obstacle_spawn_system
    float elapsed              = 0.0f;   // total session time; incremented per-frame by difficulty_system
};
