#include "all_systems.h"
#include "camera_system.h"
#include "settings_system.h"
#include "../components/actions.h"
#include "../components/energy_bar.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/rhythm.h"
#include "../components/game_state.h"
#include "../components/settings.h"
#include "../components/song_state.h"
#include "text_resources.h"
#include "../components/ui.h"
#include "../constants.h"
#include "../tags/tags.h"
#include "../util/level_content_config.h"
#include "../util/shape_draw_2d.h"

#include <raygui.h>
#include <raylib.h>
#include <raymath.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>

// ═════════════════════════════════════════════════════════════════════════════
// UI render system — 2D overlay pass
// ═════════════════════════════════════════════════════════════════════════════

namespace {

const Font& popup_font_for_size(const TextContext& ctx, FontSize font_size) {
    // Fabian Principle 1 / issue #1306: label-to-value lookup by ordinal,
    // not by `switch`. `Font` carries runtime GPU state so a `constexpr`
    // array of `Font` is not viable; instead the table is a `std::array`
    // of pointer-to-data-members, indexed by `static_cast<size_t>(font_size)`.
    // Row order must match the `FontSize` declaration in
    // `app/components/text.h`.
    static constexpr std::array<const Font TextContext::*, 3> kFontByFontSize{{
        /* Small  */ &TextContext::font_small,
        /* Medium */ &TextContext::font_medium,
        /* Large  */ &TextContext::font_large,
    }};
    static_assert(kFontByFontSize.size() ==
                  static_cast<std::size_t>(FontSize::Large) + 1,
                  "kFontByFontSize must cover every FontSize enumerator");

    const auto idx = static_cast<std::size_t>(font_size);
    if (idx >= kFontByFontSize.size()) return ctx.font_medium;
    return ctx.*kFontByFontSize[idx];
}

// ── Entity-driven UI render (issue #1287) ──────────────────────────────────
//
// Iterates the per-kind tag views populated by the rguilayout codegen
// (`spawn_<screen>_screen()`, see `app/systems/generated/`). Per-screen membership
// is the per-screen tag on each entity; the screen lifecycle system
// (`app/systems/screen_lifecycle_system.h`) keeps the entity set in sync
// with the active `GamePhase*Tag`, so this pass only sees the active
// screen's entities and trivially partitions by tag.
//
// Per-screen visual specifics (font size, alignment, label vs centered
// label, button colour overrides) are kept inline as the raygui defaults
// that the per-screen render constants below mirror.
//
// Migration boundary: a button press dispatch lives in
// `app/systems/ui_update_system.cpp` — that system hit-tests against
// `view<UiPosition, UiBounds, OnPress, UiButtonTag>()`. The GuiButton call
// here renders only; its return value is ignored.

constexpr int kEntityLabelTextSize  = 24;
constexpr int kEntityButtonTextSize = 28;
// Paused overlay headline ("PAUSED") needs a larger size than the default
// raygui DEFAULT::TEXT_SIZE; matches the legacy paused-layout constant
// that this screen replaces (issue #1290 / #1308).
constexpr float kPausedHeadlineHeight = 80.0f;
constexpr int   kPausedHeadlineSize   = 56;

// Level-select dynamic visuals (issue #1296). Constants mirror
// `level_select_dynamic_spawn_system.cpp` (single source of truth for
// the per-card geometry); kept duplicated here to avoid pulling the
// dynamic-spawn header into the already-busy render TU. Drift across
// the two locations would show up as a visible offset in playtest.
constexpr float kLevelCardCornerRadius = 0.1f;
constexpr int   kLevelCardCornerSegs   = 4;
constexpr float kLevelCardBorderThick  = 2.0f;
constexpr int   kLevelCardTitleSize    = 32;
constexpr int   kLevelCardTrackSize    = 32;
constexpr int   kLevelDiffTextSize     = 20;
constexpr float kLevelCardTitleOffsetX = 30.0f;
constexpr float kLevelCardTitleOffsetY = 30.0f;
constexpr float kLevelCardTrackBoxW    = 50.0f;
constexpr float kLevelCardTrackBoxH    = 40.0f;
constexpr float kLevelCardTrackInsetX  = 60.0f;
constexpr float kDiffActiveBorderThick = 3.0f;
constexpr float kDiffActiveBarInsetX   = 6.0f;
constexpr float kDiffActiveBarOffsetY  = 6.0f;
constexpr float kDiffActiveBarHeight   = 3.0f;
constexpr Color kLevelCardBgSelected   {  40,  40,  60, 255 };
constexpr Color kLevelCardBgUnselected {  20,  20,  30, 255 };
constexpr Color kLevelCardBorderSel    { 100, 150, 255, 255 };
constexpr Color kLevelCardBorderUnsel  {  60,  60,  80, 255 };
constexpr Color kDiffActiveBorderColor { 120, 180, 255, 255 };
constexpr Color kDiffActiveBarColor    {  80, 120, 200, 255 };

void render_ui_entities(entt::registry& reg) {
    const int saved_text_size       = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int saved_label_alignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);

