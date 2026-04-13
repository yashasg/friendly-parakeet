#include "all_systems.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/burnout.h"
#include "../components/rhythm.h"
#include "../components/lifetime.h"
#include "../components/game_state.h"
#include "../components/difficulty.h"
#include "../components/particle.h"
#include "../components/audio.h"
#include "../components/song_state.h"
#include "../constants.h"
#include "../text_renderer.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

static void draw_shape(Shape shape, float cx, float cy, float size, Color color) {
    switch (shape) {
        case Shape::Circle: {
            float radius = size / 2.0f;
            DrawCircleV({cx, cy}, radius, color);
            break;
        }
        case Shape::Square: {
            float half = size / 2.0f;
            DrawRectangleRec({cx - half, cy - half, size, size}, color);
            break;
        }
        case Shape::Triangle: {
            float half = size / 2.0f;
            Vector2 v1 = {cx,        cy - half};
            Vector2 v2 = {cx - half, cy + half};
            Vector2 v3 = {cx + half, cy + half};
            DrawTriangle(v1, v2, v3, color);
            break;
        }
        case Shape::Hexagon: {
            // Pointy-top hexagon: first vertex at -90° (straight up)
            float radius = size * 0.6f;
            constexpr float ANGLE_OFFSET = -90.0f * DEG2RAD;
            for (int i = 0; i < 6; ++i) {
                float a1 = ANGLE_OFFSET + (float)i * 60.0f * DEG2RAD;
                float a2 = ANGLE_OFFSET + (float)(i + 1) * 60.0f * DEG2RAD;
                Vector2 v1h = {cx + radius * cosf(a1), cy + radius * sinf(a1)};
                Vector2 v2h = {cx + radius * cosf(a2), cy + radius * sinf(a2)};
                // v2h before v1h for counter-clockwise winding (raylib requirement)
                DrawTriangle({cx, cy}, v2h, v1h, color);
            }
            break;
        }
    }
}

// ── Scene / overlay draw helpers ─────────────────────────────
// Each function draws one viewport-space scene or overlay.
// They read singletons from the registry but don't modify game state.

static void draw_title_scene(const TextContext& text_ctx, const GameState& gs) {
    const float shapes_y  = constants::SCENE_TITLE_SHAPES_Y_N    * constants::SCREEN_H;
    const float shape_sz  = constants::SCENE_TITLE_SHAPES_SIZE_N  * constants::SCREEN_W;
    const float shape_off = constants::SCENE_TITLE_SHAPES_OFFSET_N * constants::SCREEN_W;
    const float cx        = constants::VIEWPORT_CX_N * constants::SCREEN_W;

    Color title_shape_color = {80, 180, 255, 255};
    draw_shape(Shape::Circle, cx - shape_off, shapes_y, shape_sz, title_shape_color);
    draw_shape(Shape::Square, cx,             shapes_y, shape_sz, title_shape_color);
    Color green_color = {100, 255, 100, 255};
    draw_shape(Shape::Triangle, cx + shape_off, shapes_y, shape_sz, green_color);

    text_draw(text_ctx, "SHAPESHIFTER",
        cx, constants::SCENE_TITLE_TEXT_Y_N * constants::SCREEN_H,
        FontSize::Large, 80, 180, 255, 255, TextAlign::Center);

    float pulse = (std::sin(gs.phase_timer * 3.0f) + 1.0f) / 2.0f;
    auto alpha = static_cast<uint8_t>(100 + pulse * 155);
    text_draw(text_ctx, "TAP TO START",
        cx, constants::SCENE_TITLE_PROMPT_Y_N * constants::SCREEN_H,
        FontSize::Medium, 200, 200, 200, alpha, TextAlign::Center);

    #ifndef PLATFORM_WEB
    constexpr float EXIT_W = 200.0f;
    constexpr float EXIT_H = 50.0f;
    constexpr float EXIT_Y = 1050.0f;
    float exit_x = (constants::SCREEN_W - EXIT_W) / 2.0f;
    DrawRectangleRounded({exit_x, EXIT_Y, EXIT_W, EXIT_H}, 0.2f, 6, Color{40, 30, 30, 255});
    DrawRectangleRoundedLinesEx({exit_x, EXIT_Y, EXIT_W, EXIT_H}, 0.2f, 6, 1.5f, Color{100, 60, 60, 255});
    text_draw(text_ctx, "EXIT", cx, EXIT_Y + 12.0f, FontSize::Small, 180, 100, 100, 255, TextAlign::Center);
    #endif
}

