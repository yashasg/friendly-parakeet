#include "all_systems.h"
#include "camera_system.h"
#include "../components/actions.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/rhythm.h"
#include "../components/game_state.h"
#include "../components/song_state.h"
#include "../components/text_resources.h"
#include "../components/ui.h"
#include "../constants.h"
#include "../tags/tags.h"
#include "../util/level_content_config.h"
#include "../util/shape_draw_2d.h"

#include "../ui/screen_controllers/gameplay_hud_screen_controller.h"
#include <raygui.h>
#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

// ═════════════════════════════════════════════════════════════════════════════
// UI render system — 2D overlay pass
// ═════════════════════════════════════════════════════════════════════════════

namespace {

const Font& popup_font_for_size(const TextContext& ctx, FontSize font_size) {
    switch (font_size) {
        case FontSize::Small:  return ctx.font_small;
        case FontSize::Medium: return ctx.font_medium;
        case FontSize::Large:  return ctx.font_large;
    }
    return ctx.font_medium;
}

// ── Entity-driven UI render (issue #1287) ──────────────────────────────────
//
// Iterates the per-kind tag views populated by the rguilayout codegen
// (`spawn_<screen>_screen()`, see `app/ui/generated/`). Per-screen membership
// is the per-screen tag on each entity; the screen lifecycle system
// (`app/systems/screen_lifecycle_system.h`) keeps the entity set in sync
// with the active `GamePhase*Tag`, so this pass only sees the active
// screen's entities and trivially partitions by tag.
//
// Per-screen visual specifics (font size, alignment, label vs centered
// label, button colour overrides) currently match the legacy raygui
// defaults; per-screen visual tweaks migrate row-by-row as their screen
// migrates off the legacy controller path. See #1287 for the per-screen
// sub-issue ladder.
//
// Migration boundary: a button press dispatch lives in
// `app/systems/ui_update_system.cpp` — that system hit-tests against
// `view<UiPosition, UiBounds, OnPress, UiButtonTag>()`. The GuiButton call
// here renders only; its return value is ignored.

constexpr int kEntityLabelTextSize  = 24;
constexpr int kEntityButtonTextSize = 28;
// Paused overlay headline ("PAUSED") needs a larger size than the default
// raygui DEFAULT::TEXT_SIZE; matched against the legacy
// `paused_layout.h` constant.
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
    {
        auto view = reg.view<UiPosition, UiBounds, UiLabel, UiLabelTag>();
        for (auto [e, pos, sz, label] : view.each()) {
            (void)e;
            if (label.text[0] == '\0') continue;
            const int text_size = (sz.h >= kPausedHeadlineHeight)
                                ? kPausedHeadlineSize
                                : kEntityLabelTextSize;
            GuiSetStyle(DEFAULT, TEXT_SIZE, text_size);
            GuiLabel(Rectangle{pos.x, pos.y, sz.w, sz.h}, label.text.data());
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
    {
        constexpr Color kShapeIconColor{200, 230, 255, 255};

        auto draw_for_tag = []<typename Tag>(entt::registry& r,
                                             Shape shape,
                                             Color color) {
            auto v = r.view<UiPosition, UiBounds, Tag>();
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

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, saved_label_alignment);
    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);
}

} // namespace

void ui_render_system(entt::registry& reg, float /*alpha*/) {
    auto& text_ctx = reg.ctx().get<TextContext>();
    const auto& st = reg.ctx().get<ScreenTransform>();
    auto& ui_cam = ui_camera(reg).cam;

    ClearBackground(BLANK);
    BeginMode2D(ui_cam);

    // Popups
    {
        auto view = reg.view<PopupDisplay, ScreenPosition, TagHUDPass>();
        for (auto [entity, pd, sp] : view.each()) {
            (void)entity;
            if (!text_ctx.loaded || pd.text[0] == '\0') {
                continue;
            }

            const Font& font = popup_font_for_size(text_ctx, pd.font_size);
            const float font_size = static_cast<float>(font.baseSize);
            const float spacing = 1.0f;
            if (pd.measured_font_base_size != font.baseSize ||
                pd.measured_font_texture_id != font.texture.id) {
                const Vector2 size = MeasureTextEx(font, pd.text, font_size, spacing);
                pd.text_half_width = size.x / 2.0f;
                pd.measured_font_base_size = font.baseSize;
                pd.measured_font_texture_id = font.texture.id;
            }
            DrawTextEx(font, pd.text, {sp.x - pd.text_half_width, sp.y},
                       font_size, spacing, {pd.r, pd.g, pd.b, pd.a});
        }
    }

    // Overlay (if active)
    if (reg.ctx().contains<GamePhasePausedTag>()) {
        DrawRectangleRec({0, 0, constants::SCREEN_W_F, constants::SCREEN_H_F},
                         {0, 0, 0, 160});
    }

    // UI rendering is handled by the entity-driven `render_ui_entities`
    // call below (issue #1287). Screens still on the legacy controller
    // path render via the per-phase dispatch further down; each will be
    // migrated off in a follow-up sub-issue.

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
    // Per #1287 the legacy `render_<screen>_screen_ui` calls below are
    // removed one at a time as each screen migrates off the
    // `screen_controllers/*` path. Paused is the pilot.
    render_ui_entities(reg);

    // Dynamic screens that still need specialized rendering.
    //
    // Per Fabian's existential processing (issue #1202/#1204, PR D): each
    // former `case GamePhase::X` is its own per-tag transform on the ctx-tag
    // mirror seeded by `enter_phase<...>()`. The mirror
    // invariant pinned by `tests/test_game_phase_tags.cpp` guarantees that
    // exactly one `GamePhase*Tag` is present at any time, so these `if`
    // blocks are mutually exclusive even without `else` chaining and
    // dispatch on tag presence rather than on an enum compare.
    //
    // Paused was migrated to the entity-driven path (#1287 pilot);
    // Tutorial migrated in #1291; Song Complete migrated in #1292; Game
    // Over migrated in #1293; Title migrated in #1294; Settings migrated
    // in #1295; Level Select migrated in #1296. The remaining one screen
    // (Gameplay HUD) migrates in #1297; at which point this `if` block
    // drops out entirely.
    const auto& ctx = reg.ctx();
    if (ctx.contains<GamePhasePlayingTag>())      { render_gameplay_hud_screen_ui(reg); }

    // Restore raw mouse transform for non-UI systems in subsequent frames.
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);

    EndMode2D();
}
