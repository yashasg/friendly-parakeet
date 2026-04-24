#include "all_systems.h"
#include "../components/shape_mesh.h"
#include "../components/shape_vertices.h"
#include "../components/camera.h"
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
#include "text_renderer.h"
#include "camera_system.h"
#include "../components/ui_state.h"
#include "ui_loader.h"
#include <raylib.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

// World coordinates = game-pixel coordinates (1:1 scale).

static void draw_shape_flat(Shape shape, float cx, float cy, float size, Color color) {
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

using json = nlohmann::json;

static Color json_color(const json& arr) {
    uint8_t a = arr.size() > 3 ? arr[3].get<uint8_t>() : 255;
    return {arr[0].get<uint8_t>(), arr[1].get<uint8_t>(),
            arr[2].get<uint8_t>(), a};
}

static FontSize json_font(const std::string& s) {
    if (s == "large") return FontSize::Large;
    if (s == "small") return FontSize::Small;
    return FontSize::Medium;
}

static TextAlign json_align(const json& el) {
    auto a = el.value("align", "left");
    if (a == "center") return TextAlign::Center;
    if (a == "right")  return TextAlign::Right;
    return TextAlign::Left;
}

static Shape json_shape(const std::string& s) {
    if (s == "square")   return Shape::Square;
    if (s == "triangle") return Shape::Triangle;
    return Shape::Circle;
}

static float el_x(const json& el) {
    return el["x_n"].get<float>() * constants::SCREEN_W;
}

static float el_y(const json& el) {
    return el["y_n"].get<float>() * constants::SCREEN_H;
}

static const json* find_el(const json& screen, const std::string& id) {
    if (!screen.contains("elements")) return nullptr;
    for (auto& el : screen["elements"]) {
        if (el.value("id", "") == id) return &el;
    }
    return nullptr;
}

static void render_text(const json& el, const TextContext& ctx, float timer) {
    Color c = json_color(el["color"]);
    if (el.contains("animation")) {
        auto& anim = el["animation"];
        float pulse = (std::sin(timer * anim["speed"].get<float>()) + 1.0f) / 2.0f;
        auto amin = anim["alpha_range"][0].get<uint8_t>();
        auto amax = anim["alpha_range"][1].get<uint8_t>();
        c.a = static_cast<uint8_t>(amin + static_cast<int>(pulse * (amax - amin)));
    }
    text_draw(ctx, el["text"].get<std::string>().c_str(),
        el_x(el), el_y(el),
        json_font(el.value("font_size", "medium")),
        c.r, c.g, c.b, c.a, json_align(el));
}

static void render_button(const json& el, const TextContext& ctx, float timer) {
    if (el.contains("platform_only")) {
        auto plat = el["platform_only"].get<std::string>();
        #ifdef PLATFORM_WEB
        if (plat == "desktop") return;
        #else
        if (plat == "web") return;
        #endif
    }
    float x = el["x_n"].get<float>() * constants::SCREEN_W;
    float y = el["y_n"].get<float>() * constants::SCREEN_H;
    float w = el["w_n"].get<float>() * constants::SCREEN_W;
    float h = el["h_n"].get<float>() * constants::SCREEN_H;
    float cr = el.value("corner_radius", 0.2f);
    Color bg = json_color(el["bg_color"]);
    Color border = json_color(el["border_color"]);
    Color tc = json_color(el["text_color"]);
    if (el.contains("animation")) {
        auto& anim = el["animation"];
        float pulse = (std::sin(timer * anim["speed"].get<float>()) + 1.0f) / 2.0f;
        auto amin = anim["alpha_range"][0].get<uint8_t>();
        auto amax = anim["alpha_range"][1].get<uint8_t>();
        tc.a = static_cast<uint8_t>(amin + static_cast<int>(pulse * (amax - amin)));
    }
    DrawRectangleRounded({x, y, w, h}, cr, 4, bg);
    DrawRectangleRoundedLinesEx({x, y, w, h}, cr, 4, 1.5f, border);
    float cx = x + w / 2.0f;
    text_draw(ctx, el["text"].get<std::string>().c_str(),
        cx, y + 12.0f,
        json_font(el.value("font_size", "small")),
        tc.r, tc.g, tc.b, tc.a, TextAlign::Center);
}

static void render_shape(const json& el) {
    draw_shape_flat(
        json_shape(el["shape"].get<std::string>()),
        el["x_n"].get<float>() * static_cast<float>(constants::SCREEN_W),
        el["y_n"].get<float>() * static_cast<float>(constants::SCREEN_H),
        el["size_n"].get<float>() * static_cast<float>(constants::SCREEN_W),
        json_color(el["color"]));
}

static void render_elements(const json& screen, const TextContext& ctx, float timer) {
    if (!screen.contains("elements")) return;
    for (auto& el : screen["elements"]) {
        auto type = el.value("type", "");
        if (type == "text")        render_text(el, ctx, timer);
        else if (type == "button") render_button(el, ctx, timer);
        else if (type == "shape")  render_shape(el);
    }
}

static const char* phase_to_screen(GamePhase phase) {
    switch (phase) {
        case GamePhase::Title:        return "title";
        case GamePhase::LevelSelect:  return "level_select";
        case GamePhase::Playing:      return "gameplay";
        case GamePhase::Paused:       return "paused";
        case GamePhase::GameOver:     return "game_over";
        case GamePhase::SongComplete: return "song_complete";
    }
    return "title";
}

static void draw_title_scene(const TextContext& text_ctx, const GameState& gs,
                             const json& screen) {
    render_elements(screen, text_ctx, gs.phase_timer);
}

static void draw_level_select_scene(const TextContext& text_ctx,
                                    const LevelSelectState& lss,
                                    const GameState& gs,
                                    const json& screen) {
    // Static elements (header, start button) rendered generically
    render_elements(screen, text_ctx, gs.phase_timer);

    // Dynamic card list
    auto* cards = find_el(screen, "song_cards");
    if (cards) {
        auto& c = *cards;
        float card_x  = c["x_n"].get<float>() * constants::SCREEN_W;
        float start_y = c["start_y_n"].get<float>() * constants::SCREEN_H;
        float card_w  = c["card_w_n"].get<float>() * constants::SCREEN_W;
        float card_h  = c["card_h_n"].get<float>() * constants::SCREEN_H;
        float gap     = c["card_gap_n"].get<float>() * constants::SCREEN_H;
        float cr      = c.value("corner_radius", 0.1f);
        Color sel_bg     = json_color(c["selected_bg"]);
        Color unsel_bg   = json_color(c["unselected_bg"]);
        Color sel_border = json_color(c["selected_border"]);
        Color unsel_border = json_color(c["unselected_border"]);
        float title_ox = c["title_offset_x_n"].get<float>() * constants::SCREEN_W;
        float title_oy = c["title_offset_y_n"].get<float>() * constants::SCREEN_H;

        for (int i = 0; i < LevelSelectState::LEVEL_COUNT; ++i) {
            float cy = start_y + static_cast<float>(i) * (card_h + gap);
            bool selected = (i == lss.selected_level);
            Color bg     = selected ? sel_bg     : unsel_bg;
            Color border = selected ? sel_border : unsel_border;
            DrawRectangleRounded({card_x, cy, card_w, card_h}, cr, 4, bg);
            DrawRectangleRoundedLinesEx({card_x, cy, card_w, card_h}, cr, 4, 2.0f, border);

            uint8_t title_a = selected ? 255 : 150;
            text_draw(text_ctx, LevelSelectState::LEVELS[i].title,
                card_x + title_ox, cy + title_oy, FontSize::Medium,
                255, 255, 255, title_a, TextAlign::Left);

            char track_num[4];
            std::snprintf(track_num, sizeof(track_num), "%d", i + 1);
            text_draw(text_ctx, track_num,
                card_x + card_w - 40.0f, cy + title_oy, FontSize::Medium,
                80, 180, 255, 100, TextAlign::Right);

            if (selected) {
                auto* diff = find_el(screen, "difficulty_buttons");
                if (diff) {
                    auto& d = *diff;
                    float diff_y   = cy + d["y_offset_n"].get<float>() * constants::SCREEN_H;
                    float dx_start = d["x_start_n"].get<float>() * constants::SCREEN_W;
                    float btn_w    = d["button_w_n"].get<float>() * constants::SCREEN_W;
                    float btn_h    = d["button_h_n"].get<float>() * constants::SCREEN_H;
                    float btn_gap  = d["button_gap_n"].get<float>() * constants::SCREEN_W;
                    float dcr      = d.value("corner_radius", 0.2f);
                    Color a_bg     = json_color(d["active_bg"]);
                    Color a_border = json_color(d["active_border"]);
                    Color a_text   = json_color(d["active_text"]);
                    Color i_bg     = json_color(d["inactive_bg"]);
                    Color i_border = json_color(d["inactive_border"]);
                    Color i_text   = json_color(d["inactive_text"]);

                    for (int dd = 0; dd < 3; ++dd) {
                        float bx = dx_start + static_cast<float>(dd) * (btn_w + btn_gap);
                        bool active = (dd == lss.selected_difficulty);
                        Color bbg = active ? a_bg : i_bg;
                        Color bborder = active ? a_border : i_border;
                        Color btc = active ? a_text : i_text;
                        DrawRectangleRounded({bx, diff_y, btn_w, btn_h}, dcr, 4, bbg);
                        DrawRectangleRoundedLinesEx({bx, diff_y, btn_w, btn_h}, dcr, 4, 1.5f, bborder);
                        text_draw(text_ctx, LevelSelectState::DIFFICULTY_NAMES[dd],
                            bx + btn_w / 2.0f, diff_y + 10.0f, FontSize::Small,
                            btc.r, btc.g, btc.b, btc.a, TextAlign::Center);
                    }
                }
            }
        }
    }
}

static void draw_hud(entt::registry& reg, const TextContext& text_ctx,
                     const json& screen) {
    auto& score  = reg.ctx().get<ScoreState>();

    // Dynamic text: score and high score
    auto* score_el = find_el(screen, "score");
    if (score_el) {
        text_draw_number(text_ctx, score.displayed_score,
            el_x(*score_el), el_y(*score_el),
            json_font(score_el->value("font_size", "medium")),
            255, 255, 255, 255);
    }
    auto* hi_el = find_el(screen, "high_score");
    if (hi_el) {
        Color hc = json_color((*hi_el)["color"]);
        text_draw_number(text_ctx, score.high_score,
            el_x(*hi_el), el_y(*hi_el),
            json_font(hi_el->value("font_size", "small")),
            hc.r, hc.g, hc.b, hc.a);
    }

    // Shape buttons
    auto* sb = find_el(screen, "shape_buttons");
    if (sb) {
        auto& s = *sb;
        float btn_w       = s["button_w_n"].get<float>() * constants::SCREEN_W;
        float btn_h       = s["button_h_n"].get<float>() * constants::SCREEN_H;
        float btn_spacing = s["spacing_n"].get<float>()   * constants::SCREEN_W;
        float btn_y       = s["y_n"].get<float>()         * constants::SCREEN_H;
        float btn_radius  = btn_w / 2.8f;
        float btn_area_x  = (constants::SCREEN_W - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
        float btn_cy      = btn_y + btn_h / 2.0f;
        Color a_bg     = json_color(s["active_bg"]);
        Color i_bg     = json_color(s["inactive_bg"]);
        Color a_border = json_color(s["active_border"]);
        Color i_border = json_color(s["inactive_border"]);
        Color a_icon   = json_color(s["active_icon"]);
        Color i_icon   = json_color(s["inactive_icon"]);

        Shape active_shape = Shape::Hexagon;
        for (auto [e, ps] : reg.view<PlayerTag, PlayerShape>().each()) {
            active_shape = ps.current;
        }

        float nearest_dist[3] = {-1.0f, -1.0f, -1.0f};
        auto& config = reg.ctx().get<DifficultyConfig>();
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
        float max_ring_radius  = btn_radius * s["approach_ring"]["max_radius_scale"].get<float>();
        Color ring_perfect = json_color(s["approach_ring"]["perfect_color"]);
        Color ring_near    = json_color(s["approach_ring"]["near_color"]);
        Color ring_far     = json_color(s["approach_ring"]["far_color"]);

        for (int i = 0; i < 3; ++i) {
            float btn_cx = btn_area_x
                + static_cast<float>(i) * (btn_w + btn_spacing) + btn_w / 2.0f;
            bool is_active = (static_cast<int>(active_shape) == i);

            Color bg     = is_active ? a_bg     : i_bg;
            Color border = is_active ? a_border : i_border;
            DrawCircleV({btn_cx, btn_cy}, btn_radius, bg);
            DrawCircleLinesV({btn_cx, btn_cy}, btn_radius, border);

            auto shape = static_cast<Shape>(i);
            Color icon = is_active ? a_icon : i_icon;
            draw_shape_flat(shape, btn_cx, btn_cy, btn_radius * 1.2f, icon);

            if (nearest_dist[i] > 0.0f && nearest_dist[i] < ring_appear_dist) {
                float ratio = (nearest_dist[i] - perfect_dist)
                            / (ring_appear_dist - perfect_dist);
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                float ring_r = btn_radius + (max_ring_radius - btn_radius) * ratio;

                Color rc;
                if (nearest_dist[i] <= perfect_dist)
                    rc = ring_perfect;
                else if (ratio < 0.3f)
                    rc = ring_near;
                else
                    rc = ring_far;

                uint8_t ring_alpha = static_cast<uint8_t>(200 * (1.0f - ratio * 0.5f));
                DrawCircleLinesV({btn_cx, btn_cy}, ring_r, {rc.r, rc.g, rc.b, ring_alpha});
                DrawCircleLinesV({btn_cx, btn_cy}, ring_r - 1.0f,
                    {rc.r, rc.g, rc.b, static_cast<uint8_t>(ring_alpha / 2)});
            }
        }
    }

    // Energy bar — vertical gradient bar with beat-bounce visualizer
    auto* energy = reg.ctx().find<EnergyState>();
    if (energy) {
        constexpr float BAR_X      = 10.0f;
        constexpr float BAR_W      = 18.0f;
        constexpr float BAR_BOT    = 1000.0f;
        constexpr float BAR_H      = 230.0f;
        constexpr float BAR_TOP    = BAR_BOT - BAR_H;
        constexpr int   SEG_COUNT  = 32;
        constexpr float SEG_GAP    = 1.0f;
        constexpr float SEG_H      = (BAR_H - (SEG_COUNT - 1) * SEG_GAP) / SEG_COUNT;

        float fill = energy->display;
        if (fill < 0.0f) fill = 0.0f;
        if (fill > 1.0f) fill = 1.0f;

        // Beat bounce: on each beat, temporarily push the visible level up
        float bounce = 0.0f;
        auto* song = reg.ctx().find<SongState>();
        if (song && song->playing && song->beat_period > 0.0f) {
            float phase = std::fmod(song->song_time, song->beat_period) / song->beat_period;
            bounce = 1.0f - phase;
            bounce = bounce * bounce * bounce;  // cubic decay
        }

        float flash_ratio = 0.0f;
        if (energy->flash_timer > 0.0f && constants::ENERGY_FLASH_DURATION > 0.0f) {
            flash_ratio = energy->flash_timer / constants::ENERGY_FLASH_DURATION;
            flash_ratio = std::clamp(flash_ratio, 0.0f, 1.0f);
        }
        float critical_ratio = 0.0f;
        if (fill < constants::ENERGY_CRITICAL_THRESH && constants::ENERGY_CRITICAL_THRESH > 0.0f) {
            critical_ratio = (constants::ENERGY_CRITICAL_THRESH - fill) / constants::ENERGY_CRITICAL_THRESH;
            critical_ratio = std::clamp(critical_ratio, 0.0f, 1.0f);
        }
        float pulse_time = (song && song->playing) ? song->song_time : static_cast<float>(GetTime());
        constexpr float CRITICAL_PULSE_FREQ       = 10.0f;
        constexpr float CRITICAL_PULSE_BASE       = 0.35f;
        constexpr float CRITICAL_PULSE_MODULATION = 0.65f;
        float critical_pulse = 0.5f + 0.5f * std::sin(pulse_time * CRITICAL_PULSE_FREQ);
        float critical_intensity = critical_ratio
            * (CRITICAL_PULSE_BASE + CRITICAL_PULSE_MODULATION * critical_pulse);
        // Bounce adds up to ~5 extra segments worth above fill
        constexpr float BOUNCE_HEIGHT = 5.0f / SEG_COUNT;
        float visible_level = fill + bounce * BOUNCE_HEIGHT;
        if (visible_level > 1.0f) visible_level = 1.0f;

        // Pink overflow: bounce segments above the bar when full
        constexpr int   OVERFLOW_MAX = 5;
        int overflow_segs = 0;
        if (fill >= 0.99f) {
            overflow_segs = static_cast<int>(bounce * OVERFLOW_MAX + 0.5f);
        }

        // Overflow segments (pink, drawn above the bar)
        for (int i = 0; i < overflow_segs; ++i) {
            float seg_y = BAR_TOP - (i + 1) * (SEG_H + SEG_GAP);
            float fade = 1.0f - static_cast<float>(i) / OVERFLOW_MAX;
            uint8_t alpha = static_cast<uint8_t>(fade * 220.0f);
            DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {255, 80, 200, alpha});
        }

        // Background
        DrawRectangleRec({BAR_X, BAR_TOP, BAR_W, BAR_H}, {15, 15, 25, 180});

        int filled_segs  = static_cast<int>(fill * SEG_COUNT + 0.5f);
        int visible_segs = static_cast<int>(visible_level * SEG_COUNT + 0.5f);

        for (int i = 0; i < SEG_COUNT; ++i) {
            float seg_y = BAR_BOT - (i + 1) * (SEG_H + SEG_GAP) + SEG_GAP;

            // Gradient: red → yellow → green → cyan → dark blue (bottom to top)
            float t = static_cast<float>(i) / (SEG_COUNT - 1);
            uint8_t cr, cg, cb;
            if (t < 0.25f) {
                float s = t / 0.25f;
                cr = 255; cg = static_cast<uint8_t>(s * 255.0f); cb = 0;
            } else if (t < 0.50f) {
                float s = (t - 0.25f) / 0.25f;
                cr = static_cast<uint8_t>((1.0f - s) * 255.0f); cg = 255; cb = 0;
            } else if (t < 0.75f) {
                float s = (t - 0.50f) / 0.25f;
                cr = 0; cg = 255; cb = static_cast<uint8_t>(s * 255.0f);
            } else {
                float s = (t - 0.75f) / 0.25f;
                cr = 0;
                cg = static_cast<uint8_t>((1.0f - s) * 255.0f);
                cb = static_cast<uint8_t>(255.0f - s * 100.0f);
            }

            if (i < filled_segs) {
                // Solid energy segment
                constexpr float FLASH_RED_BOOST_FACTOR    = 0.45f;
                constexpr float CRITICAL_RED_BOOST_FACTOR = 0.35f;
                constexpr float CRITICAL_COOL_DIM_FACTOR  = 0.40f;
                float red_boost = flash_ratio * FLASH_RED_BOOST_FACTOR
                                + critical_intensity * CRITICAL_RED_BOOST_FACTOR;
                float cool_dim  = critical_intensity * CRITICAL_COOL_DIM_FACTOR;
                uint8_t rr = static_cast<uint8_t>(std::clamp(
                    static_cast<float>(cr) + red_boost * (255.0f - static_cast<float>(cr)),
                    0.0f, 255.0f));
                uint8_t rg = static_cast<uint8_t>(std::clamp(
                    static_cast<float>(cg) * (1.0f - cool_dim),
                    0.0f, 255.0f));
                uint8_t rb = static_cast<uint8_t>(std::clamp(
                    static_cast<float>(cb) * (1.0f - cool_dim),
                    0.0f, 255.0f));
                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {rr, rg, rb, 255});
            } else if (i < visible_segs) {
                // Bounce segment — same gradient color, fades out toward top
                float fade = 1.0f - static_cast<float>(i - filled_segs)
                                   / std::max(1.0f, static_cast<float>(visible_segs - filled_segs));
                uint8_t alpha = static_cast<uint8_t>(fade * 200.0f);
                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {cr, cg, cb, alpha});
            } else {
                // Empty
                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {35, 35, 50, 50});
            }
        }

        if (flash_ratio > 0.0f) {
            uint8_t alpha = static_cast<uint8_t>(std::clamp(flash_ratio * 140.0f, 0.0f, 255.0f));
            DrawRectangleRec({BAR_X - 1.0f, BAR_TOP - 1.0f, BAR_W + 2.0f, BAR_H + 2.0f},
                {255, 80, 80, alpha});
        }

        // Border
        float border_thickness = 1.0f + critical_intensity * 2.0f;
        uint8_t border_r = static_cast<uint8_t>(80.0f + critical_intensity * 175.0f);
        uint8_t border_g = static_cast<uint8_t>(80.0f - critical_intensity * 40.0f);
        uint8_t border_b = static_cast<uint8_t>(100.0f - critical_intensity * 60.0f);
        uint8_t border_a = static_cast<uint8_t>(140.0f + critical_intensity * 90.0f);
        DrawRectangleLinesEx({BAR_X, BAR_TOP, BAR_W, BAR_H}, border_thickness,
            {border_r, border_g, border_b, border_a});
    }

    // Lane divider
    auto* line = find_el(screen, "lane_divider");
    if (line) {
        float div_y = (*line)["y_n"].get<float>() * constants::SCREEN_H;
        Color lc = json_color((*line)["color"]);
        DrawLineV({0, div_y}, {static_cast<float>(constants::SCREEN_W), div_y}, lc);
    }
}