    // Labels (and dummy-rec slots that carry text). Both kinds use GuiLabel
    // rendering with center alignment; the kind distinction is for hit-test
    // exclusion in `ui_update_system` and future visual specialization.
    //
    // Per-entity font-size / alpha overrides (issue #1297): if
    // `UiLabelFontSize` is present and > 0 use it (gameplay HUD score /
    // high-score / chain / energy use this), otherwise fall back to the
    // height-driven Paused headline heuristic. `UiLabelAlpha` does the
    // same trick for the GuiSetAlpha multiplier.
    {
        auto view = reg.view<UiPosition, UiBounds, UiLabel, UiLabelTag>();
        for (auto [e, pos, sz, label] : view.each()) {
            if (label.text[0] == '\0') continue;
            const auto* fsize = reg.try_get<UiLabelFontSize>(e);
            const int text_size = (fsize != nullptr && fsize->size > 0)
                                ? fsize->size
                                : ((sz.h >= kPausedHeadlineHeight)
                                    ? kPausedHeadlineSize
                                    : kEntityLabelTextSize);
            const auto* alpha = reg.try_get<UiLabelAlpha>(e);
            const float alpha_value = (alpha != nullptr) ? alpha->value : 1.0f;
            GuiSetStyle(DEFAULT, TEXT_SIZE, text_size);
            if (alpha_value < 1.0f) GuiSetAlpha(alpha_value);
            GuiLabel(Rectangle{pos.x, pos.y, sz.w, sz.h}, label.text.data());
            if (alpha_value < 1.0f) GuiSetAlpha(1.0f);
        }
    }

    // ── Level Select cards (issue #1296) ────────────────────────────
    // Per-level rounded rect + border + title + track number. Selection
    // is determined by `LevelIndex.value == LevelSelectState.selected_level`
    // (no separate active-tag — existence of the singleton ctx data is the
    // signal). Rendered before the generic GuiButton pass so card paint
    // does not get overdrawn; the generic button pass excludes
    // `LevelCardTag` to avoid double-rendering them as raygui buttons.
    {
        auto* lss = reg.ctx().find<LevelSelectState>();
        if (lss != nullptr) {
            const int selected = lss->selected_level;
            auto cards = reg.view<UiPosition, UiBounds, UiLabel,
                                  LevelCardTag, LevelIndex>();
            for (auto [e, pos, sz, label, idx] : cards.each()) {
                (void)e;
                const bool is_selected = (idx.value == selected);
                const Color bg     = is_selected ? kLevelCardBgSelected
                                                 : kLevelCardBgUnselected;
                const Color border = is_selected ? kLevelCardBorderSel
                                                 : kLevelCardBorderUnsel;
                const Rectangle card{pos.x, pos.y, sz.w, sz.h};
                DrawRectangleRounded(card, kLevelCardCornerRadius,
                                     kLevelCardCornerSegs, bg);
                DrawRectangleRoundedLinesEx(card, kLevelCardCornerRadius,
                                            kLevelCardCornerSegs,
                                            kLevelCardBorderThick, border);

                GuiSetStyle(DEFAULT, TEXT_SIZE, kLevelCardTitleSize);
                GuiSetAlpha(is_selected ? 1.0f : 0.6f);
                GuiLabel(Rectangle{pos.x + kLevelCardTitleOffsetX,
                                   pos.y + kLevelCardTitleOffsetY,
                                   sz.w - kLevelCardTitleOffsetX * 2.0f,
                                   kLevelCardTitleSize + 8.0f},
                         label.text.data());
                GuiSetAlpha(1.0f);

                // Track number (1-based). LEVEL_COUNT is small (3 today,
                // grows linearly with content) but `LevelIndex::value` is
                // a plain `int`; size the buffer to fit any 32-bit
                // integer + sign + null so GCC's
                // `-Werror=format-truncation` is satisfied without
                // adding a clamp that would silently misrender on a
                // malformed entity.
                char track_buf[12];
                std::snprintf(track_buf, sizeof(track_buf), "%d", idx.value + 1);
                GuiSetStyle(DEFAULT, TEXT_SIZE, kLevelCardTrackSize);
                GuiSetAlpha(0.4f);
                GuiLabel(Rectangle{pos.x + sz.w - kLevelCardTrackInsetX,
                                   pos.y + kLevelCardTitleOffsetY,
                                   kLevelCardTrackBoxW, kLevelCardTrackBoxH},
                         track_buf);
                GuiSetAlpha(1.0f);
            }
        }
    }

