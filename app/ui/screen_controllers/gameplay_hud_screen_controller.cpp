// Gameplay HUD screen controller.

#include "../../components/game_state.h"
#include "../../components/energy_bar.h"
#include "../../systems/input.h"
#include "../../systems/input_events.h"
#include "../../components/obstacle.h"
#include "../../components/player.h"
#include "../../components/scoring.h"
#include "../../components/song_state.h"
#include "../../components/transform.h"
#include "../../constants.h"
#include "../../entities/settings.h"
#include "../../util/rhythm_math.h"
#include "../../util/shape_lane_mapping.h"
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

// Per-shape flat draw functions (issue #1202/#1204). Each former
// `switch (shape)` case becomes its own row in a fn-ptr column indexed by
// `shape_index(shape)`; the dispatcher reads the column directly rather than
// branching on the discriminator. Same mechanic as ProceduralWave PR #1254.
void draw_circle_flat(float cx, float cy, float size, Color color) {
    DrawCircleV({cx, cy}, size / 2.0f, color);
}
void draw_square_flat(float cx, float cy, float size, Color color) {
    const float half = size / 2.0f;
    DrawRectangleRec({cx - half, cy - half, size, size}, color);
}
void draw_triangle_flat(float cx, float cy, float size, Color color) {
    const float half = size / 2.0f;
    DrawTriangle({cx, cy - half}, {cx - half, cy + half}, {cx + half, cy + half}, color);
}
void draw_hexagon_flat(float cx, float cy, float size, Color color) {
    DrawPoly({cx, cy}, 6, size * 0.6f, -90.0f, color);
}

using ShapeFlatDrawFn = void (*)(float, float, float, Color);
constexpr std::array<ShapeFlatDrawFn, kShapeCount> kShapeFlatDrawFns{
    &draw_circle_flat,
    &draw_square_flat,
    &draw_triangle_flat,
    &draw_hexagon_flat,
};

void draw_shape_flat(Shape shape, float cx, float cy, float size, Color color) {
    const int idx = shape_index(shape);
    if (idx < 0) return;
    kShapeFlatDrawFns[static_cast<size_t>(idx)](cx, cy, size, color);
}

struct GameplayHudVisualStyle {
    Color active_bg;
    Color inactive_bg;
    Color active_border;
    Color inactive_border;
    Color active_icon;
    Color inactive_icon;
    float approach_ring_max_radius_scale;
    Color lane_divider_color;
};

constexpr GameplayHudVisualStyle kGameplayHudVisualStyle{
    {60, 60, 100, 255},
    {30, 30, 50, 200},
    {120, 180, 255, 255},
    {60, 60, 80, 255},
    {200, 230, 255, 255},
    {100, 100, 120, 200},
    2.0f,
    {40, 40, 60, 200},
};

constexpr float kLaneDividerOffsetFromShapeRow = 20.0f;

struct ApproachRingEnvelope {
    float radius = 0.0f;
    float alpha_scale = 0.0f;
};

ApproachRingEnvelope approach_ring_envelope(float ratio,
                                            float btn_radius,
                                            float max_ring_radius,
                                            bool reduce_motion,
                                            float near_threshold = 0.3f) {
    ApproachRingEnvelope out{};
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    if (reduce_motion) {
        if (ratio > near_threshold) return out;
        out.radius = max_ring_radius;
        out.alpha_scale = 1.0f;
        return out;
    }
    out.radius = btn_radius + (max_ring_radius - btn_radius) * ratio;
    out.alpha_scale = 1.0f - ratio * 0.5f;
    return out;
}

