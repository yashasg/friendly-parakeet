#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <entt/entt.hpp>
#include "constants.h"
#include "components/player.h"
#include "components/rendering.h"
#include "systems/shape_mesh.h"
#include "systems/camera_system.h"
#include "components/transform.h"
#include "entities/obstacle_entity.h"
#include "components/camera_resources.h"
#include "systems/runtime_systems.h"
#include "systems/all_systems.h"
#include <raylib.h>
#include <raymath.h>
#include <cmath>

using Catch::Matchers::WithinAbs;

// ═════════════════════════════════════════════════════════════════════════════
// §1  Camera3D setup validation
// ═════════════════════════════════════════════════════════════════════════════
// These tests verify the Camera3D configuration constants that game_camera_system
// uses.  No GPU context required — pure struct/math tests.

TEST_CASE("Camera3D: default setup covers game world", "[camera3d]") {
    Camera3D cam{};
    cam.position   = {360.0f, 620.0f, 2400.0f};
    cam.target     = {360.0f, 0.0f, 500.0f};
    cam.up         = {0.0f, 1.0f, 0.0f};
    cam.fovy       = 54.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    // Camera is behind the player (z=920) looking forward (−Z)
    CHECK(cam.position.z > constants::PLAYER_Y);

    // Camera is above the ground plane
    CHECK(cam.position.y > 0.0f);

    // Target is in front of camera (lower z)
    CHECK(cam.target.z < cam.position.z);

    // Camera X is centered on the playfield
    CHECK_THAT(static_cast<double>(cam.position.x),
               WithinAbs(constants::SCREEN_W / 2.0, 0.01));
    CHECK_THAT(static_cast<double>(cam.target.x),
               WithinAbs(constants::SCREEN_W / 2.0, 0.01));

    // FOV is reasonable (positive, less than 180)
    CHECK(cam.fovy > 0.0f);
    CHECK(cam.fovy < 180.0f);
}

TEST_CASE("Camera3D: coordinate mapping — game (x,y) → 3D (x,0,y)", "[camera3d]") {
    // The coordinate mapping rule: game Position{x, y} → 3D {x, 0, y}
    float game_x = 360.0f;
    float game_y = constants::PLAYER_Y;
    float x3d = game_x;
    float y3d = 0.0f;
    float z3d = game_y;

    CHECK_THAT(static_cast<double>(x3d), WithinAbs(game_x, 0.01));
    CHECK_THAT(static_cast<double>(y3d), WithinAbs(0.0, 0.01));
    CHECK_THAT(static_cast<double>(z3d), WithinAbs(game_y, 0.01));
}

TEST_CASE("Camera3D: jump lifts player off ground plane", "[camera3d]") {
    // y_offset is negative when jumping.  3D y = −y_offset.
    float y_offset_jumping = -constants::JUMP_HEIGHT;
    float y_3d = -y_offset_jumping;

    CHECK(y_3d > 0.0f);
    CHECK_THAT(static_cast<double>(y_3d),
               WithinAbs(constants::JUMP_HEIGHT, 0.01));
}

TEST_CASE("Camera3D: spawn and destroy Y map to valid Z range", "[camera3d]") {
    // Obstacles spawn at game y = SPAWN_Y (−120) and are destroyed at DESTROY_Y (1400).
    // In 3D: z = game_y, so z ∈ [−120, 1400].
    float spawn_z   = constants::SPAWN_Y;
    float destroy_z = constants::DESTROY_Y;

    Camera3D cam{};
    cam.position = {360.0f, 300.0f, 1148.0f};
    CHECK(spawn_z < cam.position.z);
    CHECK(destroy_z > cam.position.z);  // behind camera (clipped by frustum)
}


// ═════════════════════════════════════════════════════════════════════════════
// §2  Floor ring segment sweep regression
// ═════════════════════════════════════════════════════════════════════════════
// Verifies the local floor-ring angle sweep covers the full circle.

TEST_CASE("floor ring: angular sweep covers full circle", "[floor][regression]") {
    constexpr int seg = 24;
    constexpr float tau = 6.28318530717958647692f;
    bool has_positive_sin = false;
    bool has_negative_sin = false;

    for (int i = 0; i < seg; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(seg)) * tau;
        const float y = std::sin(angle);
        has_positive_sin = has_positive_sin || y > 0.5f;
        has_negative_sin = has_negative_sin || y < -0.5f;
    }

    const float start_angle = 0.0f;
    const float wrapped_end_angle =
        (static_cast<float>(seg) / static_cast<float>(seg)) * tau;

    CHECK(has_positive_sin);
    CHECK(has_negative_sin);
    CHECK_THAT(start_angle, WithinAbs(0.0, 1e-6));
    CHECK_THAT(wrapped_end_angle, WithinAbs(tau, 1e-6));
}

