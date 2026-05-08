#include "floor_render_system.h"

#include "../components/beat_map.h"
#include "../components/song_state.h"
#include "../constants.h"
#include "../rendering/camera_resources.h"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int SHAPE_COLOR_COUNT =
    static_cast<int>(sizeof(constants::SHAPE_COLORS) / sizeof(constants::SHAPE_COLORS[0]));
static_assert(constants::LANE_COUNT <= SHAPE_COLOR_COUNT,
              "Lane rendering expects one canonical shape color per lane");

Color floor_lane_color(int lane, uint8_t alpha) {
    Color c = constants::SHAPE_COLORS[lane];
    c.a = alpha;
    return c;
}

void draw_floor_lines(const FloorParams& fp) {
    constexpr float y = 0.0f;

    {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        const Color edge_color{40, 40, 60, 120};
        DrawLine3D({0.0f, y, 0.0f}, {0.0f, y, sh}, edge_color);
        DrawLine3D({sw, y, 0.0f}, {sw, y, sh}, edge_color);
    }

    {
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        constexpr float lane_half = 120.0f;
        for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
            const float cx = constants::LANE_X[lane];
            const Color c = floor_lane_color(lane, 50);
            DrawLine3D({cx - lane_half, y, 0.0f}, {cx - lane_half, y, sh}, c);
            DrawLine3D({cx + lane_half, y, 0.0f}, {cx + lane_half, y, sh}, c);
        }
    }

    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        const float cx = constants::LANE_X[lane];
        const Color c = floor_lane_color(lane, fp.alpha);

        for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
            const float cz = constants::FLOOR_Y_START
                + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;

            if (j < constants::FLOOR_SHAPE_COUNT - 1) {
                const float next_cz = cz + constants::FLOOR_SHAPE_SPACING;
                DrawLine3D({cx, y, cz + fp.half}, {cx, y, next_cz - fp.half}, c);
            }

            if (lane == 1) {
                const float l = cx - fp.half;
                const float r = cx + fp.half;
                const float t = cz - fp.half;
                const float b = cz + fp.half;
                DrawLine3D({l, y, t}, {r, y, t}, c);
                DrawLine3D({r, y, t}, {r, y, b}, c);
                DrawLine3D({r, y, b}, {l, y, b}, c);
                DrawLine3D({l, y, b}, {l, y, t}, c);
            }

            if (lane == 2) {
                const float apex_x = cx;
                const float apex_z = cz - fp.half;
                const float bl_x = cx - fp.half;
                const float bl_z = cz + fp.half;
                const float br_x = cx + fp.half;
                const float br_z = cz + fp.half;
                DrawLine3D({apex_x, y, apex_z}, {br_x, y, br_z}, c);
                DrawLine3D({br_x, y, br_z}, {bl_x, y, bl_z}, c);
                DrawLine3D({bl_x, y, bl_z}, {apex_x, y, apex_z}, c);
            }
        }
    }
}

void draw_floor_rings(const FloorParams& fp) {
    constexpr int ring_segments = 24;
    constexpr float tau = 6.28318530717958647692f;
    const Color c = floor_lane_color(0, fp.alpha);

    rlBegin(RL_TRIANGLES);
    for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
        const float cz = constants::FLOOR_Y_START
            + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;
        const float cx = constants::LANE_X[0];
        const float inner_r = fp.half - fp.thick;
        const float outer_r = fp.half;

        for (int i = 0; i < ring_segments; ++i) {
            const int next = (i + 1) % ring_segments;
            const float angle1 = (static_cast<float>(i) / static_cast<float>(ring_segments)) * tau;
            const float angle2 = (static_cast<float>(next) / static_cast<float>(ring_segments)) * tau;
            const float dir_x1 = std::cos(angle1);
            const float dir_z1 = std::sin(angle1);
            const float dir_x2 = std::cos(angle2);
            const float dir_z2 = std::sin(angle2);

            const float ox1 = cx + dir_x1 * outer_r;
            const float oz1 = cz + dir_z1 * outer_r;
            const float ox2 = cx + dir_x2 * outer_r;
            const float oz2 = cz + dir_z2 * outer_r;
            const float ix1 = cx + dir_x1 * inner_r;
            const float iz1 = cz + dir_z1 * inner_r;
            const float ix2 = cx + dir_x2 * inner_r;
            const float iz2 = cz + dir_z2 * inner_r;

            rlColor4ub(c.r, c.g, c.b, c.a);
            rlVertex3f(ox1, 0.0f, oz1);
            rlVertex3f(ix1, 0.0f, iz1);
            rlVertex3f(ox2, 0.0f, oz2);
            rlVertex3f(ix1, 0.0f, iz1);
            rlVertex3f(ix2, 0.0f, iz2);
            rlVertex3f(ox2, 0.0f, oz2);
        }
    }
    rlEnd();
}

void draw_floor_beat_lines(const SongState* song, const BeatMap* map) {
    if (!song || !song->playing || song->scroll_speed <= 0.0f) {
        return;
    }

    constexpr float z_min = 0.0f;
    constexpr float z_max = static_cast<float>(constants::SCREEN_H);
    constexpr float x_min = 0.0f;
    constexpr float x_max = static_cast<float>(constants::SCREEN_W);

    const float visible_time_min = song->song_time
        - (z_max - constants::PLAYER_Y) / song->scroll_speed;
    const float visible_time_max = song->song_time
        + (constants::PLAYER_Y - z_min) / song->scroll_speed;

    const auto draw_line_at_time = [&](float beat_time) {
        const float z = constants::PLAYER_Y + (song->song_time - beat_time) * song->scroll_speed;
        if (z < z_min || z > z_max) {
            return;
        }

        const float dist_to_player = constants::PLAYER_Y - z;
        const bool on_beat_line = dist_to_player >= 0.0f && dist_to_player <= 4.0f;
        const uint8_t alpha = on_beat_line ? 220 : 120;
        const uint8_t red = on_beat_line ? 220 : 120;
        const uint8_t green = on_beat_line ? 245 : 170;
        const uint8_t blue = on_beat_line ? 255 : 210;
        DrawLine3D({x_min, 0.0f, z}, {x_max, 0.0f, z}, {red, green, blue, alpha});
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
        if (beat_index < 0) {
            beat_index = 0;
        }
        for (;; ++beat_index) {
            const float beat_time = song->offset + beat_index * song->beat_period;
            if (beat_time > visible_time_max) {
                break;
            }
            draw_line_at_time(beat_time);
        }
    }
}

}  // namespace

void floor_render_system(const entt::registry& reg) {
    const auto& floor_params = reg.ctx().get<FloorParams>();
    const auto* song = reg.ctx().find<SongState>();
    const auto* map = reg.ctx().find<BeatMap>();

    draw_floor_lines(floor_params);
    draw_floor_beat_lines(song, map);
    draw_floor_rings(floor_params);
}