    // Buttons — rendered with GuiButton. Press dispatch lives in
    // `ui_update_system`; ignoring the return value keeps that path the
    // single source of truth for button activation.
    //
    // Toggle buttons (`UiToggleTag`, Settings screen #1295) need two-cue
    // ON/OFF styling per A11Y issue #390: green border + dim green fill
    // when on, grey border + dark fill when off (the icon-prefix cue is
    // baked into `UiLabel::text` by `settings_screen_bind_system`).
    // Plain (non-toggle) buttons render in a second pass with the default
    // raygui style. `UiHiddenOnWebTag` entities (Title-screen ExitButton
    // on Web per #511) are skipped to keep the button invisible on Web —
    // their bounds still act as a tap-anywhere dead-zone via
    // `title_start_tap_system`.
    //
    // Level-select dynamic archetypes (`LevelCardTag`, `DifficultyButtonTag`,
    // issue #1296) are excluded — they render in dedicated passes (cards
    // above, difficulty buttons below) so the rounded-rect / active-state
    // emphasis aren't overdrawn by a default GuiButton call.
    {
        GuiSetStyle(DEFAULT, TEXT_SIZE, kEntityButtonTextSize);

        // ── Pass A: toggle buttons (per-entity colour override) ───
        constexpr int kToggleStyleProps[] = {
            BORDER_COLOR_NORMAL, BASE_COLOR_NORMAL, TEXT_COLOR_NORMAL,
            BORDER_COLOR_FOCUSED, BASE_COLOR_FOCUSED, TEXT_COLOR_FOCUSED,
        };
        constexpr std::size_t kToggleStylePropCount =
            sizeof(kToggleStyleProps) / sizeof(kToggleStyleProps[0]);
        int saved_button_style[kToggleStylePropCount];
        for (std::size_t i = 0; i < kToggleStylePropCount; ++i) {
            saved_button_style[i] = GuiGetStyle(BUTTON, kToggleStyleProps[i]);
        }

        constexpr Color kToggleOnBorder  {  64, 200, 110, 255 };
        constexpr Color kToggleOnBase    {  20,  64,  36, 255 };
        constexpr Color kToggleOffBorder { 130, 130, 130, 255 };
        constexpr Color kToggleOffBase   {  36,  36,  36, 255 };
        constexpr Color kToggleTextColor { 230, 230, 230, 255 };

        auto toggle_view = reg.view<UiPosition, UiBounds, UiLabel,
                                    UiButtonTag, UiToggleTag>(
#ifdef PLATFORM_WEB
            entt::exclude<UiHiddenOnWebTag>
#endif
        );
        for (auto entity : toggle_view) {
            const auto& pos = toggle_view.template get<UiPosition>(entity);
            const auto& sz  = toggle_view.template get<UiBounds>(entity);
            const auto& lbl = toggle_view.template get<UiLabel>(entity);
            // `UiToggleState` is written each frame by the per-screen bind
            // system; if missing (frame 0 race before the bind runs) treat
            // as OFF — safe default, harmless visual flicker.
            const auto* state = reg.try_get<UiToggleState>(entity);
            const bool on = (state != nullptr) && state->on;

            const Color border = on ? kToggleOnBorder : kToggleOffBorder;
            const Color base   = on ? kToggleOnBase   : kToggleOffBase;
            GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL,  ColorToInt(border));
            GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToInt(border));
            GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,    ColorToInt(base));
            GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED,   ColorToInt(base));
            GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,    ColorToInt(kToggleTextColor));
            GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,   ColorToInt(kToggleTextColor));
            (void)GuiButton(Rectangle{pos.x, pos.y, sz.w, sz.h}, lbl.text.data());
        }

        for (std::size_t i = 0; i < kToggleStylePropCount; ++i) {
            GuiSetStyle(BUTTON, kToggleStyleProps[i], saved_button_style[i]);
        }

        // ── Pass B: plain (non-toggle, non-level-select) buttons ──
        auto view = reg.view<UiPosition, UiBounds, UiLabel, UiButtonTag>(
            entt::exclude<UiToggleTag,
                          LevelCardTag,
                          DifficultyButtonTag
#ifdef PLATFORM_WEB
                          , UiHiddenOnWebTag
#endif
                          >
        );
        for (auto [e, pos, sz, label] : view.each()) {
            (void)e;
            (void)GuiButton(Rectangle{pos.x, pos.y, sz.w, sz.h}, label.text.data());
        }
    }

    // ── Level Select difficulty buttons (issue #1296) ───────────────
    // Render only those belonging to the currently selected level card.
    // Active difficulty gets a two-cue emphasis (thick border + inset
    // selection bar) painted *after* GuiButton so it isn't overdrawn by
    // raygui's BUTTON style (#469). The colour cue alone wouldn't satisfy
    // A11Y/2.1.3; the non-colour cues (line thickness + bar) make the
    // selected option distinguishable to colourblind players.
    {
        auto* lss = reg.ctx().find<LevelSelectState>();
        if (lss != nullptr) {
            const int active_level = lss->selected_level;
            const int active_diff  = lss->selected_difficulty;

            GuiSetStyle(DEFAULT, TEXT_SIZE, kLevelDiffTextSize);
            auto diffs = reg.view<UiPosition, UiBounds, UiLabel,
                                  DifficultyButtonTag, LevelIndex,
                                  DifficultyIndex>();
            for (auto [e, pos, sz, label, lidx, didx] : diffs.each()) {
                (void)e;
                if (lidx.value != active_level) continue;
                const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
                (void)GuiButton(rect, label.text.data());
                if (didx.value == active_diff) {
                    DrawRectangleRoundedLinesEx(rect, kLevelCardCornerRadius,
                                                kLevelCardCornerSegs,
                                                kDiffActiveBorderThick,
                                                kDiffActiveBorderColor);
                    const Rectangle bar{
                        rect.x + kDiffActiveBarInsetX,
                        rect.y + rect.height - kDiffActiveBarOffsetY,
                        rect.width - kDiffActiveBarInsetX * 2.0f,
                        kDiffActiveBarHeight,
                    };
                    DrawRectangleRec(bar, kDiffActiveBarColor);
                }
            }
        }
    }

    // Shape icons — per-shape tag table; each row picks one row of
    // `shape_draw_2d::kFlatDrawFns`. No `switch` on a discriminator
    // (Fabian Principle 1). Used today by the Title screen's three shape
    // preview entities (issue #1294); any `.rgl` UiDummyRec named
    // ShapeCircle / ShapeSquare / ShapeTriangle / ShapeHexagon picks up
    // the matching tag via `tools/rguilayout/codegen.py`.
    //
    // `LaneButtonTag` (gameplay HUD #1297) is excluded — those entities
    // carry `UiShapeIconCircle/Square/TriangleTag` too but their bg /
    // border / approach ring is painted by the dedicated lane button
    // pass further below, which draws the icon itself at the proper
    // button-radius scale (so this generic pass would double-draw).
    {
        constexpr Color kShapeIconColor{200, 230, 255, 255};

        auto draw_for_tag = []<typename Tag>(entt::registry& r,
                                             Shape shape,
                                             Color color) {
            auto v = r.view<UiPosition, UiBounds, Tag>(
                entt::exclude<LaneButtonTag>);
            for (auto [e, pos, sz] : v.each()) {
                (void)e;
                const float cx   = pos.x + sz.w * 0.5f;
                const float cy   = pos.y + sz.h * 0.5f;
                const float size = std::min(sz.w, sz.h);
                shape_draw_2d::draw_flat(shape, cx, cy, size, color);
            }
        };

        draw_for_tag.template operator()<UiShapeIconCircleTag>  (reg, Shape::Circle,   kShapeIconColor);
        draw_for_tag.template operator()<UiShapeIconSquareTag>  (reg, Shape::Square,   kShapeIconColor);
        draw_for_tag.template operator()<UiShapeIconTriangleTag>(reg, Shape::Triangle, kShapeIconColor);
        draw_for_tag.template operator()<UiShapeIconHexagonTag> (reg, Shape::Hexagon,  kShapeIconColor);
    }

    // ── Gameplay HUD lane buttons (issue #1297) ─────────────────────
    //
    // Three buttons (Circle / Square / Triangle) painted as filled disc
    // + border + shape icon, with an optional approach-ring overlay
    // when the next required obstacle for that shape is within the OK
    // timing window. The button "active" state is per-shape tag
    // presence on the player (Fabian Principle 1: no `Shape` enum
    // compare). The approach-ring envelope is precomputed by
    // `approach_ring_envelope_system` into the per-button `ApproachRing`
    // component; this pass only reads it.
    //
    // Lane divider: rendered as a thin line at the LaneDividerSlot.
    // The codegen spawns it as a `UiDummyRecTag` entity sized 720×4 at
    // (0, 1120); we identify it by the `LaneDividerSlot`-shaped bounds
    // and draw a single line at the top edge (matches legacy).
    {
        constexpr Color kLaneButtonActiveBg     {  60,  60, 100, 255 };
        constexpr Color kLaneButtonInactiveBg   {  30,  30,  50, 200 };
        constexpr Color kLaneButtonActiveBorder { 120, 180, 255, 255 };
        constexpr Color kLaneButtonInactiveBord {  60,  60,  80, 255 };
        constexpr Color kLaneButtonActiveIcon   { 200, 230, 255, 255 };
        constexpr Color kLaneButtonInactiveIcon { 100, 100, 120, 200 };
        constexpr float kLaneButtonIconScale    = 1.2f;
        constexpr float kLaneButtonRadiusFactor = 1.0f / 2.8f;

        auto draw_lane = [&]<typename ShapeIconTag>(entt::registry& r,
                                                    Shape shape,
                                                    bool active) {
            auto v = r.view<LaneButtonTag, UiPosition, UiBounds, ShapeIconTag>();
            for (auto entity : v) {
                const auto& pos = v.template get<UiPosition>(entity);
                const auto& sz  = v.template get<UiBounds>(entity);
                const float cx  = pos.x + sz.w * 0.5f;
                const float cy  = pos.y + sz.h * 0.5f;
                const float btn_radius = sz.w * kLaneButtonRadiusFactor;
                const Color bg     = active ? kLaneButtonActiveBg
                                            : kLaneButtonInactiveBg;
                const Color border = active ? kLaneButtonActiveBorder
                                            : kLaneButtonInactiveBord;
                const Color icon   = active ? kLaneButtonActiveIcon
                                            : kLaneButtonInactiveIcon;
                DrawCircleV({cx, cy}, btn_radius, bg);
                DrawCircleLinesV({cx, cy}, btn_radius, border);
                shape_draw_2d::draw_flat(shape, cx, cy,
                                         btn_radius * kLaneButtonIconScale,
                                         icon);

                const auto* ring = r.template try_get<ApproachRing>(entity);
                if (ring == nullptr || !r.template all_of<ApproachRingVisibleTag>(entity)) continue;
                const Color base_color{ring->color_r, ring->color_g,
                                       ring->color_b, ring->color_a};
                const Color ring_color = Fade(base_color,
                    (200.0f / 255.0f) * ring->alpha_scale);
                DrawCircleLinesV({cx, cy}, ring->radius, ring_color);
                DrawCircleLinesV({cx, cy}, ring->radius - 1.0f,
                                 Color{ring->color_r, ring->color_g, ring->color_b,
                                       static_cast<unsigned char>(ring_color.a / 2)});
            }
        };

        // Active state from the player entity's `Shape*Tag` (no Shape
        // enum compare — Fabian Principle 1). Frame-0 race (player not
        // yet spawned) treated as all-inactive: harmless visual.
        bool circle_active   = false;
        bool square_active   = false;
        bool triangle_active = false;
        for (auto entity : reg.view<PlayerTag, PlayerShape>()) {
            circle_active   = reg.all_of<ShapeCircleTag>(entity);
            square_active   = reg.all_of<ShapeSquareTag>(entity);
            triangle_active = reg.all_of<ShapeTriangleTag>(entity);
        }

        draw_lane.template operator()<UiShapeIconCircleTag>  (reg, Shape::Circle,   circle_active);
        draw_lane.template operator()<UiShapeIconSquareTag>  (reg, Shape::Square,   square_active);
        draw_lane.template operator()<UiShapeIconTriangleTag>(reg, Shape::Triangle, triangle_active);
    }

    // ── Gameplay HUD lane divider (issue #1297) ─────────────────────
    //
    // The lane divider is a non-text dummy-rec slot baked into the
    // `.rgl` at (0, 1120, 720, 4). It renders as a 1-px horizontal line
    // at the top edge of those bounds. Identified by a 720-wide, 4-tall
    // dummy-rec under `GameplayHudTag` — no separate tag is necessary
    // since the geometry is unique within the HUD.
    {
        constexpr Color kLaneDividerColor{40, 40, 60, 200};
        constexpr float kLaneDividerWidth  = constants::SCREEN_W_F;
        constexpr float kLaneDividerHeight = 4.0f;
        auto view = reg.view<GameplayHudTag, UiDummyRecTag, UiPosition, UiBounds>();
        for (auto [e, pos, sz] : view.each()) {
            (void)e;
            if (sz.w != kLaneDividerWidth || sz.h != kLaneDividerHeight) continue;
            DrawLineV({0.0f, pos.y},
                      {constants::SCREEN_W_F, pos.y},
                      kLaneDividerColor);
        }
    }

    // ── Gameplay HUD chain bg pulse (issue #1297) ───────────────────
    //
    // When `ChainBgPulseTag` is present on the ChainSlot entity, render
    // a thin pulsing border rectangle behind the chain text. The bind
    // system emplaces/removes the tag based on chain count; the
    // sin-based animation reads `SongState::song_time` when available
    // (so pause freezes the pulse), falling back to `GetTime()` while
    // the song isn't playing. Reduce-motion settings skip the
    // animation (steady half-pulse).
    {
        constexpr Color kChainPulseColor{100, 255, 180, 255};
        constexpr Rectangle kChainPulseInset{-4.0f, 2.0f, -22.0f, -4.0f};
        const auto* settings = find_settings_state(reg);
        const bool reduce_motion = (settings != nullptr) && settings->reduce_motion;
        const auto* song = reg.ctx().find<SongState>();
        const float pulse_time = (song != nullptr && song->playing)
                                 ? song->song_time
                                 : static_cast<float>(GetTime());
        const float pulse = reduce_motion
                          ? 0.5f
                          : (0.5f + 0.5f * std::sin(pulse_time * 8.0f));

        auto view = reg.view<ChainBgPulseTag, UiPosition, UiBounds>();
        for (auto [e, pos, sz] : view.each()) {
            (void)e;
            const Rectangle border{
                pos.x + kChainPulseInset.x,
                pos.y + kChainPulseInset.y,
                sz.w  + kChainPulseInset.width,
                sz.h  + kChainPulseInset.height,
            };
            DrawRectangleLinesEx(border, 2.0f,
                Fade(kChainPulseColor, 0.20f + 0.20f * pulse));
        }
    }

    // ── Gameplay HUD energy bar (issue #1297) ───────────────────────
    //
    // The energy bar entity is spawned once at session init by
    // `energy_bar_entity` and persists across phases (so its
    // `EnergyBarVisual` state survives Pause/Resume). It is rendered
    // here only while the gameplay HUD screen is active (gated by the
    // presence of any `GameplayHudTag` entity, same gate the bind
    // systems use). The actual segment geometry comes from
    // `EnergyBarLayout` (single source of truth), not the
    // EnergyBarSlot `.rgl` rectangle (which is a placeholder marker).
    if (reg.view<GameplayHudTag>().begin() != reg.view<GameplayHudTag>().end()) {
        auto view = reg.view<EnergyBarTag, EnergyBarLayout, EnergyBarVisual>();
        for (auto [entity, layout, visual] : view.each()) {
            (void)entity;

            const int segment_count = effective_energy_bar_segment_count(layout);
            const float bar_top = layout.bottom - layout.height;
            const float seg_h = (layout.height - (segment_count - 1) * layout.segment_gap)
                / static_cast<float>(segment_count);

            for (int i = 0; i < visual.overflow_segments; ++i) {
                const float seg_y = bar_top - (i + 1) * (seg_h + layout.segment_gap);
                const float fade  = 1.0f - static_cast<float>(i) / 5.0f;
                DrawRectangleRec({layout.x, seg_y, layout.width, seg_h},
                    Fade({255, 80, 200, 255}, fade * (220.0f / 255.0f)));
            }

            DrawRectangleRec({layout.x, bar_top, layout.width, layout.height},
                             {15, 15, 25, 180});

            const int filled_segs  = static_cast<int>(
                visual.fill * static_cast<float>(segment_count) + 0.5f);
            const int visible_segs = static_cast<int>(
                visual.visible_level * static_cast<float>(segment_count) + 0.5f);

            for (int i = 0; i < segment_count; ++i) {
                const float seg_y = layout.bottom
                    - (i + 1) * (seg_h + layout.segment_gap)
                    + layout.segment_gap;
                const float t = static_cast<float>(i)
                    / static_cast<float>(segment_count > 1 ? segment_count - 1 : 1);
                unsigned char cr = 0, cg = 0, cb = 0;
                if (t < 0.33f) {
                    const float s = t / 0.33f;
                    cr = 255;
                    cg = static_cast<unsigned char>(80.0f + s * 175.0f);
                    cb = static_cast<unsigned char>(30.0f + s * 30.0f);
                } else if (t < 0.66f) {
                    const float s = (t - 0.33f) / 0.33f;
                    cr = static_cast<unsigned char>(255.0f - s * 255.0f);
                    cg = 255;
                    cb = static_cast<unsigned char>(60.0f + s * 195.0f);
                } else {
                    const float s = (t - 0.66f) / 0.34f;
                    cr = static_cast<unsigned char>(40.0f + s * 120.0f);
                    cg = static_cast<unsigned char>(255.0f - s * 120.0f);
                    cb = 255;
                }

                if (i < filled_segs) {
                    const float red_boost = visual.flash_ratio * 0.45f
                                          + visual.critical_intensity * 0.35f;
                    const float cool_dim  = visual.critical_intensity * 0.40f;
                    const unsigned char rr = static_cast<unsigned char>(
                        Clamp(Lerp(static_cast<float>(cr), 255.0f, red_boost),
                              0.0f, 255.0f));
                    const unsigned char rg = static_cast<unsigned char>(
                        Clamp(Lerp(static_cast<float>(cg), 0.0f, cool_dim),
                              0.0f, 255.0f));
                    const unsigned char rb = static_cast<unsigned char>(
                        Clamp(Lerp(static_cast<float>(cb), 0.0f, cool_dim),
                              0.0f, 255.0f));
                    DrawRectangleRec({layout.x, seg_y, layout.width, seg_h},
                                     {rr, rg, rb, 255});
                } else if (i < visible_segs) {
                    const float fade = 1.0f - static_cast<float>(i - filled_segs)
                        / std::max(1.0f, static_cast<float>(visible_segs - filled_segs));
                    DrawRectangleRec({layout.x, seg_y, layout.width, seg_h},
                        Fade({cr, cg, cb, 255}, fade * (200.0f / 255.0f)));
                } else {
                    DrawRectangleRec({layout.x, seg_y, layout.width, seg_h},
                                     {35, 35, 50, 50});
                }
            }

            if (visual.flash_overlay > 0.0f) {
                DrawRectangleRec({layout.x - 1.0f, bar_top - 1.0f,
                                  layout.width + 2.0f, layout.height + 2.0f},
                    Fade({255, 80, 80, 255},
                         visual.flash_overlay * (140.0f / 255.0f)));
            }

            const float border_thickness = 1.0f + visual.critical_intensity * 2.0f;
            const unsigned char border_r = static_cast<unsigned char>(
                80.0f + visual.critical_intensity * 175.0f);
            const unsigned char border_g = static_cast<unsigned char>(
                80.0f - visual.critical_intensity * 40.0f);
            const unsigned char border_b = static_cast<unsigned char>(
                100.0f - visual.critical_intensity * 60.0f);
            const unsigned char border_a = static_cast<unsigned char>(
                140.0f + visual.critical_intensity * 90.0f);
            DrawRectangleLinesEx(
                {layout.x, bar_top, layout.width, layout.height},
                border_thickness,
                {border_r, border_g, border_b, border_a});
        }
    }

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, saved_label_alignment);
    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);
}

} // namespace