static void draw_level_select_scene(const TextContext& text_ctx,
                                    const LevelSelectState& lss,
                                    const GameState& gs) {
    const float cx = constants::VIEWPORT_CX_N * constants::SCREEN_W;

    text_draw(text_ctx, "SELECT LEVEL",
        cx, 80.0f, FontSize::Large, 80, 180, 255, 255, TextAlign::Center);

    constexpr float CARD_START_Y = 200.0f;
    constexpr float CARD_HEIGHT  = 200.0f;
    constexpr float CARD_GAP     = 40.0f;
    constexpr float CARD_X       = 60.0f;
    constexpr float CARD_W       = 600.0f;
    constexpr float DIFF_BTN_W   = 160.0f;
    constexpr float DIFF_BTN_H   = 50.0f;
    constexpr float DIFF_BTN_Y_OFF = 120.0f;
    constexpr float DIFF_BTN_X0  = 100.0f;
    constexpr float DIFF_BTN_GAP = 20.0f;

    for (int i = 0; i < LevelSelectState::LEVEL_COUNT; ++i) {
        float cy = CARD_START_Y + static_cast<float>(i) * (CARD_HEIGHT + CARD_GAP);
        bool selected = (i == lss.selected_level);

        // Card background
        Color card_bg = selected ? Color{40, 50, 80, 255} : Color{25, 25, 40, 255};
        Color border  = selected ? Color{80, 180, 255, 255} : Color{50, 50, 70, 255};
        DrawRectangleRounded({CARD_X, cy, CARD_W, CARD_HEIGHT}, 0.1f, 8, card_bg);
        DrawRectangleRoundedLinesEx({CARD_X, cy, CARD_W, CARD_HEIGHT}, 0.1f, 8, 2.0f, border);

        // Song title
        uint8_t title_a = selected ? 255 : 150;
        text_draw(text_ctx, LevelSelectState::LEVELS[i].title,
            CARD_X + 30.0f, cy + 25.0f, FontSize::Medium,
            255, 255, 255, title_a, TextAlign::Left);

        // Track number
        char track_num[4];
        std::snprintf(track_num, sizeof(track_num), "%d", i + 1);
        text_draw(text_ctx, track_num,
            CARD_X + CARD_W - 40.0f, cy + 25.0f, FontSize::Medium,
            80, 180, 255, 100, TextAlign::Right);

        // Difficulty buttons (only for selected card)
        if (selected) {
            float diff_y = cy + DIFF_BTN_Y_OFF;
            for (int d = 0; d < 3; ++d) {
                float bx = DIFF_BTN_X0 + static_cast<float>(d) * (DIFF_BTN_W + DIFF_BTN_GAP);
                bool active = (d == lss.selected_difficulty);

                Color btn_bg = active ? Color{80, 180, 255, 255} : Color{35, 35, 55, 255};
                Color btn_border = active ? Color{120, 220, 255, 255} : Color{60, 60, 80, 255};
                uint8_t text_r = active ? 0 : 180;
                uint8_t text_g = active ? 0 : 180;
                uint8_t text_b = active ? 0 : 180;

                DrawRectangleRounded({bx, diff_y, DIFF_BTN_W, DIFF_BTN_H}, 0.2f, 6, btn_bg);
                DrawRectangleRoundedLinesEx({bx, diff_y, DIFF_BTN_W, DIFF_BTN_H}, 0.2f, 6, 1.5f, btn_border);
                text_draw(text_ctx, LevelSelectState::DIFFICULTY_NAMES[d],
                    bx + DIFF_BTN_W / 2.0f, diff_y + 10.0f, FontSize::Small,
                    text_r, text_g, text_b, 255, TextAlign::Center);
            }
        }
    }

    // START button
    constexpr float START_BTN_W = 300.0f;
    constexpr float START_BTN_H = 60.0f;
    constexpr float START_BTN_Y = 1050.0f;
    float start_x = (constants::SCREEN_W - START_BTN_W) / 2.0f;
    Color start_bg = {30, 120, 60, 255};
    Color start_border = {60, 200, 100, 255};
    DrawRectangleRounded({start_x, START_BTN_Y, START_BTN_W, START_BTN_H}, 0.2f, 6, start_bg);
    DrawRectangleRoundedLinesEx({start_x, START_BTN_Y, START_BTN_W, START_BTN_H}, 0.2f, 6, 2.0f, start_border);
    float pulse = (std::sin(gs.phase_timer * 3.0f) + 1.0f) / 2.0f;
    auto start_alpha = static_cast<uint8_t>(180 + pulse * 75);
    text_draw(text_ctx, "START",
        cx, START_BTN_Y + 14.0f, FontSize::Medium,
        200, 255, 200, start_alpha, TextAlign::Center);
}

