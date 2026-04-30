// Gameplay HUD screen controller.

#include "../../components/game_state.h"
#include "../../components/input_events.h"
#include "../../components/obstacle.h"
#include "../../components/player.h"
#include "../../components/scoring.h"
#include "../../components/song_state.h"
#include "../../components/transform.h"
#include "../../components/ui_layout_cache.h"
#include "../../constants.h"
#include "screen_controller_base.h"
#include "gameplay_hud_screen_controller.h"
#include <entt/entt.hpp>
#include <array>
#include <cmath>
#include <cstdio>

#include <raygui.h>
#include "../generated/gameplay_hud_layout.h"
#include <raymath.h>

namespace {

using GameplayHudController = RGuiScreenController<GameplayHudLayoutState,
                                                    &GameplayHudLayout_Init,
                                                    &GameplayHudLayout_Render>;
GameplayHudController gameplay_hud_controller;

void draw_shape_flat(Shape shape, float cx, float cy, float size, Color color) {
    switch (shape) {
        case Shape::Circle: {
            DrawCircleV({cx, cy}, size / 2.0f, color);
            break;
        }
        case Shape::Square: {
            float half = size / 2.0f;
            DrawRectangleRec({cx - half, cy - half, size, size}, color);
            break;
        }
        case Shape::Triangle: {
            float half = size / 2.0f;
            DrawTriangle({cx, cy - half}, {cx - half, cy + half}, {cx + half, cy + half}, color);
            break;
        }
        case Shape::Hexagon: {
            float radius = size * 0.6f;
            constexpr float angle_offset = -90.0f * DEG2RAD;
            for (int i = 0; i < 6; ++i) {
                float a1 = angle_offset + static_cast<float>(i) * 60.0f * DEG2RAD;
                float a2 = angle_offset + static_cast<float>(i + 1) * 60.0f * DEG2RAD;
                Vector2 v1h = {cx + radius * std::cos(a1), cy + radius * std::sin(a1)};
                Vector2 v2h = {cx + radius * std::cos(a2), cy + radius * std::sin(a2)};
                DrawTriangle({cx, cy}, v2h, v1h, color);
            }
            break;
        }
    }
}

HudLayout fallback_hud_layout() {
    HudLayout layout{};
    layout.btn_w = constants::BUTTON_W_N * constants::SCREEN_W_F;
    layout.btn_h = constants::BUTTON_H_N * constants::SCREEN_H_F;
    layout.btn_spacing = constants::BUTTON_SPACING_N * constants::SCREEN_W_F;
    layout.btn_y = constants::BUTTON_Y_N * constants::SCREEN_H_F;
    layout.active_bg = {60, 60, 100, 255};
    layout.inactive_bg = {30, 30, 50, 200};
    layout.active_border = {120, 180, 255, 255};
    layout.inactive_border = {60, 60, 80, 255};
    layout.active_icon = {200, 230, 255, 255};
    layout.inactive_icon = {100, 100, 120, 200};
    layout.approach_ring_max_radius_scale = 2.0f;
    layout.ring_perfect = {100, 255, 100, 255};
    layout.ring_near = {180, 255, 100, 255};
    layout.ring_far = {120, 120, 180, 255};
    layout.has_lane_divider = true;
    layout.lane_divider_y = 1120.0f;
    layout.lane_divider_color = {40, 40, 60, 200};
    layout.valid = true;
    return layout;
}

const HudLayout& resolved_hud_layout(const entt::registry& reg) {
    const auto* hud = reg.ctx().find<HudLayout>();
    if (hud && hud->valid) return *hud;
    static const HudLayout fallback = fallback_hud_layout();
    return fallback;
}

Rectangle shape_slot_bounds(const GameplayHudLayoutState& state, GameplayHudShapeSlot slot) {
    switch (slot) {
        case GameplayHudShapeSlot::Circle:
            return GameplayHudLayout_CircleButtonBounds(&state);
        case GameplayHudShapeSlot::Square:
            return GameplayHudLayout_SquareButtonBounds(&state);
        case GameplayHudShapeSlot::Triangle:
            return GameplayHudLayout_TriangleButtonBounds(&state);
    }
    return GameplayHudLayout_CircleButtonBounds(&state);
}

void render_shape_buttons(const entt::registry& reg,
                          const HudLayout& layout,
                          const GameplayHudLayoutState& ui_state) {
    struct ButtonVisual {
        Shape shape;
        float cx;
        float cy;
    };
    std::array<ButtonVisual, 3> buttons{};
    const auto circle_bounds = shape_slot_bounds(ui_state, GameplayHudShapeSlot::Circle);
    const auto square_bounds = shape_slot_bounds(ui_state, GameplayHudShapeSlot::Square);
    const auto triangle_bounds = shape_slot_bounds(ui_state, GameplayHudShapeSlot::Triangle);
    buttons[0] = ButtonVisual{Shape::Circle,
                              circle_bounds.x + circle_bounds.width * 0.5f,
                              circle_bounds.y + circle_bounds.height * 0.5f};
    buttons[1] = ButtonVisual{Shape::Square,
                              square_bounds.x + square_bounds.width * 0.5f,
                              square_bounds.y + square_bounds.height * 0.5f};
    buttons[2] = ButtonVisual{Shape::Triangle,
                              triangle_bounds.x + triangle_bounds.width * 0.5f,
                              triangle_bounds.y + triangle_bounds.height * 0.5f};

    float btn_radius = circle_bounds.width / 2.8f;

    Shape active_shape = Shape::Hexagon;
    for (auto [entity, player_shape] : reg.view<PlayerTag, PlayerShape>().each()) {
        active_shape = player_shape.current;
        (void)entity;
    }

    std::array<float, 4> nearest_dist = {-1.0f, -1.0f, -1.0f, -1.0f};
    for (auto [entity, obstacle_pos, required_shape] :
         reg.view<ObstacleTag, Position, RequiredShape>(entt::exclude<ScoredTag>).each()) {
        (void)entity;
        int shape_index = static_cast<int>(required_shape.shape);
        if (shape_index < 0 || shape_index >= static_cast<int>(nearest_dist.size())) continue;
        float dist = constants::PLAYER_Y - obstacle_pos.y;
        if (dist > 0.0f && (nearest_dist[shape_index] < 0.0f || dist < nearest_dist[shape_index])) {
            nearest_dist[shape_index] = dist;
        }
    }

    auto* song_state = reg.ctx().find<SongState>();
    float perfect_dist = song_state
        ? song_state->scroll_speed * (song_state->morph_duration + song_state->half_window)
        : constants::BASE_SCROLL_SPEED * 0.5f;
    float ring_appear_dist = constants::APPROACH_DIST;
    float max_ring_radius = btn_radius * layout.approach_ring_max_radius_scale;

    for (const auto& button : buttons) {
        bool is_active = (active_shape == button.shape);
        Color bg = is_active ? layout.active_bg : layout.inactive_bg;
        Color border = is_active ? layout.active_border : layout.inactive_border;
        Color icon = is_active ? layout.active_icon : layout.inactive_icon;
        DrawCircleV({button.cx, button.cy}, btn_radius, bg);
        DrawCircleLinesV({button.cx, button.cy}, btn_radius, border);
        draw_shape_flat(button.shape, button.cx, button.cy, btn_radius * 1.2f, icon);

        int shape_index = static_cast<int>(button.shape);
        if (shape_index < 0 || shape_index >= static_cast<int>(nearest_dist.size())) continue;
        if (nearest_dist[shape_index] <= 0.0f || nearest_dist[shape_index] >= ring_appear_dist) continue;
        float ratio = (nearest_dist[shape_index] - perfect_dist) / (ring_appear_dist - perfect_dist);
        ratio = Clamp(ratio, 0.0f, 1.0f);
        float ring_r = Lerp(btn_radius, max_ring_radius, ratio);

        Color base = (nearest_dist[shape_index] <= perfect_dist) ? layout.ring_perfect
                   : ((ratio < 0.3f) ? layout.ring_near : layout.ring_far);
        Color ring_color = Fade(base, (200.0f / 255.0f) * (1.0f - ratio * 0.5f));
        DrawCircleLinesV({button.cx, button.cy}, ring_r, ring_color);
        DrawCircleLinesV({button.cx, button.cy}, ring_r - 1.0f,
            {base.r, base.g, base.b, static_cast<unsigned char>(ring_color.a / 2)});
    }
}

void render_energy_bar(const entt::registry& reg, const EnergyState& energy) {
    constexpr float BAR_X = 16.0f;
    constexpr float BAR_W = 14.0f;
    constexpr float BAR_BOT = 965.0f;
    constexpr float BAR_H = 180.0f;
    constexpr float BAR_TOP = BAR_BOT - BAR_H;
    constexpr int SEG_COUNT = 32;
    constexpr float SEG_GAP = 1.0f;
    constexpr float SEG_H = (BAR_H - (SEG_COUNT - 1) * SEG_GAP) / SEG_COUNT;

    float fill = Clamp(energy.display, 0.0f, 1.0f);

    float bounce = 0.0f;
    auto* song = reg.ctx().find<SongState>();
    if (song && song->playing && song->beat_period > 0.0f) {
        float phase = std::fmod(song->song_time, song->beat_period) / song->beat_period;
        bounce = 1.0f - phase;
        bounce = bounce * bounce * bounce;
    }

    float flash_ratio = 0.0f;
    if (energy.flash_timer > 0.0f && constants::ENERGY_FLASH_DURATION > 0.0f) {
        flash_ratio = Clamp(energy.flash_timer / constants::ENERGY_FLASH_DURATION, 0.0f, 1.0f);
    }

    float critical_ratio = 0.0f;
    if (fill < constants::ENERGY_CRITICAL_THRESH && constants::ENERGY_CRITICAL_THRESH > 0.0f) {
        critical_ratio = Clamp((constants::ENERGY_CRITICAL_THRESH - fill)
            / constants::ENERGY_CRITICAL_THRESH, 0.0f, 1.0f);
    }

    float pulse_time = (song && song->playing) ? song->song_time : static_cast<float>(GetTime());
    float critical_pulse = 0.5f + 0.5f * std::sin(pulse_time * 10.0f);
    float critical_intensity = critical_ratio * (0.35f + 0.65f * critical_pulse);
    float visible_level = std::min(fill + bounce * (5.0f / SEG_COUNT), 1.0f);

    int overflow_segs = 0;
    if (fill >= 0.99f) overflow_segs = static_cast<int>(bounce * 5.0f + 0.5f);
    for (int i = 0; i < overflow_segs; ++i) {
        float seg_y = BAR_TOP - (i + 1) * (SEG_H + SEG_GAP);
        float fade = 1.0f - static_cast<float>(i) / 5.0f;
        DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H},
            Fade({255, 80, 200, 255}, fade * (220.0f / 255.0f)));
    }

    DrawRectangleRec({BAR_X, BAR_TOP, BAR_W, BAR_H}, {15, 15, 25, 180});

    int filled_segs = static_cast<int>(fill * SEG_COUNT + 0.5f);
    int visible_segs = static_cast<int>(visible_level * SEG_COUNT + 0.5f);

    for (int i = 0; i < SEG_COUNT; ++i) {
        float seg_y = BAR_BOT - (i + 1) * (SEG_H + SEG_GAP) + SEG_GAP;
        float t = static_cast<float>(i) / (SEG_COUNT - 1);
        unsigned char cr = 0, cg = 0, cb = 0;
        if (t < 0.33f) {
            float s = t / 0.33f;
            cr = 255;
            cg = static_cast<unsigned char>(80.0f + s * 175.0f);
            cb = static_cast<unsigned char>(30.0f + s * 30.0f);
        } else if (t < 0.66f) {
            float s = (t - 0.33f) / 0.33f;
            cr = static_cast<unsigned char>(255.0f - s * 255.0f);
            cg = 255;
            cb = static_cast<unsigned char>(60.0f + s * 195.0f);
        } else {
            float s = (t - 0.66f) / 0.34f;
            cr = static_cast<unsigned char>(40.0f + s * 120.0f);
            cg = static_cast<unsigned char>(255.0f - s * 120.0f);
            cb = 255;
        }

        if (i < filled_segs) {
            float red_boost = flash_ratio * 0.45f + critical_intensity * 0.35f;
            float cool_dim = critical_intensity * 0.40f;
            unsigned char rr = static_cast<unsigned char>(
                Clamp(Lerp(static_cast<float>(cr), 255.0f, red_boost), 0.0f, 255.0f));
            unsigned char rg = static_cast<unsigned char>(
                Clamp(Lerp(static_cast<float>(cg), 0.0f, cool_dim), 0.0f, 255.0f));
            unsigned char rb = static_cast<unsigned char>(
                Clamp(Lerp(static_cast<float>(cb), 0.0f, cool_dim), 0.0f, 255.0f));
            DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {rr, rg, rb, 255});
        } else if (i < visible_segs) {
            float fade = 1.0f - static_cast<float>(i - filled_segs)
                / std::max(1.0f, static_cast<float>(visible_segs - filled_segs));
            DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, Fade({cr, cg, cb, 255}, fade * (200.0f / 255.0f)));
        } else {
            DrawRectangleRec({BAR_X, seg_y, BAR_W, SEG_H}, {35, 35, 50, 50});
        }
    }

    if (flash_ratio > 0.0f) {
        DrawRectangleRec({BAR_X - 1.0f, BAR_TOP - 1.0f, BAR_W + 2.0f, BAR_H + 2.0f},
            Fade({255, 80, 80, 255}, flash_ratio * (140.0f / 255.0f)));
    }

    float border_thickness = 1.0f + critical_intensity * 2.0f;
    unsigned char border_r = static_cast<unsigned char>(80.0f + critical_intensity * 175.0f);
    unsigned char border_g = static_cast<unsigned char>(80.0f - critical_intensity * 40.0f);
    unsigned char border_b = static_cast<unsigned char>(100.0f - critical_intensity * 60.0f);
    unsigned char border_a = static_cast<unsigned char>(140.0f + critical_intensity * 90.0f);
    DrawRectangleLinesEx({BAR_X, BAR_TOP, BAR_W, BAR_H}, border_thickness,
        {border_r, border_g, border_b, border_a});
}

} // anonymous namespace