static void draw_overlay(const json& screen) {
    if (screen.contains("overlay")) {
        Color oc = json_color(screen["overlay"]["color"]);
        DrawRectangleRec({0, 0, float(constants::SCREEN_W), float(constants::SCREEN_H)}, oc);
    }
}

static void draw_game_over_overlay(entt::registry& reg, const TextContext& text_ctx,
                                   const GameState& gs, const json& screen) {
    auto& score_state = reg.ctx().get<ScoreState>();
    draw_overlay(screen);
    render_elements(screen, text_ctx, gs.phase_timer);

    auto* sc = find_el(screen, "score");
    if (sc) {
        Color c = json_color((*sc)["color"]);
        text_draw_number(text_ctx, score_state.score,
            el_x(*sc), el_y(*sc),
            json_font(sc->value("font_size", "medium")),
            c.r, c.g, c.b, c.a);
    }
    auto* hi = find_el(screen, "high_score");
    if (hi) {
        Color c = json_color((*hi)["color"]);
        text_draw_number(text_ctx, score_state.high_score,
            el_x(*hi), el_y(*hi),
            json_font(hi->value("font_size", "small")),
            c.r, c.g, c.b, c.a);
    }
}

static void draw_song_complete_overlay(entt::registry& reg, const TextContext& text_ctx,
                                       const GameState& gs, const json& screen) {
    auto& score_state = reg.ctx().get<ScoreState>();
    draw_overlay(screen);
    render_elements(screen, text_ctx, gs.phase_timer);

    auto* sc = find_el(screen, "score");
    if (sc) {
        Color c = json_color((*sc)["color"]);
        text_draw_number(text_ctx, score_state.score,
            el_x(*sc), el_y(*sc),
            json_font(sc->value("font_size", "medium")),
            c.r, c.g, c.b, c.a);
    }
    auto* hi = find_el(screen, "high_score");
    if (hi) {
        Color c = json_color((*hi)["color"]);
        text_draw_number(text_ctx, score_state.high_score,
            el_x(*hi), el_y(*hi),
            json_font(hi->value("font_size", "medium")),
            c.r, c.g, c.b, c.a);
    }

    auto* st = find_el(screen, "timing_stats");
    auto* results = reg.ctx().find<SongResults>();
    if (st && results) {
        float y = (*st)["y_n"].get<float>() * constants::SCREEN_H;
        float dy = (*st)["row_spacing_n"].get<float>() * constants::SCREEN_H;
        float lx = (*st)["label_x_n"].get<float>() * constants::SCREEN_W;
        float rx = (*st)["value_x_n"].get<float>() * constants::SCREEN_W;
        auto fs = json_font(st->value("font_size", "small"));

        const int counts[] = {
            results->perfect_count, results->good_count,
            results->ok_count, results->bad_count, results->miss_count
        };
        auto& rows = (*st)["rows"];
        for (size_t i = 0; i < rows.size() && i < 5; ++i) {
            Color rc = json_color(rows[i]["color"]);
            text_draw(text_ctx, rows[i]["label"].get<std::string>().c_str(),
                lx, y, fs, rc.r, rc.g, rc.b, rc.a, TextAlign::Left);
            text_draw_number(text_ctx, counts[i], rx, y, fs, 255, 255, 255, 255);
            y += dy;
        }
    }
}

