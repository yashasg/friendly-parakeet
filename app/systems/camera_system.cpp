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
#include "../components/song_state.h"
#include "../constants.h"
#include "../platform_display.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <tuple>

// Shape properties: radius_scale, height_ratio (height / radius)
const ShapeProps SHAPE_PROPS[4] = {
    { 0.5f, 0.6f },  // Circle:   cylinder
    { 0.5f, 0.6f },  // Square:   cube
    { 0.5f, 0.6f },  // Triangle: cone
    { 0.6f, 1.17f }, // Hexagon:  cylinder (6 slices)
};

namespace camera {

// GLSL 330 (desktop OpenGL 3.3) vs GLSL 100 (WebGL / OpenGL ES 2.0)
#ifdef PLATFORM_WEB
static const char* MESH_VS =
    "#version 100\n"
    "attribute vec3 vertexPosition;\n"
    "attribute vec3 vertexNormal;\n"
    "attribute vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matNormal;\n"
    "varying vec3 fragNormal;\n"
    "void main() {\n"
    "    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* MESH_FS =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 fragNormal;\n"
    "uniform vec4 colDiffuse;\n"
    "void main() {\n"
    "    float shade = 0.35 + 0.65 * clamp(fragNormal.y, 0.0, 1.0);\n"
    "    gl_FragColor = vec4(colDiffuse.rgb * shade, colDiffuse.a);\n"
    "}\n";
#else
static const char* MESH_VS =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec3 vertexNormal;\n"
    "in vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matNormal;\n"
    "out vec3 fragNormal;\n"
    "void main() {\n"
    "    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* MESH_FS =
    "#version 330\n"
    "in vec3 fragNormal;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    float shade = 0.35 + 0.65 * clamp(fragNormal.y, 0.0, 1.0);\n"
    "    finalColor = vec4(colDiffuse.rgb * shade, colDiffuse.a);\n"
    "}\n";
#endif

static ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};

    sm.shapes[0] = GenMeshCylinder(1.0f, SHAPE_PROPS[0].height_ratio, 12);
    sm.shapes[1] = GenMeshCube(2.0f, SHAPE_PROPS[1].height_ratio, 2.0f);
    sm.shapes[2] = GenMeshCone(1.0f, SHAPE_PROPS[2].height_ratio, 3);
    sm.shapes[3] = GenMeshCylinder(1.0f, SHAPE_PROPS[3].height_ratio, 6);
    sm.slab = GenMeshCube(1.0f, 1.0f, 1.0f);
    sm.quad = GenMeshPlane(1.0f, 1.0f, 1, 1);

    Shader shader = LoadShaderFromMemory(MESH_VS, MESH_FS);
    sm.material = LoadMaterialDefault();
    sm.material.shader = shader;

    return sm;
}

static void unload_shape_meshes(ShapeMeshes& sm) {
    UnloadShader(sm.material.shader);
    for (int i = 0; i < 4; ++i)
        UnloadMesh(sm.shapes[i]);
    UnloadMesh(sm.slab);
    UnloadMesh(sm.quad);
    UnloadMaterial(sm.material);
}

void init(entt::registry& reg) {
    // 3D gameplay camera
    Camera3D cam3d = {};
    cam3d.position   = {360.0f, 700.0f, 1600.0f};
    cam3d.target     = {360.0f, 0.0f,   500.0f};
    cam3d.up         = {0.0f, 1.0f, 0.0f};
    cam3d.fovy       = 45.0f;
    cam3d.projection = CAMERA_PERSPECTIVE;
    reg.ctx().emplace<GameCamera>(GameCamera{cam3d});

    // 2D UI camera (identity — screen-space, no transform)
    Camera2D cam2d = {};
    cam2d.offset   = {0, 0};
    cam2d.target   = {0, 0};
    cam2d.rotation = 0.0f;
    cam2d.zoom     = 1.0f;
    reg.ctx().emplace<UICamera>(UICamera{cam2d});

    // Render targets
    RenderTexture2D world = LoadRenderTexture(
        constants::SCREEN_W, constants::SCREEN_H);
    SetTextureFilter(world.texture, TEXTURE_FILTER_BILINEAR);
    RenderTexture2D ui = LoadRenderTexture(
        constants::SCREEN_W, constants::SCREEN_H);
    SetTextureFilter(ui.texture, TEXTURE_FILTER_BILINEAR);
    reg.ctx().emplace<RenderTargets>(RenderTargets{world, ui});

    reg.ctx().emplace<ScreenTransform>();
    reg.ctx().emplace<FloorParams>();
    reg.ctx().emplace<ShapeMeshes>(build_shape_meshes());
}

void shutdown(entt::registry& reg) {
    unload_shape_meshes(reg.ctx().get<ShapeMeshes>());
    auto& targets = reg.ctx().get<RenderTargets>();
    UnloadRenderTexture(targets.world);
    UnloadRenderTexture(targets.ui);
}

} // namespace camera

// ── Helpers ─────────────────────────────────────────────────────────────────