void render_shape_buttons(const entt::registry& reg,
                          const GameplayHudVisualStyle& style,
                          const GameplayHudLayoutState& ui_state) {
    struct ButtonVisual {
        Shape shape;
        float cx;
        float cy;
    };
    std::array<ButtonVisual, 3> buttons{};
    const auto circle_bounds = GameplayHudLayout_CircleButtonBounds(&ui_state);
    const auto square_bounds = GameplayHudLayout_SquareButtonBounds(&ui_state);
    const auto triangle_bounds = GameplayHudLayout_TriangleButtonBounds(&ui_state);
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
    for (auto [entity, obstacle, obstacle_pos, required_shape] :
         reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape>(entt::exclude<ScoredTag>).each()) {
        (void)entity;
        (void)obstacle;
        int shape_index = static_cast<int>(required_shape.shape);
        if (shape_index < 0 || shape_index >= static_cast<int>(nearest_dist.size())) continue;
        float dist = constants::PLAYER_Y - obstacle_pos.position.y;
        if (dist > 0.0f && (nearest_dist[shape_index] < 0.0f || dist < nearest_dist[shape_index])) {
            nearest_dist[shape_index] = dist;
        }
    }

    auto* song_state = reg.ctx().find<SongState>();
    float perfect_dist = gameplay_hud_perfect_distance(song_state);
    float good_dist = gameplay_hud_good_distance(song_state);
    float ring_appear_dist = gameplay_hud_ok_distance(song_state);
    float max_ring_radius = btn_radius * style.approach_ring_max_radius_scale;

    // Reduce-motion (#534): suppress the continuous approach-ring lerp/fade
    // and snap to a static "perfect-window-imminent" indicator so the
    // timing cue stays informational without the size animation.
    const auto* settings_ptr = find_settings_state(reg);
    const bool reduce_motion = settings_ptr && settings_ptr->reduce_motion;

    for (const auto& button : buttons) {
        bool is_active = (active_shape == button.shape);
        Color bg = is_active ? style.active_bg : style.inactive_bg;
        Color border = is_active ? style.active_border : style.inactive_border;
        Color icon = is_active ? style.active_icon : style.inactive_icon;
        DrawCircleV({button.cx, button.cy}, btn_radius, bg);
        DrawCircleLinesV({button.cx, button.cy}, btn_radius, border);
        draw_shape_flat(button.shape, button.cx, button.cy, btn_radius * 1.2f, icon);

        int shape_index = static_cast<int>(button.shape);
        if (shape_index < 0 || shape_index >= static_cast<int>(nearest_dist.size())) continue;
        const auto cue = gameplay_hud_ring_cue(nearest_dist[shape_index],
                                               perfect_dist,
                                               good_dist,
                                               ring_appear_dist);
        if (!cue.visible) continue;
        float ratio = gameplay_hud_ring_ratio(nearest_dist[shape_index], perfect_dist, ring_appear_dist);

        const auto envelope = approach_ring_envelope(ratio, btn_radius,
                                                     max_ring_radius, reduce_motion);
        if (envelope.alpha_scale <= 0.0f) continue;

        Color ring_color = Fade(cue.color, (200.0f / 255.0f) * envelope.alpha_scale);
        DrawCircleLinesV({button.cx, button.cy}, envelope.radius, ring_color);
        DrawCircleLinesV({button.cx, button.cy}, envelope.radius - 1.0f,
            {cue.color.r, cue.color.g, cue.color.b, static_cast<unsigned char>(ring_color.a / 2)});
    }
}

void render_energy_bar(const entt::registry& reg) {
    auto view = reg.view<EnergyBarTag, EnergyBarLayout, EnergyBarVisual>();
    for (auto [entity, layout, visual] : view.each()) {
        (void)entity;

        const int segment_count = effective_energy_bar_segment_count(layout);
        const float bar_top = layout.bottom - layout.height;
        const float seg_h = (layout.height - (segment_count - 1) * layout.segment_gap)
            / static_cast<float>(segment_count);

        for (int i = 0; i < visual.overflow_segments; ++i) {
            float seg_y = bar_top - (i + 1) * (seg_h + layout.segment_gap);
            float fade = 1.0f - static_cast<float>(i) / 5.0f;
            DrawRectangleRec({layout.x, seg_y, layout.width, seg_h},
                Fade({255, 80, 200, 255}, fade * (220.0f / 255.0f)));
        }

        DrawRectangleRec({layout.x, bar_top, layout.width, layout.height}, {15, 15, 25, 180});

        int filled_segs = static_cast<int>(visual.fill * static_cast<float>(segment_count) + 0.5f);
        int visible_segs = static_cast<int>(visual.visible_level * static_cast<float>(segment_count) + 0.5f);

        for (int i = 0; i < segment_count; ++i) {
            float seg_y = layout.bottom - (i + 1) * (seg_h + layout.segment_gap) + layout.segment_gap;
            float t = static_cast<float>(i) / static_cast<float>(segment_count > 1 ? segment_count - 1 : 1);
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
                float red_boost = visual.flash_ratio * 0.45f + visual.critical_intensity * 0.35f;
                float cool_dim = visual.critical_intensity * 0.40f;
                unsigned char rr = static_cast<unsigned char>(
                    Clamp(Lerp(static_cast<float>(cr), 255.0f, red_boost), 0.0f, 255.0f));
                unsigned char rg = static_cast<unsigned char>(
                    Clamp(Lerp(static_cast<float>(cg), 0.0f, cool_dim), 0.0f, 255.0f));
                unsigned char rb = static_cast<unsigned char>(
                    Clamp(Lerp(static_cast<float>(cb), 0.0f, cool_dim), 0.0f, 255.0f));
                DrawRectangleRec({layout.x, seg_y, layout.width, seg_h}, {rr, rg, rb, 255});
            } else if (i < visible_segs) {
                float fade = 1.0f - static_cast<float>(i - filled_segs)
                    / std::max(1.0f, static_cast<float>(visible_segs - filled_segs));
                DrawRectangleRec({layout.x, seg_y, layout.width, seg_h}, Fade({cr, cg, cb, 255}, fade * (200.0f / 255.0f)));
            } else {
                DrawRectangleRec({layout.x, seg_y, layout.width, seg_h}, {35, 35, 50, 50});
            }
        }

        if (visual.flash_overlay > 0.0f) {
            DrawRectangleRec({layout.x - 1.0f, bar_top - 1.0f, layout.width + 2.0f, layout.height + 2.0f},
                Fade({255, 80, 80, 255}, visual.flash_overlay * (140.0f / 255.0f)));
        }

        float border_thickness = 1.0f + visual.critical_intensity * 2.0f;
        unsigned char border_r = static_cast<unsigned char>(80.0f + visual.critical_intensity * 175.0f);
        unsigned char border_g = static_cast<unsigned char>(80.0f - visual.critical_intensity * 40.0f);
        unsigned char border_b = static_cast<unsigned char>(100.0f - visual.critical_intensity * 60.0f);
        unsigned char border_a = static_cast<unsigned char>(140.0f + visual.critical_intensity * 90.0f);
        DrawRectangleLinesEx({layout.x, bar_top, layout.width, layout.height}, border_thickness,
            {border_r, border_g, border_b, border_a});
    }
}

} // anonymous namespace