static void draw_pause_overlay(const TextContext& text_ctx, const json& screen) {
    draw_overlay(screen);
    render_elements(screen, text_ctx, 0.0f);
}

// ═════════════════════════════════════════════════════════════════════════════
// 3D world draw passes — floor, obstacles, particles, shapes
// ═════════════════════════════════════════════════════════════════════════════

static void draw_floor_lines(entt::registry& reg, const FloorParams& fp) {
    (void)reg;
    static const Color LANE_COLORS[3] = {
        {80, 200, 255, 255}, {255, 100, 100, 255}, {100, 255, 100, 255},
    };

    rlBegin(RL_LINES);

    // Corridor edges
    {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        rlColor4ub(40, 40, 60, 120);
        rlVertex3f(0.0f, 0.0f, 0.0f);  rlVertex3f(0.0f, 0.0f, sh);
        rlVertex3f(sw,   0.0f, 0.0f);  rlVertex3f(sw,   0.0f, sh);
    }

    // Lane guide lines
    {
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        constexpr float lane_half = 120.0f;
        for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
            float cx = constants::LANE_X[lane];
            Color c = LANE_COLORS[lane];
            rlColor4ub(c.r, c.g, c.b, 50);
            rlVertex3f(cx - lane_half, 0.0f, 0.0f);
            rlVertex3f(cx - lane_half, 0.0f, sh);
            rlVertex3f(cx + lane_half, 0.0f, 0.0f);
            rlVertex3f(cx + lane_half, 0.0f, sh);
        }
    }

    // Floor connectors + shape outlines
    for (int lane = 0; lane < constants::LANE_COUNT; ++lane) {
        float cx = constants::LANE_X[lane];
        Color c = LANE_COLORS[lane];
        c.a = fp.alpha;

        for (int j = 0; j < constants::FLOOR_SHAPE_COUNT; ++j) {
            float cz = constants::FLOOR_Y_START
                + static_cast<float>(j) * constants::FLOOR_SHAPE_SPACING;

            if (j < constants::FLOOR_SHAPE_COUNT - 1) {
                float next_cz = cz + constants::FLOOR_SHAPE_SPACING;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3f(cx, 0.0f, cz + fp.half);
                rlVertex3f(cx, 0.0f, next_cz - fp.half);
            }

            if (lane == 1) {
                float l = cx - fp.half, r = cx + fp.half;
                float t = cz - fp.half, b = cz + fp.half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3f(l, 0.0f, t); rlVertex3f(r, 0.0f, t);
                rlVertex3f(r, 0.0f, t); rlVertex3f(r, 0.0f, b);
                rlVertex3f(r, 0.0f, b); rlVertex3f(l, 0.0f, b);
                rlVertex3f(l, 0.0f, b); rlVertex3f(l, 0.0f, t);
            }

            if (lane == 2) {
                float apex_x = cx, apex_z = cz - fp.half;
                float bl_x = cx - fp.half, bl_z = cz + fp.half;
                float br_x = cx + fp.half, br_z = cz + fp.half;
                rlColor4ub(c.r, c.g, c.b, c.a);
                rlVertex3f(apex_x, 0.0f, apex_z); rlVertex3f(br_x, 0.0f, br_z);
                rlVertex3f(br_x, 0.0f, br_z);     rlVertex3f(bl_x, 0.0f, bl_z);
                rlVertex3f(bl_x, 0.0f, bl_z);     rlVertex3f(apex_x, 0.0f, apex_z);
            }
        }
    }

    rlEnd();
}

