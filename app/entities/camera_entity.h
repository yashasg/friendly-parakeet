#pragma once
#include <raylib.h>
#include <entt/entt.hpp>
#include <stdexcept>

// Per-entity data components attached to the game and UI camera entities.
struct GameCamera { Camera3D cam; };
struct UICamera   { Camera2D cam; };

// Create the singleton game camera entity (3D perspective, fixed position).
// Throws std::logic_error if the registry already has one.
void spawn_game_camera(entt::registry& reg);

// Create the singleton UI camera entity (2D, identity / screen-space).
// Throws std::logic_error if the registry already has one.
void spawn_ui_camera(entt::registry& reg);

// Accessors — safe after spawn_game_camera / spawn_ui_camera are called.
inline GameCamera& game_camera(entt::registry& reg) {
    auto view = reg.view<GameCamera>();
    auto it = view.begin();
    if (it == view.end()) {
        throw std::logic_error("GameCamera entity is missing; call spawn_game_camera() first");
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple GameCamera entities exist");
    }
    return view.get<GameCamera>(entity);
}

inline UICamera& ui_camera(entt::registry& reg) {
    auto view = reg.view<UICamera>();
    auto it = view.begin();
    if (it == view.end()) {
        throw std::logic_error("UICamera entity is missing; call spawn_ui_camera() first");
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple UICamera entities exist");
    }
    return view.get<UICamera>(entity);
}

inline const GameCamera& game_camera(const entt::registry& reg) {
    auto view = reg.view<const GameCamera>();
    auto it = view.begin();
    if (it == view.end()) {
        throw std::logic_error("GameCamera entity is missing; call spawn_game_camera() first");
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple GameCamera entities exist");
    }
    return view.get<const GameCamera>(entity);
}

inline const UICamera& ui_camera(const entt::registry& reg) {
    auto view = reg.view<const UICamera>();
    auto it = view.begin();
    if (it == view.end()) {
        throw std::logic_error("UICamera entity is missing; call spawn_ui_camera() first");
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple UICamera entities exist");
    }
    return view.get<const UICamera>(entity);
}