namespace {

float gameplay_hud_timing_distance(const SongState* song_state, float timing_seconds) {
    const float scroll_speed = (song_state && song_state->scroll_speed > 0.0f)
        ? song_state->scroll_speed
        : constants::BASE_SCROLL_SPEED;
    return scroll_speed * timing_seconds;
}

} // anonymous namespace

float gameplay_hud_perfect_distance(const SongState* song_state) {
    return gameplay_hud_timing_distance(song_state, kTimingPerfectSeconds);
}

float gameplay_hud_good_distance(const SongState* song_state) {
    return gameplay_hud_timing_distance(song_state, kTimingGoodSeconds);
}

float gameplay_hud_ok_distance(const SongState* song_state) {
    return gameplay_hud_timing_distance(song_state, kTimingOkSeconds);
}

float gameplay_hud_ring_ratio(float nearest_dist, float perfect_dist, float ring_appear_dist) {
    const float denom = ring_appear_dist - perfect_dist;
    if (denom <= 0.0f) return 0.0f;
    return Clamp((nearest_dist - perfect_dist) / denom, 0.0f, 1.0f);
}

GameplayHudRingCue gameplay_hud_ring_cue(float nearest_dist,
                                         float perfect_dist,
                                         float good_dist,
                                         float ring_appear_dist) {
    if (nearest_dist <= 0.0f || nearest_dist >= ring_appear_dist) return {};
    if (nearest_dist <= perfect_dist) return {true, kGameplayHudRingPerfectColor};
    if (nearest_dist <= good_dist)    return {true, kGameplayHudRingNearColor};
    return {true, kGameplayHudRingFarColor};
}

void init_gameplay_hud_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

Rectangle gameplay_hud_circle_input_bounds() {
    static const GameplayHudLayoutState geometry_state = GameplayHudLayout_Init();
    return GameplayHudLayout_CircleButtonBounds(&geometry_state);
}

Rectangle gameplay_hud_square_input_bounds() {
    static const GameplayHudLayoutState geometry_state = GameplayHudLayout_Init();
    return GameplayHudLayout_SquareButtonBounds(&geometry_state);
}

Rectangle gameplay_hud_triangle_input_bounds() {
    static const GameplayHudLayoutState geometry_state = GameplayHudLayout_Init();
    return GameplayHudLayout_TriangleButtonBounds(&geometry_state);
}

void gameplay_hud_process_button_input(entt::registry& reg) {
    const auto& input = reg.ctx().get<InputState>();
    if (!(input.click || input.button_touch_up || input.touch_up)) return;

    static const GameplayHudLayoutState geometry_state = GameplayHudLayout_Init();
    if (input.click) {
        const Vector2 pointer = {input.end_x, input.end_y};
        gameplay_hud_apply_button_presses(
            reg,
            CheckCollisionPointRec(pointer, GameplayHudLayout_PauseButtonBounds(&geometry_state)),
            CheckCollisionPointRec(pointer, GameplayHudLayout_CircleButtonBounds(&geometry_state)),
            CheckCollisionPointRec(pointer, GameplayHudLayout_SquareButtonBounds(&geometry_state)),
            CheckCollisionPointRec(pointer, GameplayHudLayout_TriangleButtonBounds(&geometry_state)));
    }

    if (input.touch_up && !input.button_touch_up) {
        const Vector2 pointer = {input.end_x, input.end_y};
        gameplay_hud_apply_button_presses(
            reg,
            CheckCollisionPointRec(pointer, GameplayHudLayout_PauseButtonBounds(&geometry_state)),
            false,
            false,
            false);
    }

    if (input.button_touch_up) {
        const Vector2 pointer = {input.button_end_x, input.button_end_y};
        gameplay_hud_apply_button_presses(
            reg,
            false,
            CheckCollisionPointRec(pointer, GameplayHudLayout_CircleButtonBounds(&geometry_state)),
            CheckCollisionPointRec(pointer, GameplayHudLayout_SquareButtonBounds(&geometry_state)),
            CheckCollisionPointRec(pointer, GameplayHudLayout_TriangleButtonBounds(&geometry_state)));
    }
}