static void draw_floor_rings(const FloorParams& fp) {
    static const Color LANE0_COLOR = {80, 200, 255, 255};
    Color c = LANE0_COLOR;
    c.a = fp.alpha;

    rlBegin(RL_TRIANGLES);
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

static void draw_world_rects(entt::registry& reg) {
    auto* sm = reg.ctx().find<camera::ShapeMeshes>();
    if (!sm) return;

    constexpr float OBSTACLE_HEIGHT = 20.0f;

    auto draw_slab = [&](float x, float z, float w, float d, float h, Color tint) {
        Matrix mat = {
            w, 0, 0, 0,
            0, h, 0, 0,
            0, 0, d, 0,
            x, 0, z, 1,
        };
        sm->material.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        DrawMesh(sm->slab, sm->material, mat);
    };

    // Multi-slab obstacles (camera_system can't store multiple transforms per entity)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate:
                    draw_slab(0, pos.y, pos.x-50, dsz.h, OBSTACLE_HEIGHT, col);
                    draw_slab(pos.x+50, pos.y, constants::SCREEN_W-pos.x-50, dsz.h, OBSTACLE_HEIGHT, col);
                    break;
                case ObstacleKind::LaneBlock: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked)
                        for (int i = 0; i < 3; ++i)
                            if ((blocked->mask >> i) & 1)
                                draw_slab(constants::LANE_X[i]-dsz.w/2, pos.y, dsz.w, dsz.h, OBSTACLE_HEIGHT, col);
                    break;
                }
                case ObstacleKind::ComboGate: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked)
                        for (int i = 0; i < 3; ++i)
                            if ((blocked->mask >> i) & 1)
                                draw_slab(constants::LANE_X[i]-120, pos.y, 240.0f, dsz.h, OBSTACLE_HEIGHT, col);
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    for (int i = 0; i < 3; ++i)
                        if (!rlane || i != rlane->lane)
                            draw_slab(constants::LANE_X[i]-120, pos.y, 240.0f, dsz.h, OBSTACLE_HEIGHT, col);
                    break;
                }
                default: {
                    // Single-slab obstacles: use ModelTransform from camera_system
                    auto* mt = reg.try_get<ModelTransform>(entity);
                    if (mt && mt->mesh_type == MeshType::Slab) {
                        sm->material.maps[MATERIAL_MAP_DIFFUSE].color = mt->tint;
                        DrawMesh(sm->slab, sm->material, mt->mat);
                    }
                    break;
                }
            }
        }
    }

    // Particles: use ModelTransform from camera_system
    {
        auto view = reg.view<ParticleTag, ModelTransform>();
        for (auto [entity, mt] : view.each()) {
            sm->material.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
            DrawMesh(sm->quad, sm->material, mt.mat);
        }
    }
}

