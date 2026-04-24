#include "camera_system.h"
#include "all_systems.h"
#include "../components/shape_mesh.h"
#include "../components/rendering.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/particle.h"
#include "../components/lifetime.h"
#include "../components/scoring.h"
#include "../components/camera.h"
#include "../constants.h"
#include "../platform_display.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>

namespace camera {

ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};
    for (int i = 0; i < 4; ++i) {
        sm.shapes[i] = build_prism(SHAPE_TABLE[i]);
        UploadMesh(&sm.shapes[i], false);
    }
    sm.slab = build_unit_slab();
    UploadMesh(&sm.slab, false);
    sm.quad = build_unit_quad();
    UploadMesh(&sm.quad, false);
    sm.material = LoadMaterialDefault();
    return sm;
}

void unload_shape_meshes(ShapeMeshes& sm) {
    for (int i = 0; i < 4; ++i)
        UnloadMesh(sm.shapes[i]);
    UnloadMesh(sm.slab);
    UnloadMesh(sm.quad);
    UnloadMaterial(sm.material);
}

} // namespace camera

// ── Helpers ─────────────────────────────────────────────────────────────────

static Matrix slab_matrix(float x, float z, float w, float h, float d) {
    return MatrixMultiply(MatrixScale(w, h, d), MatrixTranslate(x, 0, z));
}

static Matrix shape_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    return MatrixMultiply(MatrixScale(s, s, s), MatrixTranslate(cx, y_3d, cz));
}

// ── game_camera_system: model-to-world transforms for all 3D renderables ────

void game_camera_system(entt::registry& reg, float /*dt*/) {
    constexpr float OBSTACLE_HEIGHT = 20.0f;
    constexpr float LOWBAR_HEIGHT   = 30.0f;
    constexpr float HIGHBAR_HEIGHT  = 10.0f;

    // 1. Single-slab obstacle transforms
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::LanePushLeft:
                case ObstacleKind::LanePushRight:
                    reg.emplace_or_replace<ModelTransform>(entity,
                        ModelTransform{slab_matrix(pos.x-dsz.w/2, pos.y, dsz.w, dsz.h, OBSTACLE_HEIGHT),
                                       col, MeshType::Slab, 0});
                    break;
                case ObstacleKind::LowBar:
                    reg.emplace_or_replace<ModelTransform>(entity,
                        ModelTransform{slab_matrix(0, pos.y, static_cast<float>(constants::SCREEN_W), dsz.h, LOWBAR_HEIGHT),
                                       col, MeshType::Slab, 0});
                    break;
                case ObstacleKind::HighBar:
                    reg.emplace_or_replace<ModelTransform>(entity,
                        ModelTransform{slab_matrix(0, pos.y, static_cast<float>(constants::SCREEN_W), dsz.h, HIGHBAR_HEIGHT),
                                       col, MeshType::Slab, 0});
                    break;
                default:
                    break;
            }
        }
    }

    // 2. MeshChild transforms (multi-slab obstacles, ghost shapes)
    {
        auto view = reg.view<MeshChild>();
        for (auto [entity, mc] : view.each()) {
            if (!reg.valid(mc.parent)) continue;
            auto* parent_pos = reg.try_get<Position>(mc.parent);
            if (!parent_pos) continue;

            float z = parent_pos->y + mc.z_offset;

            if (mc.mesh_type == MeshType::Slab) {
                reg.emplace_or_replace<ModelTransform>(entity,
                    ModelTransform{slab_matrix(mc.x, z, mc.width, mc.depth, mc.height),
                                   mc.tint, MeshType::Slab, 0});
            } else {
                const auto& desc = SHAPE_TABLE[mc.mesh_index];
                reg.emplace_or_replace<ModelTransform>(entity,
                    ModelTransform{shape_matrix(mc.x, 0.0f, z, mc.width, desc.radius_scale),
                                   mc.tint, MeshType::Shape, mc.mesh_index});
            }
        }
    }

    // 3. Player shape transform
    {
        auto view = reg.view<PlayerTag, Position, PlayerShape, VerticalState, Color>();
        for (auto [entity, pos, pshape, vstate, col] : view.each()) {
            float y_3d = -vstate.y_offset;
            float sz = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) sz *= 0.5f;
            const auto& desc = SHAPE_TABLE[static_cast<int>(pshape.current)];
            reg.emplace_or_replace<ModelTransform>(entity,
                ModelTransform{shape_matrix(pos.x, y_3d, pos.y, sz, desc.radius_scale),
                               col, MeshType::Shape, static_cast<int>(pshape.current)});
        }
    }

    // 4. Particle transforms
    {
        auto view = reg.view<ParticleTag, Position, ParticleData, Color, Lifetime>();
        for (auto [entity, pos, pdata, col, life] : view.each()) {
            float ratio = (life.max_time > 0.0f) ? (life.remaining / life.max_time) : 1.0f;
            float sz = pdata.size * ratio;
            float half = sz / 2.0f;
            Matrix mat = MatrixMultiply(
                MatrixScale(sz, 1, sz),
                MatrixTranslate(pos.x - half, 0, pos.y - half));
            reg.emplace_or_replace<ModelTransform>(entity,
                ModelTransform{mat, col, MeshType::Quad, 0});
        }
    }
}

// ── ui_camera_system: screen-space transforms for UI layer ──────────────────

void ui_camera_system(entt::registry& reg, float /*dt*/) {
    // 1. Screen transform (letterbox)
    {
        float win_w, win_h;
        platform_get_display_size(win_w, win_h);
        float scale = std::min(
            win_w / static_cast<float>(constants::SCREEN_W),
            win_h / static_cast<float>(constants::SCREEN_H));
        float dst_w = constants::SCREEN_W * scale;
        float dst_h = constants::SCREEN_H * scale;
        auto& st     = reg.ctx().get<ScreenTransform>();
        st.offset_x  = (win_w - dst_w) * 0.5f;
        st.offset_y  = (win_h - dst_h) * 0.5f;
        st.scale     = scale;
    }

    // 2. Popup screen-space projection
    {
        auto& cam = reg.ctx().get<Camera3D>();
        auto view = reg.view<ScorePopup, Position>();
        for (auto [entity, popup, pos] : view.each()) {
            Vector3 world_pos = {pos.x, 5.0f, pos.y};
            Vector2 sp = GetWorldToScreenEx(world_pos, cam,
                             constants::SCREEN_W, constants::SCREEN_H);
            reg.emplace_or_replace<ScreenPosition>(entity,
                ScreenPosition{sp.x, sp.y});
        }
    }
}