static void draw_hud(entt::registry& reg, const TextContext& text_ctx) {
    auto& score  = reg.ctx().get<ScoreState>();
    auto& config = reg.ctx().get<DifficultyConfig>();

    text_draw_number(text_ctx, score.displayed_score,
        constants::HUD_SCORE_X_N * constants::SCREEN_W,
        constants::HUD_SCORE_Y_N * constants::SCREEN_H,
        FontSize::Medium, 255, 255, 255, 255);

    text_draw_number(text_ctx, score.high_score,
        constants::HUD_SCORE_X_N   * constants::SCREEN_W,
        constants::HUD_HISCORE_Y_N * constants::SCREEN_H,
        FontSize::Small, 150, 150, 150, 180);

    const float btn_w       = constants::BUTTON_W_N       * constants::SCREEN_W;
    const float btn_h       = constants::BUTTON_H_N       * constants::SCREEN_H;
    const float btn_spacing = constants::BUTTON_SPACING_N  * constants::SCREEN_W;
    const float btn_y       = constants::BUTTON_Y_N        * constants::SCREEN_H;
    float btn_radius = btn_w / 2.8f;
    float btn_area_x = (constants::SCREEN_W - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
    float btn_cy     = btn_y + btn_h / 2.0f;

    Shape active_shape = Shape::Hexagon;
    for (auto [e, ps] : reg.view<PlayerTag, PlayerShape>().each()) {
        active_shape = ps.current;
    }

    float nearest_dist[3] = {-1.0f, -1.0f, -1.0f};
    for (auto [e, opos, req] :
         reg.view<ObstacleTag, Position, RequiredShape>(entt::exclude<ScoredTag>).each()) {
        int si = static_cast<int>(req.shape);
        if (si < 0 || si > 2) continue;
        float d = constants::PLAYER_Y - opos.y;
        if (d > 0.0f && (nearest_dist[si] < 0.0f || d < nearest_dist[si])) {
            nearest_dist[si] = d;
        }
    }

    auto* song_hud = reg.ctx().find<SongState>();
    float perfect_dist = song_hud
        ? song_hud->scroll_speed * (song_hud->morph_duration + song_hud->half_window)
        : config.scroll_speed * 0.5f;
    float ring_appear_dist = constants::APPROACH_DIST;
    float max_ring_radius  = btn_radius * 2.0f;

    for (int i = 0; i < 3; ++i) {
        float btn_cx = btn_area_x
            + static_cast<float>(i) * (btn_w + btn_spacing) + btn_w / 2.0f;
        bool is_active = (static_cast<int>(active_shape) == i);

        Color btn_bg = is_active ? Color{60, 60, 100, 255} : Color{30, 30, 50, 200};
        DrawCircleV({btn_cx, btn_cy}, btn_radius, btn_bg);

        Color btn_border = is_active ? Color{120, 180, 255, 255} : Color{60, 60, 80, 255};
        DrawCircleLinesV({btn_cx, btn_cy}, btn_radius, btn_border);

        auto shape = static_cast<Shape>(i);
        Color icon_color = is_active ? Color{200, 230, 255, 255} : Color{100, 100, 120, 200};
        draw_shape(shape, btn_cx, btn_cy, btn_radius * 1.2f, icon_color);

        if (nearest_dist[i] > 0.0f && nearest_dist[i] < ring_appear_dist) {
            float ratio = (nearest_dist[i] - perfect_dist)
                        / (ring_appear_dist - perfect_dist);
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            float ring_r = btn_radius + (max_ring_radius - btn_radius) * ratio;

            uint8_t r_col, g_col, b_col;
            if (nearest_dist[i] <= perfect_dist) {
                r_col = 100; g_col = 255; b_col = 100;
            } else if (ratio < 0.3f) {
                r_col = 180; g_col = 255; b_col = 100;
            } else {
                r_col = 120; g_col = 120; b_col = 180;
            }
            uint8_t ring_alpha = static_cast<uint8_t>(200 * (1.0f - ratio * 0.5f));
            DrawCircleLinesV({btn_cx, btn_cy}, ring_r, {r_col, g_col, b_col, ring_alpha});
            DrawCircleLinesV({btn_cx, btn_cy}, ring_r - 1.0f,
                {r_col, g_col, b_col, static_cast<uint8_t>(ring_alpha / 2)});
        }
    }

    float divider_y = constants::SWIPE_ZONE_SPLIT * constants::SCREEN_H;
    DrawLineV({0, divider_y}, {static_cast<float>(constants::SCREEN_W), divider_y},
              {40, 40, 60, 200});
}

static void draw_end_screen_buttons(const TextContext& text_ctx) {
    const float cx = constants::VIEWPORT_CX_N * constants::SCREEN_W;
    constexpr float BTN_W = 280.0f;
    constexpr float BTN_H = 55.0f;
    constexpr float BTN_GAP = 20.0f;
    float btn_x = (constants::SCREEN_W - BTN_W) / 2.0f;
    float y1 = 900.0f;
    float y2 = y1 + BTN_H + BTN_GAP;

    DrawRectangleRounded({btn_x, y1, BTN_W, BTN_H}, 0.2f, 6, Color{30, 50, 80, 255});
    DrawRectangleRoundedLinesEx({btn_x, y1, BTN_W, BTN_H}, 0.2f, 6, 1.5f, Color{80, 180, 255, 255});
    text_draw(text_ctx, "LEVEL SELECT", cx, y1 + 14.0f, FontSize::Small, 180, 220, 255, 255, TextAlign::Center);

    DrawRectangleRounded({btn_x, y2, BTN_W, BTN_H}, 0.2f, 6, Color{35, 35, 45, 255});
    DrawRectangleRoundedLinesEx({btn_x, y2, BTN_W, BTN_H}, 0.2f, 6, 1.5f, Color{80, 80, 100, 255});
    text_draw(text_ctx, "MAIN MENU", cx, y2 + 14.0f, FontSize::Small, 150, 150, 170, 255, TextAlign::Center);
}

static void draw_game_over_overlay(entt::registry& reg, const TextContext& text_ctx,
                                   const GameState& /*gs*/) {
    auto& score = reg.ctx().get<ScoreState>();
    const float cx = constants::VIEWPORT_CX_N * constants::SCREEN_W;

    DrawRectangleRec({0, 0, float(constants::SCREEN_W), float(constants::SCREEN_H)},
                     {0, 0, 0, 180});

    text_draw(text_ctx, "GAME OVER",
        cx, constants::SCENE_GO_TITLE_Y_N * constants::SCREEN_H,
        FontSize::Large, 255, 80, 80, 255, TextAlign::Center);

    text_draw_number(text_ctx, score.score,
        cx, constants::SCENE_GO_SCORE_Y_N * constants::SCREEN_H,
        FontSize::Medium, 255, 255, 255, 255);

    text_draw_number(text_ctx, score.high_score,
        cx, constants::SCENE_GO_HISCORE_Y_N * constants::SCREEN_H,
        FontSize::Small, 200, 200, 100, 255);

    draw_end_screen_buttons(text_ctx);
}

static void draw_song_complete_overlay(entt::registry& reg, const TextContext& text_ctx,
                                       const GameState& /*gs*/) {
    auto& score = reg.ctx().get<ScoreState>();
    const float cx = constants::VIEWPORT_CX_N  * constants::SCREEN_W;
    const float lx = constants::SCENE_SC_STATS_LX_N * constants::SCREEN_W;
    const float rx = constants::SCENE_SC_STATS_RX_N * constants::SCREEN_W;

    DrawRectangleRec({0, 0, float(constants::SCREEN_W), float(constants::SCREEN_H)},
                     {0, 0, 0, 180});

    text_draw(text_ctx, "SONG COMPLETE",
        cx, constants::SCENE_SC_TITLE_Y_N * constants::SCREEN_H,
        FontSize::Large, 100, 255, 100, 255, TextAlign::Center);

    text_draw(text_ctx, "SCORE",
        cx, constants::SCENE_SC_SLABEL_Y_N * constants::SCREEN_H,
        FontSize::Small, 180, 180, 180, 255, TextAlign::Center);
    text_draw_number(text_ctx, score.score,
        cx, constants::SCENE_SC_SCORE_Y_N * constants::SCREEN_H,
        FontSize::Medium, 255, 255, 255, 255);

    text_draw(text_ctx, "HIGH SCORE",
        cx, constants::SCENE_SC_HSLABEL_Y_N * constants::SCREEN_H,
        FontSize::Small, 180, 180, 180, 255, TextAlign::Center);
    text_draw_number(text_ctx, score.high_score,
        cx, constants::SCENE_SC_HISCORE_Y_N * constants::SCREEN_H,
        FontSize::Medium, 255, 215, 0, 255);

    auto* results = reg.ctx().find<SongResults>();
    if (results) {
        float y = constants::SCENE_SC_TIMING_Y_N * constants::SCREEN_H;
        const float dy = constants::SCENE_SC_TIMING_DY_N * constants::SCREEN_H;
        text_draw(text_ctx, "PERFECT", lx, y, FontSize::Small, 100, 255, 100, 255, TextAlign::Left);
        text_draw_number(text_ctx, results->perfect_count, rx, y, FontSize::Small, 255, 255, 255, 255);
        y += dy;
        text_draw(text_ctx, "GOOD", lx, y, FontSize::Small, 180, 255, 100, 255, TextAlign::Left);
        text_draw_number(text_ctx, results->good_count, rx, y, FontSize::Small, 255, 255, 255, 255);
        y += dy;
        text_draw(text_ctx, "OK", lx, y, FontSize::Small, 255, 255, 100, 255, TextAlign::Left);
        text_draw_number(text_ctx, results->ok_count, rx, y, FontSize::Small, 255, 255, 255, 255);
        y += dy;
        text_draw(text_ctx, "BAD", lx, y, FontSize::Small, 255, 150, 100, 255, TextAlign::Left);
        text_draw_number(text_ctx, results->bad_count, rx, y, FontSize::Small, 255, 255, 255, 255);
        y += dy;
        text_draw(text_ctx, "MISS", lx, y, FontSize::Small, 255, 80, 80, 255, TextAlign::Left);
        text_draw_number(text_ctx, results->miss_count, rx, y, FontSize::Small, 255, 255, 255, 255);
    }

    draw_end_screen_buttons(text_ctx);
}

static void draw_pause_overlay(const TextContext& text_ctx) {
    DrawRectangleRec({0, 0, float(constants::SCREEN_W), float(constants::SCREEN_H)},
                     {0, 0, 0, 160});

    text_draw(text_ctx, "PAUSED",
        constants::VIEWPORT_CX_N * constants::SCREEN_W,
        constants::SCENE_PAUSE_Y_N * constants::SCREEN_H,
        FontSize::Large, 255, 255, 255, 255, TextAlign::Center);
}

void render_system(entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& text_ctx = reg.ctx().get<TextContext>();
    auto& camera = reg.ctx().get<Camera2D>();

    // Clear
    ClearBackground({15, 15, 25, 255});

    // ── WORLD SPACE (Camera2D) ───────────────────────────────
    // A single BeginMode2D/EndMode2D pair wraps ALL world-space draws.
    // Camera is currently an identity transform (zoom=1, target/offset={0,0}),
    // so world coords map 1:1 to the 720×1280 render-texture.  Future camera
    // effects (shake, zoom) only need to modify `camera`; no game-logic
    // positions need to change.
    BeginMode2D(camera);

    // ── Draw lane floors (shaped columns that pulse on beat) ─
    {
        auto* song = reg.ctx().find<SongState>();
        float pulse = 0.0f;
        if (song && song->playing && song->beat_period > 0.0f && song->current_beat >= 0) {
            float time_since_beat = song->song_time
                - (song->offset + static_cast<float>(song->current_beat) * song->beat_period);
            float pulse_t = std::clamp(time_since_beat / constants::FLOOR_PULSE_DECAY, 0.0f, 1.0f);
            float ease = 1.0f - (1.0f - pulse_t) * (1.0f - pulse_t);
            pulse = 1.0f - ease;
        }

        float alpha = constants::FLOOR_ALPHA_REST
            + (constants::FLOOR_ALPHA_PEAK - constants::FLOOR_ALPHA_REST) * pulse;
        float scale = constants::FLOOR_SCALE_REST
            + (constants::FLOOR_SCALE_PEAK - constants::FLOOR_SCALE_REST) * pulse;
        float size  = constants::FLOOR_SHAPE_SIZE * scale;
        float half  = size / 2.0f;
        float thick = constants::FLOOR_OUTLINE_THICK;

        static const Color LANE_SHAPE_COLORS[3] = {
            {80,  200, 255, 255},
            {255, 100, 100, 255},
            {100, 255, 100, 255},
        };

        for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
            float cx = constants::LANE_X[lane];
            Color c  = LANE_SHAPE_COLORS[lane];
            c.a      = static_cast<unsigned char>(alpha);

            for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
                float cy = constants::FLOOR_Y_START + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;

                // Draw connecting segment from this shape's bottom edge to next shape's top edge
                if (j < constants::FLOOR_SHAPE_COUNT - 1) {
                    float next_cy = cy + constants::FLOOR_SHAPE_SPACING;
                    DrawLineEx({cx, cy + half}, {cx, next_cy - half},
                               constants::FLOOR_OUTLINE_THICK, c);
                }

                switch (lane) {
                    case 0:
                        DrawRing({cx, cy}, half - thick, half, 0, 360, 36, c);
                        break;
                    case 1:
                        DrawRectangleLinesEx({cx - half, cy - half, size, size}, thick, c);
                        break;
                    case 2: {
                        Vector2 v1 = {cx,        cy - half};
                        Vector2 v2 = {cx - half, cy + half};
                        Vector2 v3 = {cx + half, cy + half};
                        DrawTriangleLines(v1, v3, v2, c);
                        break;
                    }
                    default: break;
                }
            }
        }
    }

    // ── Draw obstacles ──────────────────────────────────────
    if (gs.phase != GamePhase::Title) {
        auto view = reg.view<ObstacleTag, Position, Obstacle, DrawColor, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {

            Color c = {col.r, col.g, col.b, col.a};

            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    // Full-width gate with shape hole
                    DrawRectangleRec({0, pos.y, pos.x - 50, dsz.h}, c);
                    DrawRectangleRec({pos.x + 50, pos.y,
                        constants::SCREEN_W - pos.x - 50, dsz.h}, c);
                    // Draw shape indicator in the gap
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        Color ghost = {col.r, col.g, col.b, 120};
                        draw_shape(req->shape, pos.x, pos.y + dsz.h / 2, 40, ghost);
                    }
                    break;
                }
                case ObstacleKind::LaneBlock: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - dsz.w / 2;
                                DrawRectangleRec({lx, pos.y, dsz.w, dsz.h}, c);
                            }
                        }
                    }
                    break;
                }
                case ObstacleKind::LowBar:
                case ObstacleKind::HighBar: {
                    DrawRectangleRec({0, pos.y, float(constants::SCREEN_W), dsz.h}, c);
                    break;
                }
                case ObstacleKind::ComboGate: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - 120;
                                DrawRectangleRec({lx, pos.y, 240.0f, dsz.h}, c);
                            }
                        }
                    }
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        Color white_ghost = {255, 255, 255, 180};
                        int open = 1;
                        if (blocked) {
                            for (int i = 0; i < 3; ++i) {
                                if (!((blocked->mask >> i) & 1)) { open = i; break; }
                            }
                        }
                        draw_shape(req->shape,
                            constants::LANE_X[open], pos.y + dsz.h / 2, 30, white_ghost);
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    for (int i = 0; i < 3; ++i) {
                        if (!rlane || i != rlane->lane) {
                            float lx = constants::LANE_X[i] - 120;
                            DrawRectangleRec({lx, pos.y, 240.0f, dsz.h}, c);
                        }
                    }
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req && rlane) {
                        Color white_ghost = {255, 255, 255, 180};
                        draw_shape(req->shape,
                            constants::LANE_X[rlane->lane], pos.y + dsz.h / 2, 30, white_ghost);
                    }
                    break;
                }
            }
        }
    }

    // ── Draw particles ──────────────────────────────────────
    {
        auto view = reg.view<ParticleTag, Position, ParticleData, DrawColor, Lifetime>();
        for (auto [entity, pos, pdata, col, life] : view.each()) {
            float ratio = (life.max_time > 0.0f) ? (life.remaining / life.max_time) : 1.0f;
            float draw_size = pdata.size * ratio;
            float half = draw_size / 2.0f;
            DrawRectangleRec({pos.x - half, pos.y - half, draw_size, draw_size},
                             {col.r, col.g, col.b, col.a});
        }
    }

    // ── Draw timing grade popups ───────────────────────────
    {
        auto view = reg.view<ScorePopup, Position, DrawColor, Lifetime>();
        for (auto [entity, popup, pos, col, life] : view.each()) {
            float alpha_ratio = life.remaining / life.max_time;
            auto popup_alpha = static_cast<uint8_t>(alpha_ratio * 255);

            if (popup.timing_tier <= 3) {
                // Show timing grade text: PERFECT / GOOD / OK / BAD
                const char* grade_text = "BAD";
                FontSize grade_font = FontSize::Small;
                switch (popup.timing_tier) {
                    case 3: grade_text = "PERFECT"; grade_font = FontSize::Medium; break;
                    case 2: grade_text = "GOOD";    grade_font = FontSize::Small;  break;
                    case 1: grade_text = "OK";      grade_font = FontSize::Small;  break;
                    case 0: grade_text = "BAD";     grade_font = FontSize::Small;  break;
                }
                text_draw(text_ctx, grade_text,
                    pos.x, pos.y, grade_font,
                    col.r, col.g, col.b, popup_alpha,
                    TextAlign::Center);
            } else {
                // Non-timed obstacle: show score number
                FontSize popup_font = FontSize::Small;
                text_draw_number(text_ctx, popup.value,
                    pos.x, pos.y, popup_font,
                    col.r, col.g, col.b, popup_alpha);
            }
        }
    }

    // ── Draw player ─────────────────────────────────────────
    {
        auto view = reg.view<PlayerTag, Position, PlayerShape, VerticalState, DrawColor>();
        for (auto [entity, pos, pshape, vstate, col] : view.each()) {
            float draw_y = pos.y + vstate.y_offset;

            float size = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) {
                size *= 0.5f;
            }

            Color pc = {col.r, col.g, col.b, col.a};
            draw_shape(pshape.current, pos.x, draw_y, size, pc);
        }
    }

    EndMode2D();
    // ── VIEWPORT SPACE (HUD + overlays) ─────────────────────

    if (gs.phase == GamePhase::Title) {
        draw_title_scene(text_ctx, gs);
        return;
    }

    if (gs.phase == GamePhase::LevelSelect) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        draw_level_select_scene(text_ctx, lss, gs);
        return;
    }

    if (gs.phase == GamePhase::Playing || gs.phase == GamePhase::Paused) {
        draw_hud(reg, text_ctx);
    }

    if (gs.phase == GamePhase::GameOver) {
        draw_game_over_overlay(reg, text_ctx, gs);
    }

    if (gs.phase == GamePhase::SongComplete) {
        draw_song_complete_overlay(reg, text_ctx, gs);
    }

    if (gs.phase == GamePhase::Paused) {
        draw_pause_overlay(text_ctx);
    }
}