static void draw_gameplay_shapes(entt::registry& reg) {
    auto* sm = reg.ctx().find<camera::ShapeMeshes>();
    if (!sm) return;

    auto draw_shape = [&](Shape shape, float cx, float y_3d, float cz,
                          float sz, Color tint) {
        int idx = static_cast<int>(shape);
        const auto& desc = SHAPE_TABLE[idx];
        float s = sz * desc.radius_scale;
        Matrix mat = {
            s, 0, 0, 0,  0, s, 0, 0,  0, 0, s, 0,
            cx, y_3d, cz, 1.0f,
        };
        sm->material.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        DrawMesh(sm->shapes[idx], sm->material, mat);
    };

    // Ghost shapes (multiple visuals per entity — can't use ModelTransform)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) draw_shape(req->shape, pos.x, 0.0f, pos.y+dsz.h/2, 40, {col.r,col.g,col.b,120});
                    break;
                }
                case ObstacleKind::ComboGate: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        auto* blocked = reg.try_get<BlockedLanes>(entity);
                        int open = 1;
                        if (blocked)
                            for (int i = 0; i < 3; ++i)
                                if (!((blocked->mask >> i) & 1)) { open = i; break; }
                        draw_shape(req->shape, constants::LANE_X[open], 0.0f, pos.y+dsz.h/2, 30, {255,255,255,180});
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* req = reg.try_get<RequiredShape>(entity);
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    if (req && rlane)
                        draw_shape(req->shape, constants::LANE_X[rlane->lane], 0.0f, pos.y+dsz.h/2, 30, {255,255,255,180});
                    break;
                }
                default: break;
            }
        }
    }

    // Player: use ModelTransform from camera_system
    {
        auto view = reg.view<PlayerTag, ModelTransform>();
        for (auto [entity, mt] : view.each()) {
            sm->material.maps[MATERIAL_MAP_DIFFUSE].color = mt.tint;
            DrawMesh(sm->shapes[mt.mesh_index], sm->material, mt.mat);
        }
    }
}

