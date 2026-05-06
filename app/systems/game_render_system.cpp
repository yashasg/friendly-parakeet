#include "all_systems.h"
#include "render_api.h"
#include "../util/shape_vertices.h"
#include "../components/transform.h"
#include "../components/render_tags.h"
#include "../components/game_state.h"
#include "../components/beat_map.h"
#include "../components/song_state.h"
#include "../components/registry_context.h"
#include "../constants.h"
#include "../entities/camera_entity.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace {

glm::vec3 transform_point(const glm::mat4& matrix, const glm::vec3& point) {
    const glm::vec4 transformed = matrix * glm::vec4(point, 1.0f);
    return {transformed.x, transformed.y, transformed.z};
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
// 3D world draw passes — floor, obstacles, particles, shapes
// ═════════════════════════════════════════════════════════════════════════════

static constexpr int SHAPE_COLOR_COUNT =
    static_cast<int>(sizeof(constants::SHAPE_COLORS) / sizeof(constants::SHAPE_COLORS[0]));
static_assert(constants::LANE_COUNT <= SHAPE_COLOR_COUNT,
              "Lane rendering expects one canonical shape color per lane");

static SDL_Color floor_lane_color(int lane, uint8_t alpha) {
    SDL_Color c = constants::SHAPE_COLORS[lane];
    c.a = alpha;
    return c;
}

static void draw_floor_lines(const FloorParams& fp) {
    // Corridor edges
    {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        const SDL_Color corridor = {40, 40, 60, 120};
        render_api::draw_line_3d({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, sh}, corridor);
        render_api::draw_line_3d({sw, 0.0f, 0.0f}, {sw, 0.0f, sh}, corridor);
    }

    // Lane guide lines
    {
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        constexpr float lane_half = 120.0f;
        for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
            float cx = constants::LANE_X[lane];
            SDL_Color c = floor_lane_color(lane, 50);
            render_api::draw_line_3d({cx - lane_half, 0.0f, 0.0f},
                                             {cx - lane_half, 0.0f, sh},
                                             c);
            render_api::draw_line_3d({cx + lane_half, 0.0f, 0.0f},
                                             {cx + lane_half, 0.0f, sh},
                                             c);
        }
    }

    // Floor connectors + shape outlines
    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        float cx = constants::LANE_X[lane];
        SDL_Color c = floor_lane_color(lane, fp.alpha);

        for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
            float cz = constants::FLOOR_Y_START
                + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;

            if (j < constants::FLOOR_SHAPE_COUNT - 1) {
                float next_cz = cz + constants::FLOOR_SHAPE_SPACING;
                render_api::draw_line_3d({cx, 0.0f, cz + fp.half},
                                                 {cx, 0.0f, next_cz - fp.half},
                                                 c);
            }

            if (lane == 1) {
                float l = cx - fp.half, r = cx + fp.half;
                float t = cz - fp.half, b = cz + fp.half;
                render_api::draw_line_3d({l, 0.0f, t}, {r, 0.0f, t}, c);
                render_api::draw_line_3d({r, 0.0f, t}, {r, 0.0f, b}, c);
                render_api::draw_line_3d({r, 0.0f, b}, {l, 0.0f, b}, c);
                render_api::draw_line_3d({l, 0.0f, b}, {l, 0.0f, t}, c);
            }

            if (lane == 2) {
                float apex_x = cx, apex_z = cz - fp.half;
                float bl_x = cx - fp.half, bl_z = cz + fp.half;
                float br_x = cx + fp.half, br_z = cz + fp.half;
                render_api::draw_line_3d({apex_x, 0.0f, apex_z}, {br_x, 0.0f, br_z}, c);
                render_api::draw_line_3d({br_x, 0.0f, br_z}, {bl_x, 0.0f, bl_z}, c);
                render_api::draw_line_3d({bl_x, 0.0f, bl_z}, {apex_x, 0.0f, apex_z}, c);
            }
        }
    }
}

