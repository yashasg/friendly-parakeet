#pragma once

#include <entt/entt.hpp>
#include "components/game_state.h"

[[nodiscard]] constexpr bool game_render_should_draw_world_meshes(GamePhase phase) noexcept {
    switch (phase) {
        case GamePhase::Playing:
        case GamePhase::Paused:
        case GamePhase::GameOver:
        case GamePhase::SongComplete:
            return true;
        case GamePhase::Title:
        case GamePhase::LevelSelect:
        case GamePhase::Settings:
        case GamePhase::Tutorial:
            return false;
    }
    return false;
}

// Runtime-only systems require a live platform/audio/render context and are
// intentionally excluded from shapeshifter_lib.

// Phase 0: Raw input (polls runtime input backend)
void input_system_init(entt::registry& reg);
void input_system(entt::registry& reg, float raw_dt);

// Phase 3: Runtime song playback backend
void song_playback_system(entt::registry& reg, float dt);

// Fixed-step tick used by runtime/integration paths.
void tick_fixed_systems(entt::registry& reg, float dt);

// Phase 7: Camera
void game_camera_system(entt::registry& reg, float dt);
void ui_camera_system(entt::registry& reg, float dt);

// Phase 8: Render
void floor_render_system(const entt::registry& reg);
void game_render_system(const entt::registry& reg, float alpha);
void ui_render_system(entt::registry& reg, float alpha);

// Runtime audio dispatch/drain
void audio_system(entt::registry& reg);