void gameplay_hud_apply_button_presses(entt::registry& reg,
                                       bool pause_pressed,
                                       bool circle_pressed,
                                       bool square_pressed,
                                       bool triangle_pressed) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::Playing) return;

    auto& disp = reg.ctx().get<entt::dispatcher>();
    if (circle_pressed) {
        disp.enqueue<ButtonPressEvent>(
            {ButtonPressKind::Shape, Shape::Circle, MenuActionKind::Confirm, 0});
    }
    if (square_pressed) {
        disp.enqueue<ButtonPressEvent>(
            {ButtonPressKind::Shape, Shape::Square, MenuActionKind::Confirm, 0});
    }
    if (triangle_pressed) {
        disp.enqueue<ButtonPressEvent>(
            {ButtonPressKind::Shape, Shape::Triangle, MenuActionKind::Confirm, 0});
    }

    if (pause_pressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Paused;
    }
}

void render_gameplay_hud_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<GameplayHudController>(reg);
    auto& state = controller.state();
    auto* score = reg.ctx().find<ScoreState>();
    auto* display = reg.ctx().find<ScoreDisplay>();
    auto* current = reg.ctx().find<CurrentSongHighScore>();

    int saved_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);

    // Render score at top-left
    if (score && display) {
        GuiSetStyle(DEFAULT, TEXT_SIZE, 28);
        char score_text[32];
        std::snprintf(score_text, sizeof(score_text), "%d", display->displayed);
        GuiLabel(Rectangle{ 80, 20, 200, 40 }, score_text);

        // High score below
        GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
        char high_score_text[32];
        const int32_t high = current ? current->value : 0;
        std::snprintf(high_score_text, sizeof(high_score_text), "BEST: %d", high);
        GuiSetAlpha(0.7f);
        GuiLabel(Rectangle{ 80, 50, 200, 30 }, high_score_text);
        GuiSetAlpha(1.0f);

        if (score->chain_count >= 2) {
            const bool meaningful_chain = score->chain_count >= 5;
            char chain_text[32];
            std::snprintf(chain_text, sizeof(chain_text), meaningful_chain ? "CHAIN %d!" : "CHAIN %d",
                          score->chain_count);
            if (meaningful_chain) {
                const auto* settings = find_settings_state(reg);
                const bool reduce_motion = settings && settings->reduce_motion;
                const auto* song = reg.ctx().find<SongState>();
                const float pulse_time = (song && song->playing) ? song->song_time : static_cast<float>(GetTime());
                const float pulse = reduce_motion ? 0.5f : (0.5f + 0.5f * std::sin(pulse_time * 8.0f));
                DrawRectangleLinesEx(Rectangle{76, 82, 138, 28}, 2.0f,
                                     Fade({100, 255, 180, 255}, 0.20f + 0.20f * pulse));
            }
            GuiSetStyle(DEFAULT, TEXT_SIZE, meaningful_chain ? 20 : 18);
            GuiSetAlpha(meaningful_chain ? 0.95f : 0.70f);
            GuiLabel(Rectangle{ 80, 80, 160, 32 }, chain_text);
            GuiSetAlpha(1.0f);
        }
    }

    render_shape_buttons(reg, kGameplayHudVisualStyle, state);

    render_energy_bar(reg);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetAlpha(0.8f);
    GuiLabel(Rectangle{ 10, 740, 90, 30 }, "ENERGY");
    GuiSetAlpha(1.0f);

    const Rectangle circle_bounds = GameplayHudLayout_CircleButtonBounds(&state);
    const float lane_divider_y = circle_bounds.y - kLaneDividerOffsetFromShapeRow;
    DrawLineV({0.0f, lane_divider_y},
              {constants::SCREEN_W_F, lane_divider_y},
              kGameplayHudVisualStyle.lane_divider_color);

    // Render Pause button from generated layout state.
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
    controller.render();

    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);

    state.PauseButtonPressed = false;
    state.CircleButtonPressed = false;
    state.SquareButtonPressed = false;
    state.TriangleButtonPressed = false;
}