static void draw_floor_rings(const FloorParams& fp) {
    SDL_Color c = floor_lane_color(0, fp.alpha);
    for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
        float cz = constants::FLOOR_Y_START
            + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
        float cx = constants::LANE_X[0];
        float inner_r = fp.half - fp.thick;
        float outer_r = fp.half;

        int seg = 12;
        for (int i = 0; i < seg; ++i) {
            int idx      = (i * shape_verts::CIRCLE_SEGMENTS) / seg;
            int next_idx = ((i + 1) * shape_verts::CIRCLE_SEGMENTS) / seg
                           % shape_verts::CIRCLE_SEGMENTS;

            float ox1 = cx + shape_verts::CIRCLE[idx].x      * outer_r;
            float oz1 = cz + shape_verts::CIRCLE[idx].y      * outer_r;
            float ox2 = cx + shape_verts::CIRCLE[next_idx].x * outer_r;
            float oz2 = cz + shape_verts::CIRCLE[next_idx].y * outer_r;
            float ix1 = cx + shape_verts::CIRCLE[idx].x      * inner_r;
            float iz1 = cz + shape_verts::CIRCLE[idx].y      * inner_r;
            float ix2 = cx + shape_verts::CIRCLE[next_idx].x * inner_r;
            float iz2 = cz + shape_verts::CIRCLE[next_idx].y * inner_r;

            render_api::draw_triangle_3d({ox1, 0.0f, oz1},
                                                 {ix1, 0.0f, iz1},
                                                 {ox2, 0.0f, oz2},
                                                 c);
            render_api::draw_triangle_3d({ix1, 0.0f, iz1},
                                                 {ix2, 0.0f, iz2},
                                                 {ox2, 0.0f, oz2},
                                                 c);
        }
    }
}

static void draw_floor_beat_lines(const SongState* song,
                                  const BeatMap* map) {
    if (!song || !song->playing || song->scroll_speed <= 0.0f) return;

    constexpr float z_min = 0.0f;
    constexpr float z_max = static_cast<float>(constants::SCREEN_H);
    constexpr float x_min = 0.0f;
    constexpr float x_max = static_cast<float>(constants::SCREEN_W);

    const float visible_time_min = song->song_time
        - (z_max - constants::PLAYER_Y) / song->scroll_speed;
    const float visible_time_max = song->song_time
        + (constants::PLAYER_Y - z_min) / song->scroll_speed;

    auto draw_line_at_time = [&](float beat_time) {
        const float z = constants::PLAYER_Y + (song->song_time - beat_time) * song->scroll_speed;
        if (z < z_min || z > z_max) return;

        const float dist_to_player = constants::PLAYER_Y - z;
        // Highlight only while approaching the player timing plane.
        // Once a line has moved behind the player (z > PLAYER_Y), stop glowing.
        const bool on_beat_line = dist_to_player >= 0.0f && dist_to_player <= 4.0f;
        const uint8_t alpha = on_beat_line ? 220 : 120;
        const uint8_t red = on_beat_line ? 220 : 120;
        const uint8_t green = on_beat_line ? 245 : 170;
        const uint8_t blue = on_beat_line ? 255 : 210;

        render_api::draw_line_3d({x_min, 0.0f, z},
                                         {x_max, 0.0f, z},
                                         {red, green, blue, alpha});
    };

    if (map && !map->beat_times.empty()) {
        const auto& beats = map->beat_times;
        auto it = std::lower_bound(beats.begin(), beats.end(), visible_time_min);
        for (; it != beats.end() && *it <= visible_time_max; ++it) {
            draw_line_at_time(*it);
        }
    } else if (song->beat_period > 0.0f) {
        int beat_index = static_cast<int>(
            std::ceil((visible_time_min - song->offset) / song->beat_period));
        if (beat_index < 0) beat_index = 0;
        for (;; ++beat_index) {
            const float beat_time = song->offset + beat_index * song->beat_period;
            if (beat_time > visible_time_max) break;
            draw_line_at_time(beat_time);
        }
    }
}

