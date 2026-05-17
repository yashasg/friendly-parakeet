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
//
// All four accessor bodies (game/ui × mutable/const) follow the same
// view-iterate-validate-return contract; the only per-camera differences
// are the component type and the two error-message string literals naming
// the component + the spawner. Per the byte-identical fold pattern the
// squad has already merged in #1430 / #1436 / #1440 / #1443 / #1448 (and
// #1452 for this header), the public per-camera symbols remain (callers
// in `camera_system.cpp`, `game_render_system.cpp`, `ui_render_system.cpp`,
// and `tests/test_camera_entity_contracts.cpp` reference them by name) and
// thin-call into one detail helper templated on both the component type
// and the registry type so the same template covers both mutable and
// const overloads.
namespace camera_entity_detail {
template <typename CamComp, typename RegT>
inline auto& camera_singleton(RegT& reg, const char* missing_msg, const char* multi_msg) {
    auto view = reg.template view<CamComp>();
    auto it = view.begin();
    if (it == view.end()) {
        throw std::logic_error(missing_msg);
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error(multi_msg);
    }
    return view.template get<CamComp>(entity);
}
}  // namespace camera_entity_detail

inline GameCamera& game_camera(entt::registry& reg) {
    return camera_entity_detail::camera_singleton<GameCamera>(reg,
        "GameCamera entity is missing; call spawn_game_camera() first",
        "multiple GameCamera entities exist");
}

inline UICamera& ui_camera(entt::registry& reg) {
    return camera_entity_detail::camera_singleton<UICamera>(reg,
        "UICamera entity is missing; call spawn_ui_camera() first",
        "multiple UICamera entities exist");
}

inline const GameCamera& game_camera(const entt::registry& reg) {
    return camera_entity_detail::camera_singleton<const GameCamera>(reg,
        "GameCamera entity is missing; call spawn_game_camera() first",
        "multiple GameCamera entities exist");
}

inline const UICamera& ui_camera(const entt::registry& reg) {
    return camera_entity_detail::camera_singleton<const UICamera>(reg,
        "UICamera entity is missing; call spawn_ui_camera() first",
        "multiple UICamera entities exist");
}