// GenMeshCube is centered at origin. Translate to position the slab's
// bottom-left corner at (x, 0, z) with the given dimensions.
static Matrix slab_matrix(float x, float z, float w, float h, float d) {
    return MatrixMultiply(MatrixScale(w, h, d),
                          MatrixTranslate(x + w/2, h/2, z + d/2));
}

static Matrix shape_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    return MatrixMultiply(MatrixScale(s, s, s), MatrixTranslate(cx, y_3d, cz));
}

// Cone (triangle) rotated so the base faces the camera with one vertex pointing up (△)
static Matrix cone_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    Matrix scale = MatrixScale(s, s, s);
    Matrix rotX = MatrixRotateX(-90.0f * DEG2RAD);
    Matrix rotZ = MatrixRotateZ(90.0f * DEG2RAD);
    Matrix translate = MatrixTranslate(cx, y_3d, cz);
    return MatrixMultiply(MatrixMultiply(MatrixMultiply(scale, rotX), rotZ), translate);
}

// Pick the correct matrix for a shape mesh (cone needs rotation)
static Matrix make_shape_matrix(int mesh_index, float cx, float y_3d, float cz,
                                float sz, float radius_scale) {
    if (mesh_index == static_cast<int>(Shape::Triangle))
        return cone_matrix(cx, y_3d, cz, sz, radius_scale);
    return shape_matrix(cx, y_3d, cz, sz, radius_scale);
}

// ── game_camera_system: model-to-world transforms for all 3D renderables ────

void game_camera_system(entt::registry& reg, float /*dt*/) {
    // 1. Single-slab obstacle transforms
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::LanePushLeft:
                case ObstacleKind::LanePushRight:
                    reg.emplace_or_replace<ModelTransform>(entity,
                        ModelTransform{slab_matrix(pos.x-dsz.w/2, pos.y, dsz.w, dsz.h, constants::OBSTACLE_3D_HEIGHT),
                                       col, MeshType::Slab, 0});
                    break;
                case ObstacleKind::LowBar:
                    reg.emplace_or_replace<ModelTransform>(entity,
                        ModelTransform{slab_matrix(0, pos.y, static_cast<float>(constants::SCREEN_W), dsz.h, constants::LOWBAR_3D_HEIGHT),
                                       col, MeshType::Slab, 0});
                    break;
                case ObstacleKind::HighBar:
                    reg.emplace_or_replace<ModelTransform>(entity,
                        ModelTransform{slab_matrix(0, pos.y, static_cast<float>(constants::SCREEN_W), dsz.h, constants::HIGHBAR_3D_HEIGHT),
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
            auto& parent_pos = reg.get<Position>(mc.parent);
            float z = parent_pos.y + mc.z_offset;

            if (mc.mesh_type == MeshType::Slab) {
                reg.emplace_or_replace<ModelTransform>(entity,
                    ModelTransform{slab_matrix(mc.x, z, mc.width, mc.depth, mc.height),
                                   mc.tint, MeshType::Slab, 0});
            } else {
                const auto& props = SHAPE_PROPS[mc.mesh_index];
                reg.emplace_or_replace<ModelTransform>(entity,
                    ModelTransform{make_shape_matrix(mc.mesh_index, mc.x, 0.0f, z, mc.width, props.radius_scale),
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
            int shape_idx = static_cast<int>(pshape.current);
            const auto& props = SHAPE_PROPS[shape_idx];
            reg.emplace_or_replace<ModelTransform>(entity,
                ModelTransform{make_shape_matrix(shape_idx, pos.x, y_3d, pos.y, sz, props.radius_scale),
                               col, MeshType::Shape, shape_idx});
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

    // 5. Floor pulse params (SongState → FloorParams singleton)
    {
        auto& fp = reg.ctx().get<FloorParams>();
        auto* song = reg.ctx().find<SongState>();
        float pulse = 0.0f;
        if (song && song->playing && song->beat_period > 0.0f && song->current_beat >= 0) {
            float time_since_beat = song->song_time
                - (song->offset + static_cast<float>(song->current_beat) * song->beat_period);
            float pulse_t = std::clamp(time_since_beat / constants::FLOOR_PULSE_DECAY, 0.0f, 1.0f);
            float ease = 1.0f - (1.0f - pulse_t) * (1.0f - pulse_t);
            pulse = 1.0f - ease;
        }
        float alpha_f = constants::FLOOR_ALPHA_REST
            + (constants::FLOOR_ALPHA_PEAK - constants::FLOOR_ALPHA_REST) * pulse;
        float scale = constants::FLOOR_SCALE_REST
            + (constants::FLOOR_SCALE_PEAK - constants::FLOOR_SCALE_REST) * pulse;
        fp.size  = constants::FLOOR_SHAPE_SIZE * scale;
        fp.half  = fp.size / 2.0f;
        fp.thick = constants::FLOOR_OUTLINE_THICK;
        fp.alpha = static_cast<uint8_t>(alpha_f);
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
        auto& cam = reg.ctx().get<GameCamera>().cam;
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