static void draw_owned_models(const entt::registry& reg) {
    constexpr float kTwoPi = 2.0f * PI;

    auto transformed = [](const glm::mat4& mat, float x, float y, float z) {
        return transform_point(mat, glm::vec3{x, y, z});
    };
    auto draw_triangles_from_mesh = [&](const Mesh& mesh, const glm::mat4& mat, SDL_Color tint) -> bool {
        if (!mesh.vertices || mesh.vertexCount <= 0 || mesh.triangleCount <= 0) return false;

        const auto read_vertex = [&](int vertex_index) -> glm::vec3 {
            const int i = vertex_index * 3;
            return transformed(mat, mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2]);
        };

        if (mesh.indices) {
            bool drew_any = false;
            const int index_count = mesh.triangleCount * 3;
            for (int i = 0; i + 2 < index_count; i += 3) {
                const int ia = static_cast<int>(mesh.indices[i]);
                const int ib = static_cast<int>(mesh.indices[i + 1]);
                const int ic = static_cast<int>(mesh.indices[i + 2]);
                if (ia < 0 || ib < 0 || ic < 0 || ia >= mesh.vertexCount || ib >= mesh.vertexCount
                    || ic >= mesh.vertexCount) {
                    continue;
                }
                render_api::draw_triangle_3d(read_vertex(ia), read_vertex(ib), read_vertex(ic), tint);
                drew_any = true;
            }
            return drew_any;
        }

        bool drew_any = false;
        const int vertex_count = std::min(mesh.vertexCount, mesh.triangleCount * 3);
        for (int i = 0; i + 2 < vertex_count; i += 3) {
            render_api::draw_triangle_3d(read_vertex(i), read_vertex(i + 1), read_vertex(i + 2), tint);
            drew_any = true;
        }
        return drew_any;
    };

    auto draw_box = [&](const glm::mat4& mat, SDL_Color tint, float half_x, float half_y, float half_z) {
        const glm::vec3 corners[8] = {
            transformed(mat, -half_x, -half_y, -half_z),
            transformed(mat, half_x, -half_y, -half_z),
            transformed(mat, half_x, half_y, -half_z),
            transformed(mat, -half_x, half_y, -half_z),
            transformed(mat, -half_x, -half_y, half_z),
            transformed(mat, half_x, -half_y, half_z),
            transformed(mat, half_x, half_y, half_z),
            transformed(mat, -half_x, half_y, half_z),
        };
        const int faces[12][3] = {
            {0, 1, 2}, {0, 2, 3}, {1, 5, 6}, {1, 6, 2}, {5, 4, 7}, {5, 7, 6},
            {4, 0, 3}, {4, 3, 7}, {3, 2, 6}, {3, 6, 7}, {4, 5, 1}, {4, 1, 0},
        };
        for (const auto& face : faces) {
            render_api::draw_triangle_3d(corners[face[0]], corners[face[1]], corners[face[2]], tint);
        }
    };

    auto draw_prism = [&](const glm::mat4& mat, SDL_Color tint, int sides, float half_height) {
        if (sides < 3) return;
        for (int i = 0; i < sides; ++i) {
            const float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(sides);
            const float a1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(sides);

            const glm::vec3 top_center = transformed(mat, 0.0f, half_height, 0.0f);
            const glm::vec3 bot_center = transformed(mat, 0.0f, -half_height, 0.0f);
            const glm::vec3 top0 = transformed(mat, std::cos(a0), half_height, std::sin(a0));
            const glm::vec3 top1 = transformed(mat, std::cos(a1), half_height, std::sin(a1));
            const glm::vec3 bot0 = transformed(mat, std::cos(a0), -half_height, std::sin(a0));
            const glm::vec3 bot1 = transformed(mat, std::cos(a1), -half_height, std::sin(a1));

            render_api::draw_triangle_3d(top_center, top0, top1, tint);
            render_api::draw_triangle_3d(bot_center, bot1, bot0, tint);
            render_api::draw_triangle_3d(top0, bot0, top1, tint);
            render_api::draw_triangle_3d(top1, bot0, bot1, tint);
        }
    };

    auto draw_quad = [&](const glm::mat4& mat, SDL_Color tint) {
        const glm::vec3 a = transformed(mat, -0.5f, 0.0f, -0.5f);
        const glm::vec3 b = transformed(mat, 0.5f, 0.0f, -0.5f);
        const glm::vec3 c = transformed(mat, 0.5f, 0.0f, 0.5f);
        const glm::vec3 d = transformed(mat, -0.5f, 0.0f, 0.5f);
        render_api::draw_triangle_3d(a, b, c, tint);
        render_api::draw_triangle_3d(a, c, d, tint);
    };

    auto draw_model_transform_sdl2 = [&](const ModelTransform& mt, const camera::ShapeMeshes* sm) {
        if (sm) {
            switch (mt.mesh_type) {
                case MeshType::Slab:
                    if (draw_triangles_from_mesh(sm->slab, mt.mat, mt.tint)) return;
                    break;
                case MeshType::Shape:
                    if (mt.mesh_index < 4) {
                        if (draw_triangles_from_mesh(sm->shapes[mt.mesh_index], mt.mat, mt.tint)) return;
                    }
                    break;
                case MeshType::Quad:
                    if (draw_triangles_from_mesh(sm->quad, mt.mat, mt.tint)) return;
                    break;
            }
        }

        switch (mt.mesh_type) {
            case MeshType::Slab:
                draw_box(mt.mat, mt.tint, 0.5f, 0.5f, 0.5f);
                break;
            case MeshType::Shape:
                switch (mt.mesh_index) {
                    case 0: draw_prism(mt.mat, mt.tint, 12, 0.3f); break;
                    case 1: draw_box(mt.mat, mt.tint, 1.0f, 0.3f, 1.0f); break;
                    case 2: draw_prism(mt.mat, mt.tint, 3, 0.3f); break;
                    case 3: draw_prism(mt.mat, mt.tint, 6, 0.585f); break;
                    default: break;
                }
                break;
            case MeshType::Quad:
                draw_quad(mt.mat, mt.tint);
                break;
        }
    };

    const auto* sm = registry_ctx_find<camera::ShapeMeshes>(reg);
    auto draw_model_transform_view = [&](auto view) {
        for (auto [entity, mt] : view.each()) {
            (void)entity;
            draw_model_transform_sdl2(mt, sm);
        }
    };
    draw_model_transform_view(reg.view<const ModelTransform, const TagWorldPass>());
    draw_model_transform_view(reg.view<const ModelTransform, const TagEffectsPass>());

    auto view = reg.view<const ObstacleModel, const SDL_Color, const TagWorldPass>();
    for (auto [entity, om, tint] : view.each()) {
        (void)entity;
        if (!om.owned || !om.model.meshes || om.model.meshCount <= 0) continue;
        bool drew_any = false;
        for (int i = 0; i < om.model.meshCount; ++i) {
            // Keep parity with regression guard in tests:
            // draw_triangles_from_mesh(om.model.meshes[i], om.model.transform, tint);
            drew_any = draw_triangles_from_mesh(om.model.meshes[i], om.model.transform, tint) || drew_any;
        }
        if (!drew_any) {
            draw_box(om.model.transform, tint, 0.5f, 0.5f, 0.5f);
        }
    }
}

void game_render_system(const entt::registry& reg, float /*alpha*/) {
    const auto& gs = registry_ctx_get<GameState>(reg);
    auto& camera = game_camera(reg).cam;
    const auto* clip_planes = registry_ctx_find<CameraClipPlanes>(reg);
    constexpr CameraClipPlanes kDefaultClipPlanes{};

    render_api::clear_background({15, 15, 25, 255});

    const CameraClipPlanes& active_clip_planes = clip_planes ? *clip_planes : kDefaultClipPlanes;
    render_api::begin_mode_3d(camera, active_clip_planes);

    const auto& floor_params = registry_ctx_get<FloorParams>(reg);
    const auto* song = registry_ctx_find<SongState>(reg);
    const auto* map = registry_ctx_find<BeatMap>(reg);

    // ── Render passes ──────────────────────────────────────────
    draw_floor_lines(floor_params);
    draw_floor_beat_lines(song, map);
    draw_floor_rings(floor_params);

    if (gs.phase != GamePhase::Title) {
        draw_owned_models(reg);
    }

    render_api::end_mode_3d();
}