void ui_render_system(entt::registry& reg, float /*alpha*/) {
    auto* text_ctx = reg.ctx().find<TextContext>();
    const auto& st = reg.ctx().get<ScreenTransform>();
    auto& ui_cam = ui_camera(reg).cam;

    ClearBackground(BLANK);
    BeginMode2D(ui_cam);

    // Popups (skipped entirely when fonts failed to load — issue #1619 made
    // presence of the `TextContext` ctx singleton IS the "fonts loaded"
    // predicate, eradicating the parallel-bool `loaded` NULL column).
    if (text_ctx != nullptr) {
        auto view = reg.view<PopupDisplay, ScreenPosition, TagHUDPass>();
        for (auto [entity, pd, sp] : view.each()) {
            if (pd.text[0] == '\0') {
                continue;
            }

            const Font& font = popup_font_for_size(*text_ctx, pd.font_size);
            const float font_size = static_cast<float>(font.baseSize);
            const float spacing = 1.0f;

            // PopupTextMeasured presence + key-match IS the cache-valid
            // predicate (issue #1549, Fabian Principle 3). Absence or
            // key-mismatch triggers a re-measurement that emplaces / replaces
            // the row with the freshly observed `(font_base_size,
            // font_texture_id, half_width)` tuple.
            const auto* measured = reg.try_get<PopupTextMeasured>(entity);
            float half_width;
            if (measured != nullptr &&
                measured->font_base_size == font.baseSize &&
                measured->font_texture_id == font.texture.id) {
                half_width = measured->half_width;
            } else {
                const Vector2 size = MeasureTextEx(font, pd.text, font_size, spacing);
                half_width = size.x / 2.0f;
                reg.emplace_or_replace<PopupTextMeasured>(
                    entity,
                    PopupTextMeasured{font.baseSize, font.texture.id, half_width});
            }

            DrawTextEx(font, pd.text, {sp.x - half_width, sp.y},
                       font_size, spacing, {pd.r, pd.g, pd.b, pd.a});
        }
    }

    // Overlay (if active)
    if (reg.ctx().contains<GamePhasePausedTag>()) {
        DrawRectangleRec({0, 0, constants::SCREEN_W_F, constants::SCREEN_H_F},
                         {0, 0, 0, 160});
    }

    // Make raygui hit-testing use virtual-space coordinates even when the
    // window is letterboxed/scaled relative to the fixed 720x1280 UI space.
    if (st.scale > 0.0f) {
        SetMouseOffset(static_cast<int>(std::lround(-st.offset_x)),
                       static_cast<int>(std::lround(-st.offset_y)));
        const float inv_scale = 1.0f / st.scale;
        SetMouseScale(inv_scale, inv_scale);
    }

    // Entity-driven render pass — issue #1287. Iterates every spawned UI
    // entity for the active screen (the screen lifecycle system already
    // partitions by `GamePhase*Tag`, so this pass is screen-agnostic).

    render_ui_entities(reg);

    // Restore raw mouse transform for non-UI systems in subsequent frames.
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);

    EndMode2D();
}
