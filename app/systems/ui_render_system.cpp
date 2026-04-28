#include "all_systems.h"
#include "../components/camera.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/rhythm.h"
#include "../components/game_state.h"
#include "../components/difficulty.h"
#include "../components/song_state.h"
#include "../components/ui_layout_cache.h"
#include "../constants.h"
#include "text_renderer.h"
#include "../components/ui_state.h"
#include "../components/ui_element.h"
#include "ui_loader.h"
#include "ui_source_resolver.h"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

// ── 2D shape drawing (flat, screen-space) ────────────────────

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
            float radius = size * 0.6f;
            constexpr float ANGLE_OFFSET = -90.0f * DEG2RAD;
            for (int i = 0; i < 6; ++i) {
                float a1 = ANGLE_OFFSET + (float)i * 60.0f * DEG2RAD;
                float a2 = ANGLE_OFFSET + (float)(i + 1) * 60.0f * DEG2RAD;
                Vector2 v1h = {cx + radius * cosf(a1), cy + radius * sinf(a1)};
                Vector2 v2h = {cx + radius * cosf(a2), cy + radius * sinf(a2)};
                DrawTriangle({cx, cy}, v2h, v1h, color);
            }
            break;
        }
    }
}

// ── Specialized screen renderers ─────────────────────────────

static void draw_level_select_scene(const TextContext& text_ctx,
                                    const LevelSelectState& lss,
                                    const LevelSelectLayout& layout) {
    if (!layout.valid) return;

    for (int i = 0; i < LevelSelectState::LEVEL_COUNT; ++i) {
        float cy = layout.start_y + static_cast<float>(i) * (layout.card_h + layout.card_gap);
        bool selected = (i == lss.selected_level);
        Color bg     = selected ? layout.selected_bg     : layout.unselected_bg;
        Color border = selected ? layout.selected_border : layout.unselected_border;
        DrawRectangleRounded(
            {layout.card_x, cy, layout.card_w, layout.card_h},
            layout.card_corner_radius, 4, bg);
        DrawRectangleRoundedLinesEx(
            {layout.card_x, cy, layout.card_w, layout.card_h},
            layout.card_corner_radius, 4, 2.0f, border);

        uint8_t title_a = selected ? 255 : 150;
        text_draw(text_ctx, LevelSelectState::LEVELS[i].title,
            layout.card_x + layout.title_offset_x, cy + layout.title_offset_y,
            FontSize::Medium, 255, 255, 255, title_a, TextAlign::Left);

        char track_num[4];
        std::snprintf(track_num, sizeof(track_num), "%d", i + 1);
        text_draw(text_ctx, track_num,
            layout.card_x + layout.card_w - 40.0f, cy + layout.title_offset_y,
            FontSize::Medium, 80, 180, 255, 100, TextAlign::Right);

        if (selected) {
            float diff_y = cy + layout.diff_y_offset;
            for (int dd = 0; dd < 3; ++dd) {
                float bx = layout.dx_start
                    + static_cast<float>(dd) * (layout.diff_btn_w + layout.diff_btn_gap);
                bool active = (dd == lss.selected_difficulty);
                Color bbg     = active ? layout.diff_active_bg     : layout.diff_inactive_bg;
                Color bborder = active ? layout.diff_active_border  : layout.diff_inactive_border;
                Color btc     = active ? layout.diff_active_text    : layout.diff_inactive_text;
                DrawRectangleRounded(
                    {bx, diff_y, layout.diff_btn_w, layout.diff_btn_h},
                    layout.diff_corner_radius, 4, bbg);
                DrawRectangleRoundedLinesEx(
                    {bx, diff_y, layout.diff_btn_w, layout.diff_btn_h},
                    layout.diff_corner_radius, 4, 1.5f, bborder);
                text_draw(text_ctx, LevelSelectState::DIFFICULTY_NAMES[dd],
                    bx + layout.diff_btn_w / 2.0f, diff_y + 10.0f, FontSize::Small,
                    btc.r, btc.g, btc.b, btc.a, TextAlign::Center);
            }
        }
    }
}

