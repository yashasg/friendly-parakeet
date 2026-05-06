#include "camera_entity.h"

void spawn_game_camera(entt::registry& reg) {
    if (try_game_camera(reg) != nullptr) {
        throw std::logic_error("GameCamera entity already exists");
    }

    Camera3D cam{};
    cam.position   = {360.0f, 620.0f, 2400.0f};
    cam.target     = {360.0f, 0.0f,   500.0f};
    cam.up         = {0.0f, 1.0f, 0.0f};
    cam.fovy       = 54.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    reg.emplace<GameCamera>(reg.create(), GameCamera{cam});
}

void ensure_game_camera(entt::registry& reg) {
    if (try_game_camera(reg) == nullptr) {
        spawn_game_camera(reg);
    }
}