void render_world_system(entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& camera = reg.ctx().get<Camera3D>();

    ClearBackground({15, 15, 25, 255});

    BeginMode3D(camera);

    // Override projection for pixel-scale far plane
    {
        rlDrawRenderBatchActive();
        rlMatrixMode(RL_PROJECTION);
        rlLoadIdentity();
        double near_plane = 10.0;
        double far_plane  = 20000.0;
        double top   = near_plane * tan(static_cast<double>(camera.fovy) * 0.5 * DEG2RAD);
        double right = top * (static_cast<double>(constants::SCREEN_W)
                            / static_cast<double>(constants::SCREEN_H));
        rlFrustum(-right, right, -top, top, near_plane, far_plane);
        rlMatrixMode(RL_MODELVIEW);
    }

    // Floor pulse params
    FloorParams floor_params{};
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
        float alpha_f = constants::FLOOR_ALPHA_REST
            + (constants::FLOOR_ALPHA_PEAK - constants::FLOOR_ALPHA_REST) * pulse;
        float scale = constants::FLOOR_SCALE_REST
            + (constants::FLOOR_SCALE_PEAK - constants::FLOOR_SCALE_REST) * pulse;
        floor_params.size  = constants::FLOOR_SHAPE_SIZE * scale;
        floor_params.half  = floor_params.size / 2.0f;
        floor_params.thick = constants::FLOOR_OUTLINE_THICK;
        floor_params.alpha = static_cast<uint8_t>(alpha_f);
    }

    draw_floor_lines(reg, floor_params);
    draw_floor_rings(floor_params);

    if (gs.phase != GamePhase::Title) {
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
        draw_world_rects(reg);
        draw_gameplay_shapes(reg);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
    }

    EndMode3D();
}