static void draw_hud(const entt::registry& reg, const HudLayout& layout) {
    if (!layout.valid) return;

    // Shape buttons
    {
        float btn_w       = layout.btn_w;
        float btn_h       = layout.btn_h;
        float btn_spacing = layout.btn_spacing;
        float btn_y       = layout.btn_y;
        float btn_radius  = btn_w / 2.8f;
        float btn_area_x  = (constants::SCREEN_W - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
        float btn_cy      = btn_y + btn_h / 2.0f;

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
        float max_ring_radius  = btn_radius * layout.approach_ring_max_radius_scale;

        for (int i = 0; i < 3; ++i) {
            float btn_cx = btn_area_x
                + static_cast<float>(i) * (btn_w + btn_spacing) + btn_w / 2.0f;
            bool is_active = (static_cast<int>(active_shape) == i);

            Color bg     = is_active ? layout.active_bg     : layout.inactive_bg;
            Color border = is_active ? layout.active_border : layout.inactive_border;
            DrawCircleV({btn_cx, btn_cy}, btn_radius, bg);
            DrawCircleLinesV({btn_cx, btn_cy}, btn_radius, border);

            auto shape = static_cast<Shape>(i);
            Color icon = is_active ? layout.active_icon : layout.inactive_icon;
            draw_shape_flat(shape, btn_cx, btn_cy, btn_radius * 1.2f, icon);

            if (nearest_dist[i] > 0.0f && nearest_dist[i] < ring_appear_dist) {
                float ratio = (nearest_dist[i] - perfect_dist)
                            / (ring_appear_dist - perfect_dist);
                ratio = Clamp(ratio, 0.0f, 1.0f);
                float ring_r = Lerp(btn_radius, max_ring_radius, ratio);

                Color rc;
                if (nearest_dist[i] <= perfect_dist)
                    rc = layout.ring_perfect;
                else if (ratio < 0.3f)
                    rc = layout.ring_near;
                else
                    rc = layout.ring_far;

                Color ring_color = Fade(rc, (200.0f / 255.0f) * (1.0f - ratio * 0.5f));
                DrawCircleLinesV({btn_cx, btn_cy}, ring_r, ring_color);
                uint8_t ring_alpha = ring_color.a;
                DrawCircleLinesV({btn_cx, btn_cy}, ring_r - 1.0f,
                    {rc.r, rc.g, rc.b, static_cast<uint8_t>(ring_alpha / 2)});
            }
        }
    }

    // Energy bar
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

        float bounce = 0.0f;
        auto* song = reg.ctx().find<SongState>();
        if (song && song->playing && song->beat_period > 0.0f) {
            float phase = std::fmod(song->song_time, song->beat_period) / song->beat_period;
            bounce = 1.0f - phase;
            bounce = bounce * bounce * bounce;
        }

        float flash_ratio = 0.0f;
        if (energy->flash_timer > 0.0f && constants::ENERGY_FLASH_DURATION > 0.0f) {
            flash_ratio = energy->flash_timer / constants::ENERGY_FLASH_DURATION;
            flash_ratio = Clamp(flash_ratio, 0.0f, 1.0f);
        }
        float critical_ratio = 0.0f;
        if (fill < constants::ENERGY_CRITICAL_THRESH && constants::ENERGY_CRITICAL_THRESH > 0.0f) {
            critical_ratio = (constants::ENERGY_CRITICAL_THRESH - fill) / constants::ENERGY_CRITICAL_THRESH;
            critical_ratio = Clamp(critical_ratio, 0.0f, 1.0f);
        }
        float pulse_time = (song && song->playing) ? song->song_time : static_cast<float>(GetTime());
        constexpr float CRITICAL_PULSE_FREQ       = 10.0f;
        constexpr float CRITICAL_PULSE_BASE       = 0.35f;
        constexpr float CRITICAL_PULSE_MODULATION = 0.65f;
        float critical_pulse = 0.5f + 0.5f * std::sin(pulse_time * CRITICAL_PULSE_FREQ);
        float critical_intensity = critical_ratio
            * (CRITICAL_PULSE_BASE + CRITICAL_PULSE_MODULATION * critical_pulse);
        constexpr float BOUNCE_HEIGHT = 5.0f / SEG_COUNT;
        float visible_level = fill + bounce * BOUNCE_HEIGHT;
        if (visible_level > 1.0f) visible_level = 1.0f;

        constexpr int   OVERFLOW_MAX = 5;
        int overflow_segs = 0;
        if (fill >= 0.99f) {
            overflow_segs = static_cast<int>(bounce * OVERFLOW_MAX + 0.5f);
        }

        for (int i = 0; i < overflow_segs; ++i) {
            float seg_y = BAR_TOP - (i + 1) * (SEG_H + SEG_GAP);
            float fade = 1.0f - static_cast<float>(i) / OVERFLOW_MAX;
            DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H},
                Fade({255, 80, 200, 255}, fade * (220.0f / 255.0f)));
        }

        DrawRectangleRec({BAR_X, BAR_TOP, BAR_W, BAR_H}, {15, 15, 25, 180});

        int filled_segs  = static_cast<int>(fill * SEG_COUNT + 0.5f);
        int visible_segs = static_cast<int>(visible_level * SEG_COUNT + 0.5f);

        for (int i = 0; i < SEG_COUNT; ++i) {
            float seg_y = BAR_BOT - (i + 1) * (SEG_H + SEG_GAP) + SEG_GAP;

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
                constexpr float FLASH_RED_BOOST_FACTOR    = 0.45f;
                constexpr float CRITICAL_RED_BOOST_FACTOR = 0.35f;
                constexpr float CRITICAL_COOL_DIM_FACTOR  = 0.40f;
                float red_boost = flash_ratio * FLASH_RED_BOOST_FACTOR
                                + critical_intensity * CRITICAL_RED_BOOST_FACTOR;
                float cool_dim  = critical_intensity * CRITICAL_COOL_DIM_FACTOR;
                uint8_t rr = static_cast<uint8_t>(Clamp(
                    Lerp(static_cast<float>(cr), 255.0f, red_boost),
                    0.0f, 255.0f));
                uint8_t rg = static_cast<uint8_t>(Clamp(
                    Lerp(static_cast<float>(cg), 0.0f, cool_dim),
                    0.0f, 255.0f));
                uint8_t rb = static_cast<uint8_t>(Clamp(
                    Lerp(static_cast<float>(cb), 0.0f, cool_dim),
                    0.0f, 255.0f));
                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {rr, rg, rb, 255});
            } else if (i < visible_segs) {
                float fade = 1.0f - static_cast<float>(i - filled_segs)
                                   / std::max(1.0f, static_cast<float>(visible_segs - filled_segs));
                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H},
                    Fade({cr, cg, cb, 255}, fade * (200.0f / 255.0f)));
            } else {
                DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {35, 35, 50, 50});
            }
        }

        if (flash_ratio > 0.0f) {
            DrawRectangleRec({BAR_X - 1.0f, BAR_TOP - 1.0f, BAR_W + 2.0f, BAR_H + 2.0f},
                Fade({255, 80, 80, 255}, flash_ratio * (140.0f / 255.0f)));
        }

        float border_thickness = 1.0f + critical_intensity * 2.0f;
        uint8_t border_r = static_cast<uint8_t>(80.0f + critical_intensity * 175.0f);
        uint8_t border_g = static_cast<uint8_t>(80.0f - critical_intensity * 40.0f);
        uint8_t border_b = static_cast<uint8_t>(100.0f - critical_intensity * 60.0f);
        uint8_t border_a = static_cast<uint8_t>(140.0f + critical_intensity * 90.0f);
        DrawRectangleLinesEx({BAR_X, BAR_TOP, BAR_W, BAR_H}, border_thickness,
            {border_r, border_g, border_b, border_a});
    }

    // Lane divider
    if (layout.has_lane_divider) {
        DrawLineV({0.0f, layout.lane_divider_y},
                  {constants::SCREEN_W_F, layout.lane_divider_y},
                  layout.lane_divider_color);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// UI render system — 2D overlay pass
// ═════════════════════════════════════════════════════════════════════════════

void ui_render_system(const entt::registry& reg, float /*alpha*/) {
    auto& text_ctx = reg.ctx().get<TextContext>();
    auto& ui = reg.ctx().get<UIState>();
    auto& gs = reg.ctx().get<GameState>();
    auto& ui_cam = reg.ctx().get<UICamera>().cam;

    ClearBackground(BLANK);
    BeginMode2D(ui_cam);

    // Popups
    {
        auto view = reg.view<PopupDisplay, ScreenPosition>();
        for (auto [entity, pd, sp] : view.each()) {
            text_draw(text_ctx, pd.text, sp.x, sp.y, pd.font_size,
                      pd.r, pd.g, pd.b, pd.a, TextAlign::Center);
        }
    }

    // Overlay (if active)
    if (ui.has_overlay) {
        const auto* ovl = reg.ctx().find<OverlayLayout>();
        if (ovl && ovl->valid) {
            DrawRectangleRec({0, 0, constants::SCREEN_W_F, constants::SCREEN_H_F},
                             ovl->dim_color);
        }
    }

    // UI text elements (data-driven from JSON)
    {
        auto view = reg.view<UIElementTag, UIText, Position>();
        const auto* anim_store = reg.storage<UIAnimation>();
        const auto* dyn_store  = reg.storage<UIDynamicText>();
        for (auto [entity, text, pos] : view.each()) {
            Color c = text.color;
            const auto* anim = (anim_store && anim_store->contains(entity)) ? &anim_store->get(entity) : nullptr;
            if (anim) {
                float pulse = (sinf(gs.phase_timer * anim->speed) + 1.0f) / 2.0f;
                c.a = static_cast<uint8_t>(anim->alpha_min +
                    static_cast<int>(pulse * (anim->alpha_max - anim->alpha_min)));
            }
            const char* draw_str = text.text;
            std::string resolved;
            const auto* dyn = (dyn_store && dyn_store->contains(entity)) ? &dyn_store->get(entity) : nullptr;
            if (dyn) {
                auto v = resolve_ui_dynamic_text(reg, dyn->source, dyn->format);
                if (v.has_value()) {
                    resolved = std::move(*v);
                    draw_str = resolved.c_str();
                }
            }
            text_draw(text_ctx, draw_str, pos.x, pos.y, text.font_size,
                      c.r, c.g, c.b, c.a, text.align);
        }
    }

    // UI button elements
    {
        auto view = reg.view<UIElementTag, UIButton, Position>();
        const auto* anim_store = reg.storage<UIAnimation>();
        const auto* dyn_store  = reg.storage<UIDynamicText>();
        for (auto [entity, btn, pos] : view.each()) {
            Color tc = btn.text_color;
            const auto* anim = (anim_store && anim_store->contains(entity)) ? &anim_store->get(entity) : nullptr;
            if (anim) {
                float pulse = (sinf(gs.phase_timer * anim->speed) + 1.0f) / 2.0f;
                tc.a = static_cast<uint8_t>(anim->alpha_min +
                    static_cast<int>(pulse * (anim->alpha_max - anim->alpha_min)));
            }
            DrawRectangleRounded({pos.x, pos.y, btn.w, btn.h}, btn.corner_radius, 4, btn.bg);
            DrawRectangleRoundedLinesEx({pos.x, pos.y, btn.w, btn.h}, btn.corner_radius, 4, 1.5f, btn.border);
            const char* draw_str = btn.text;
            std::string resolved;
            const auto* dyn = (dyn_store && dyn_store->contains(entity)) ? &dyn_store->get(entity) : nullptr;
            if (dyn) {
                auto v = resolve_ui_dynamic_text(reg, dyn->source, dyn->format);
                if (v.has_value()) {
                    resolved = std::move(*v);
                    draw_str = resolved.c_str();
                }
            }
            text_draw(text_ctx, draw_str, pos.x + btn.w / 2.0f, pos.y + 12.0f,
                      btn.font_size, tc.r, tc.g, tc.b, tc.a, TextAlign::Center);
        }
    }

    // UI shape elements
    {
        auto view = reg.view<UIElementTag, UIShape, Position>();
        for (auto [entity, sh, pos] : view.each()) {
            draw_shape_flat(sh.shape, pos.x, pos.y, sh.size, sh.color);
        }
    }

    // Dynamic screens that still need specialized rendering
    switch (ui.active) {
        case ActiveScreen::LevelSelect: {
            auto& lss = reg.ctx().get<LevelSelectState>();
            const auto* layout = reg.ctx().find<LevelSelectLayout>();
            if (layout) draw_level_select_scene(text_ctx, lss, *layout);
            break;
        }
        case ActiveScreen::Gameplay:
        case ActiveScreen::Paused: {
            const auto* hud = reg.ctx().find<HudLayout>();
            if (hud) draw_hud(reg, *hud);
            break;
        }
        default:
            break;
    }

    EndMode2D();
}