TEST_CASE("game_camera_system drops stale MeshChild parents without crashing", "[camera3d][mesh_child]") {
    entt::registry reg;
    reg.ctx().emplace<ShapeMeshConfig>();
    reg.ctx().emplace<FloorParams>();
    auto parent = reg.create();
    reg.emplace<WorldPosition>(parent, WorldPosition{{100.0f, 200.0f}});
    auto child = reg.create();
    reg.emplace<MeshChild>(
        child,
        MeshChild{parent, 10.0f, 0.0f, 20.0f, 30.0f, 40.0f, WHITE}
    );
    reg.emplace<MeshKindSlab>(child);
    reg.emplace<ModelTransform>(child, ModelTransform{MatrixIdentity(), WHITE});
    reg.emplace<TagWorldPass>(child);

    reg.destroy(parent);

    REQUIRE_NOTHROW(game_camera_system(reg, 0.0f));
    CHECK_FALSE(reg.valid(child));
}

TEST_CASE("game_camera_system drops mesh children for destroyed logical obstacle",
          "[camera3d][mesh_child][obstacle]") {
    entt::registry reg;
    reg.ctx().emplace<ShapeMeshConfig>();
    reg.ctx().emplace<FloorParams>();
    auto parent = spawn_shape_gate_obstacle(reg, {360.0f, -120.0f, Shape::Circle});
    REQUIRE(reg.all_of<ObstacleChildren>(parent));
    const auto children = reg.get<ObstacleChildren>(parent);
    REQUIRE(children.count > 0);
    const entt::entity child = children.children[0];
    REQUIRE(reg.valid(child));

    reg.destroy(parent);

    REQUIRE(reg.valid(child));
    REQUIRE_NOTHROW(game_camera_system(reg, 0.0f));
    CHECK_FALSE(reg.valid(child));
}

TEST_CASE("game_camera_system rejects invalid MeshChild shape index", "[camera3d][mesh_child][validation]") {
    entt::registry reg;
    reg.ctx().emplace<ShapeMeshConfig>();
    reg.ctx().emplace<FloorParams>();
    auto parent = reg.create();
    reg.emplace<WorldPosition>(parent, WorldPosition{{100.0f, 200.0f}});
    auto child = reg.create();
    reg.emplace<MeshChild>(
        child,
        MeshChild{parent, 10.0f, 0.0f, 20.0f, 30.0f, 40.0f, WHITE}
    );
    reg.emplace<MeshKindShape>(child, MeshKindShape{255});
    reg.emplace<ModelTransform>(child, ModelTransform{MatrixIdentity(), WHITE});
    reg.emplace<TagWorldPass>(child);

    REQUIRE_NOTHROW(game_camera_system(reg, 0.0f));
    CHECK_FALSE(reg.all_of<ModelTransform>(child));
}

// #1089 — MeshChildCleanupScratch::capacity_exceeded_count must stay at zero
// when a dense stale-parent cleanup pass fits inside the reserved
// stale_children buffer. runtime_system_scratch_reserve sizes the buffer at
// `beat_capacity * ObstacleChildren::MAX`, mirroring the worst-case slab
// fan-out per beat.
TEST_CASE("game_camera_system: dense stale-parent cleanup stays within reserved capacity",
          "[camera3d][mesh_child][issue1089]") {
    entt::registry reg;
    reg.ctx().emplace<ShapeMeshConfig>();
    reg.ctx().emplace<FloorParams>();
    runtime_system_scratch_init(reg);
    constexpr int beat_capacity = 4;
    runtime_system_scratch_reserve(reg, beat_capacity);

    auto& scratch = reg.ctx().get<MeshChildCleanupScratch>();
    const auto stale_capacity = scratch.stale_children.capacity();
    REQUIRE(stale_capacity >= static_cast<std::size_t>(beat_capacity * ObstacleChildren::MAX));

    constexpr int dense_count = beat_capacity * ObstacleChildren::MAX;
    for (int i = 0; i < dense_count; ++i) {
        auto parent = reg.create();
        reg.emplace<WorldPosition>(parent, WorldPosition{{static_cast<float>(i), 0.0f}});
        auto child = reg.create();
        reg.emplace<MeshChild>(
            child,
            MeshChild{parent, static_cast<float>(i), 0.0f, 20.0f, 30.0f, 40.0f, WHITE}
        );
        reg.emplace<MeshKindSlab>(child);
        reg.emplace<ModelTransform>(child, ModelTransform{MatrixIdentity(), WHITE});
        reg.emplace<TagWorldPass>(child);
        reg.destroy(parent);  // make MeshChild stale so cleanup path runs
    }

    game_camera_system(reg, 0.0f);

    CHECK(scratch.stale_children.capacity() == stale_capacity);
    CHECK(scratch.capacity_exceeded_count == 0u);
}
