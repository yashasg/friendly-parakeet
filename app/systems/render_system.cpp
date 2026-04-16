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
#include "../perspective.h"
#include "../components/ui_state.h"
#include "../ui_loader.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

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

    // Energy bar — vertical on left side, with beat-reactive visualizer segments
    auto* energy = reg.ctx().find<EnergyState>();
    if (energy) {
        constexpr float BAR_X      = 10.0f;
        constexpr float BAR_W      = 22.0f;
        constexpr float BAR_TOP    = 80.0f;
        constexpr float BAR_BOT    = 1000.0f;
        constexpr float BAR_H      = BAR_BOT - BAR_TOP;
        constexpr int   SEG_COUNT  = 20;
        constexpr float SEG_GAP    = 2.0f;
        constexpr float SEG_H      = (BAR_H - (SEG_COUNT - 1) * SEG_GAP) / SEG_COUNT;

        float fill = energy->display;
        if (fill < 0.0f) fill = 0.0f;
        if (fill > 1.0f) fill = 1.0f;

        // Beat pulse: 0→1 sawtooth that peaks on each beat then decays
        float beat_pulse = 0.0f;
        auto* song = reg.ctx().find<SongState>();
        if (song && song->playing && song->beat_period > 0.0f) {
            float phase = std::fmod(song->song_time, song->beat_period) / song->beat_period;
            beat_pulse = 1.0f - phase;                  // 1.0 at beat, decays to 0
            beat_pulse = beat_pulse * beat_pulse;        // sharper decay curve
        }

        // Background (dark strip)
        DrawRectangleRec({BAR_X, BAR_TOP, BAR_W, BAR_H}, {15, 15, 25, 160});

        // Draw segments bottom-to-top
        int filled_segs = static_cast<int>(fill * SEG_COUNT + 0.5f);
        for (int i = 0; i < SEG_COUNT; ++i) {
            float seg_y = BAR_BOT - (i + 1) * (SEG_H + SEG_GAP) + SEG_GAP;
            bool is_filled = (i < filled_segs);

            if (is_filled) {
                // Base yellow
                uint8_t r = 255;
                uint8_t g = static_cast<uint8_t>(200.0f + 55.0f * fill);
                uint8_t b = 30;
                uint8_t alpha = 255;

                // Beat-reactive: brighten segments near the fill line
                float seg_norm = static_cast<float>(i) / SEG_COUNT;
                float dist_from_top = std::abs(seg_norm - fill);
                float viz_boost = beat_pulse * std::max(0.0f, 1.0f - dist_from_top * 4.0f);
                g = static_cast<uint8_t>(std::min(255.0f, g + 55.0f * viz_boost));
                b = static_cast<uint8_t>(std::min(255.0f, b + 120.0f * viz_boost));

                // Flash effect
                if (energy->flash_timer > 0.0f) {
                    float flash_t = energy->flash_timer / constants::ENERGY_FLASH_DURATION;
                    float pulse = 0.5f + 0.5f * std::sin(flash_t * 20.0f);
                    r = static_cast<uint8_t>(std::min(255.0f, r + (255.0f - r) * pulse * 0.5f));
                    g = static_cast<uint8_t>(std::min(255.0f, g + (255.0f - g) * pulse * 0.5f));
                    alpha = static_cast<uint8_t>(180.0f + 75.0f * pulse);
                }

                // Critical throb
                if (energy->energy < constants::ENERGY_CRITICAL_THRESH && energy->flash_timer <= 0.0f) {
                    float phase_timer = reg.ctx().get<GameState>().phase_timer;
                    float throb = 0.5f + 0.5f * std::sin(phase_timer * 6.0f);
                    r = static_cast<uint8_t>(std::min(255.0f, r + 60.0f * throb));
                    alpha = static_cast<uint8_t>(200.0f + 55.0f * throb);
                }

                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {r, g, b, alpha});
            } else {
                // Empty segment — still show faint beat pulse (visualizer ghost)
                if (beat_pulse > 0.05f) {
                    float seg_norm = static_cast<float>(i) / SEG_COUNT;
                    float dist_from_fill = seg_norm - fill;
                    // Only ghost a few segments above the fill line
                    if (dist_from_fill > 0.0f && dist_from_fill < 0.2f) {
                        float ghost = beat_pulse * (1.0f - dist_from_fill * 5.0f);
                        uint8_t ga = static_cast<uint8_t>(ghost * 120.0f);
                        DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {255, 230, 50, ga});
                    }
                }
                // Dim empty marker
                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {40, 40, 55, 60});
            }
        }

        // Thin border
        DrawRectangleLinesEx({BAR_X, BAR_TOP, BAR_W, BAR_H}, 1.0f, {80, 80, 100, 140});
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