void render_ui_system(entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& text_ctx = reg.ctx().get<TextContext>();
    auto& ui = reg.ctx().get<UIState>();
    auto& ui_cam = reg.ctx().get<Camera2D>();

    ui.load_screen(phase_to_screen(gs.phase));

    ClearBackground(BLANK);

    BeginMode2D(ui_cam);

    // Popups — just draw pre-computed text
    {
        auto view = reg.view<PopupDisplay, ScreenPosition>();
        for (auto [entity, pd, sp] : view.each()) {
            text_draw(text_ctx, pd.text, sp.x, sp.y, pd.font_size,
                      pd.r, pd.g, pd.b, pd.a, TextAlign::Center);
        }
    }

    // Phase-specific UI
    if (gs.phase == GamePhase::Title) {
        draw_title_scene(text_ctx, gs, ui.screen);
    } else if (gs.phase == GamePhase::LevelSelect) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        draw_level_select_scene(text_ctx, lss, gs, ui.screen);
    } else {
        if (gs.phase == GamePhase::Playing || gs.phase == GamePhase::Paused) {
            auto gameplay = ui.screen;
            if (gs.phase == GamePhase::Paused) {
                std::string path = ui.base_dir + "/screens/gameplay.json";
                std::ifstream f(path);
                if (f.is_open()) gameplay = json::parse(f);
            }
            draw_hud(reg, text_ctx, gameplay);
        }

        if (gs.phase == GamePhase::GameOver)
            draw_game_over_overlay(reg, text_ctx, gs, ui.screen);

        if (gs.phase == GamePhase::SongComplete)
            draw_song_complete_overlay(reg, text_ctx, gs, ui.screen);

        if (gs.phase == GamePhase::Paused) {
            ui.load_screen("paused");
            draw_pause_overlay(text_ctx, ui.screen);
            ui.current.clear();
        }
    }

    EndMode2D();
}