void init_gameplay_hud_screen_ui() {
    gameplay_hud_controller.init();
}

Rectangle gameplay_hud_shape_input_bounds(GameplayHudShapeSlot slot) {
    static const GameplayHudLayoutState geometry_state = GameplayHudLayout_Init();
    return shape_slot_bounds(geometry_state, slot);
}

void gameplay_hud_apply_button_presses(entt::registry& reg,
                                       bool pause_pressed,
                                       bool circle_pressed,
                                       bool square_pressed,
                                       bool triangle_pressed) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::Playing) return;

    auto& disp = reg.ctx().get<entt::dispatcher>();
    bool any_shape_press = false;
    if (circle_pressed) {
        disp.enqueue<ButtonPressEvent>(
            {ButtonPressKind::Shape, Shape::Circle, MenuActionKind::Confirm, 0});
        any_shape_press = true;
    }
    if (square_pressed) {
        disp.enqueue<ButtonPressEvent>(
            {ButtonPressKind::Shape, Shape::Square, MenuActionKind::Confirm, 0});
        any_shape_press = true;
    }
    if (triangle_pressed) {
        disp.enqueue<ButtonPressEvent>(
            {ButtonPressKind::Shape, Shape::Triangle, MenuActionKind::Confirm, 0});
        any_shape_press = true;
    }
    if (any_shape_press) {
        disp.update<ButtonPressEvent>();
    }

    if (pause_pressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Paused;
    }
}