void render_system(entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& text_ctx = reg.ctx().get<TextContext>();
    auto& camera = reg.ctx().get<Camera2D>();
    auto& ui = reg.ctx().get<UIState>();

    // Load the right screen JSON when phase changes
    ui.load_screen(phase_to_screen(gs.phase));

    // Clear
    ClearBackground({15, 15, 25, 255});

    // ── WORLD SPACE (Camera2D) ───────────────────────────────
    // A single BeginMode2D/EndMode2D pair wraps ALL world-space draws.
    // Camera is currently an identity transform (zoom=1, target/offset={0,0}),
    // so world coords map 1:1 to the 720×1280 render-texture.  Future camera
    // effects (shake, zoom) only need to modify `camera`; no game-logic
    // positions need to change.
    BeginMode2D(camera);

    // ── Compute floor pulse params (shared across GPU batches) ─
    perspective::FloorParams floor_params{};
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

    // ── 4 GPU batches sorted by layer then primitive type ───
    // Background (floor) renders first, gameplay on top.
    perspective::flush_floor_lines(reg, floor_params);   // Pass 1: floor lines
    perspective::flush_floor_rings(floor_params);         // Pass 2: floor circles
    if (gs.phase != GamePhase::Title) {
        perspective::flush_world_rects(reg);              // Pass 3: obstacle + particle rects
        perspective::flush_gameplay_tris(reg);            // Pass 4: ghost shapes + player
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
                Vector2 pp = perspective::project(pos.x, pos.y);
                text_draw(text_ctx, grade_text,
                    pp.x, pp.y, grade_font,
                    col.r, col.g, col.b, popup_alpha,
                    TextAlign::Center);
            } else {
                // Non-timed obstacle: show score number
                FontSize popup_font = FontSize::Small;
                Vector2 pp = perspective::project(pos.x, pos.y);
                text_draw_number(text_ctx, popup.value,
                    pp.x, pp.y, popup_font,
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
            perspective::draw_shape(pshape.current, pos.x, draw_y, size, pc);
        }
    }

    EndMode2D();
    // ── VIEWPORT SPACE (HUD + overlays) ─────────────────────

    if (gs.phase == GamePhase::Title) {
        draw_title_scene(text_ctx, gs, ui.screen);
        return;
    }

    if (gs.phase == GamePhase::LevelSelect) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        draw_level_select_scene(text_ctx, lss, gs, ui.screen);
        return;
    }

    if (gs.phase == GamePhase::Playing || gs.phase == GamePhase::Paused) {
        // HUD needs the gameplay screen, not the paused screen
        auto gameplay = ui.screen;
        if (gs.phase == GamePhase::Paused) {
            // Temporarily load gameplay screen for HUD
            std::string path = ui.base_dir + "/screens/gameplay.json";
            std::ifstream f(path);
            if (f.is_open()) gameplay = json::parse(f);
        }
        draw_hud(reg, text_ctx, gameplay);
    }

    if (gs.phase == GamePhase::GameOver) {
        draw_game_over_overlay(reg, text_ctx, gs, ui.screen);
    }

    if (gs.phase == GamePhase::SongComplete) {
        draw_song_complete_overlay(reg, text_ctx, gs, ui.screen);
    }

    if (gs.phase == GamePhase::Paused) {
        ui.load_screen("paused");
        draw_pause_overlay(text_ctx, ui.screen);
        // Reset so next frame re-evaluates
        ui.current.clear();
    }
}
