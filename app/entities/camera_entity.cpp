#include "camera_entity.h"
#include <stdexcept>

void spawn_game_camera(entt::registry& reg) {
    auto existing = reg.view<GameCamera>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("GameCamera entity already exists");
    }

    Camera3D cam = {};
    cam.position   = {360.0f, 700.0f, 1900.0f};
    cam.target     = {360.0f, 0.0f,   500.0f};
    cam.up         = {0.0f, 1.0f, 0.0f};
    cam.fovy       = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    reg.emplace<GameCamera>(reg.create(), GameCamera{cam});
}

void spawn_ui_camera(entt::registry& reg) {
    auto existing = reg.view<UICamera>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("UICamera entity already exists");
    }

    Camera2D cam = {};
    cam.offset   = {0.0f, 0.0f};
    cam.target   = {0.0f, 0.0f};
    cam.rotation = 0.0f;
    cam.zoom     = 1.0f;
    reg.emplace<UICamera>(reg.create(), UICamera{cam});
}
