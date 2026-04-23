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
#include "../components/camera.h"
#include "../constants.h"
#include "../platform_display.h"
#include <raylib.h>
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
    return {
        w, 0, 0, 0,
        0, h, 0, 0,
        0, 0, d, 0,
        x, 0, z, 1,
    };
}

static Matrix shape_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    return {
        s,    0.0f, 0.0f, 0.0f,
        0.0f, s,    0.0f, 0.0f,
        0.0f, 0.0f, s,    0.0f,
        cx, y_3d, cz, 1.0f,
    };
}

// ── camera_system: compute transforms for all renderables ───────────────────

void camera_system(entt::registry& reg, float /*dt*/) {
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

    constexpr float OBSTACLE_HEIGHT = 20.0f;
    constexpr float LOWBAR_HEIGHT   = 30.0f;
    constexpr float HIGHBAR_HEIGHT  = 10.0f;

    // 2. Obstacle slab transforms
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            // Remove stale transforms from previous frame
            while (reg.any_of<ModelTransform>(entity))
                reg.remove<ModelTransform>(entity);

            auto emit = [&](float x, float z, float w, float d, float h, Color tint) {
                // Multi-slab obstacles (ShapeGate, LaneBlock, etc.) need multiple
                // transforms per entity. Use a separate entity or overwrite.
                // For simplicity: emplace on the entity for single-slab, and use
                // the primary entity's transform for the main slab.
                reg.emplace_or_replace<ModelTransform>(entity,
                    ModelTransform{slab_matrix(x, z, w, h, d), tint, MeshType::Slab, 0});
            };

            switch (obs.kind) {
                case ObstacleKind::ShapeGate:
                    // Two slabs — store the wider one on the entity
                    emit(0, pos.y, pos.x-50, dsz.h, OBSTACLE_HEIGHT, col);
                    // Second slab will be handled by render_system inline
                    // (multi-slab obstacles need a different approach — skip for now)
                    break;
                case ObstacleKind::LanePushLeft:
                case ObstacleKind::LanePushRight:
                    emit(pos.x-dsz.w/2, pos.y, dsz.w, dsz.h, OBSTACLE_HEIGHT, col);
                    break;
                case ObstacleKind::LowBar:
                    emit(0, pos.y, static_cast<float>(constants::SCREEN_W), dsz.h, LOWBAR_HEIGHT, col);
                    break;
                case ObstacleKind::HighBar:
                    emit(0, pos.y, static_cast<float>(constants::SCREEN_W), dsz.h, HIGHBAR_HEIGHT, col);
                    break;
                default:
                    // LaneBlock, ComboGate, SplitPath have multiple slabs per entity.
                    // These need render-side iteration for now.
                    break;
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
            Matrix mat = {
                sz, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, sz, 0,
                pos.x - half, 0, pos.y - half, 1,
            };
            reg.emplace_or_replace<ModelTransform>(entity,
                ModelTransform{mat, col, MeshType::Quad, 0});
        }
    }

    // 5. Ghost shape transforms (shape gates, combo gates, split paths)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (!req) break;
                    // Ghost shape is a separate visual — we can't put two
                    // ModelTransforms on one entity. Ghost shapes will still
                    // be handled by render_system for now.
                    break;
                }
                default: break;
            }
        }
    }
}
