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

#include "../ui/screen_controllers/title_screen_controller.h"
#include "../ui/screen_controllers/settings_screen_controller.h"
#include "../ui/screen_controllers/level_select_screen_controller.h"
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

    // Buttons — rendered with GuiButton. Press dispatch lives in
    // `ui_update_system`; ignoring the return value keeps that path the
    // single source of truth for button activation.
    {
        GuiSetStyle(DEFAULT, TEXT_SIZE, kEntityButtonTextSize);
        auto view = reg.view<UiPosition, UiBounds, UiLabel, UiButtonTag>();
        for (auto [e, pos, sz, label] : view.each()) {
            (void)e;
            (void)GuiButton(Rectangle{pos.x, pos.y, sz.w, sz.h}, label.text.data());
        }
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
    // Over migrated in #1293. The remaining four screens migrate in
    // follow-up sub-issues — see #1287.
    const auto& ctx = reg.ctx();
    if (ctx.contains<GamePhaseTitleTag>())        { render_title_screen_ui(reg); }
    if (ctx.contains<GamePhaseLevelSelectTag>())  { render_level_select_screen_ui(reg); }
    if (ctx.contains<GamePhasePlayingTag>())      { render_gameplay_hud_screen_ui(reg); }
    if (ctx.contains<GamePhaseSettingsTag>())     { render_settings_screen_ui(reg); }

    // Restore raw mouse transform for non-UI systems in subsequent frames.
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);

    EndMode2D();
}