void render_gameplay_hud_screen_ui(entt::registry& reg) {
    auto& state = gameplay_hud_controller.state();
    auto* score = reg.ctx().find<ScoreState>();
    auto* energy = reg.ctx().find<EnergyState>();
    const auto& hud_layout = resolved_hud_layout(reg);

    int saved_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);

    // Render score at top-left
    if (score) {
        GuiSetStyle(DEFAULT, TEXT_SIZE, 28);
        char score_text[32];
        std::snprintf(score_text, sizeof(score_text), "%d", score->displayed_score);
        GuiLabel((Rectangle){ 80, 20, 200, 40 }, score_text);
        
        // High score below
        GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
        char high_score_text[32];
        std::snprintf(high_score_text, sizeof(high_score_text), "BEST: %d", score->high_score);
        GuiSetAlpha(0.7f);
        GuiLabel((Rectangle){ 80, 50, 200, 30 }, high_score_text);
        GuiSetAlpha(1.0f);
    }

    render_shape_buttons(reg, hud_layout, state);

    if (energy) render_energy_bar(reg, *energy);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetAlpha(0.8f);
    GuiLabel((Rectangle){ 10, 740, 90, 30 }, "ENERGY");
    GuiSetAlpha(1.0f);

    if (hud_layout.has_lane_divider) {
        DrawLineV({0.0f, hud_layout.lane_divider_y},
                  {constants::SCREEN_W_F, hud_layout.lane_divider_y},
                  hud_layout.lane_divider_color);
    }

    // Render Pause button from generated layout state.
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
    gameplay_hud_controller.render();

    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);

    gameplay_hud_apply_button_presses(reg,
                                      state.PauseButtonPressed,
                                      state.CircleButtonPressed,
                                      state.SquareButtonPressed,
                                      state.TriangleButtonPressed);
}
