#include "settings_screen_bind_system.h"

#include "../components/settings.h"
#include "../components/ui.h"
#include "../tags/tags.h"
#include "settings_system.h"

#include <array>
#include <cstdio>

namespace {

// Canonical slot positions from `content/ui/screens/settings.rgl`. Position-
// keyed identification matches the convention established for the
// SongComplete / Tutorial / GameOver binders — codegen bakes these into
// the spawn block, so a layout edit that moves a slot will visibly break
// placement before any bind ever misses silently.
constexpr float kAudioOffsetDisplayX = 252.0f;
constexpr float kAudioOffsetDisplayY = 560.0f;
constexpr float kHapticsToggleX      = 152.0f;
constexpr float kHapticsToggleY      = 720.0f;
constexpr float kReduceMotionToggleX = 152.0f;
constexpr float kReduceMotionToggleY = 880.0f;

// Bind context — owns the per-frame singleton reads so the per-slot
// `bind_*` functions are pure transforms over their slot's `UiLabel` +
// `UiToggleState`. Mirrors the `*BindContext` shape used by the sibling
// `game_over_scoreboard_bind_system` / `song_complete_scoreboard_bind_system` /
// `gameplay_hud_bind_system` binders.
//
// Per-binder prefix (`Settings*`) avoids an anonymous-namespace ODR
// collision with the sibling `*_bind_system.cpp` files when CMake Unity
// chunking happens to place two binders in the same jumbo TU (issue #1329).
struct SettingsBindContext {
    int16_t audio_offset_ms;
    bool    haptics_on;
    // Display-side inversion: the visible toggle reads "MOTION: ON" when
    // motion is NOT reduced (#1295 spec).
    bool    motion_on;
};

void bind_audio_offset(const SettingsBindContext& ctx, entt::registry& /*reg*/,
                       entt::entity /*e*/, UiLabel& label) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%+d ms",
                  static_cast<int>(ctx.audio_offset_ms));
    ui_label_set(label, buf);
}

// Shared body for the two toggle binders below. Differs across the two
// callers only in the `name` literal + the `on` bool source on `ctx`;
// folded so the per-tier symbols stay distinct (the position-keyed
// `kSettingsScreenSlots` table references them as function pointers)
// without two byte-identical bodies.
void bind_toggle_impl(entt::registry& reg, entt::entity e, UiLabel& label,
                      const char* name, bool on) {
    // Two-cue ON/OFF (issue #390): icon-prefix `[X]` vs `[ ]` plus the
    // explicit ON/OFF suffix. The colour cue lives in `ui_render_system`
    // (`UiToggleTag` view selects the green/grey row).
    char buf[32];
    std::snprintf(buf, sizeof(buf), "[%s] %s: %s",
                  on ? "X" : " ", name, on ? "ON" : "OFF");
    ui_label_set(label, buf);
    reg.emplace_or_replace<UiToggleState>(e, UiToggleState{on});
}

void bind_haptics_toggle(const SettingsBindContext& ctx, entt::registry& reg,
                         entt::entity e, UiLabel& label) {
    bind_toggle_impl(reg, e, label, "HAPTICS", ctx.haptics_on);
}

void bind_motion_toggle(const SettingsBindContext& ctx, entt::registry& reg,
                        entt::entity e, UiLabel& label) {
    bind_toggle_impl(reg, e, label, "MOTION", ctx.motion_on);
}

using SettingsSlotBindFn = void (*)(const SettingsBindContext&, entt::registry&,
                                    entt::entity, UiLabel&);

struct SettingsSlotRow {
    float x;
    float y;
    SettingsSlotBindFn fn;
};

constexpr std::array<SettingsSlotRow, 3> kSettingsScreenSlots = {{
    {kAudioOffsetDisplayX, kAudioOffsetDisplayY, &bind_audio_offset},
    {kHapticsToggleX,      kHapticsToggleY,      &bind_haptics_toggle},
    {kReduceMotionToggleX, kReduceMotionToggleY, &bind_motion_toggle},
}};

}  // namespace

void settings_screen_bind_system(entt::registry& reg) {
    auto view = reg.view<SettingsScreenTag, UiPosition, UiBounds, UiLabel>();
    if (view.begin() == view.end()) return;

    const auto* state = find_settings_state(reg);
    if (state == nullptr) return;

    const SettingsBindContext ctx{
        state->audio_offset_ms,
        state->haptics_enabled,
        !state->reduce_motion,
    };

    for (auto entity : view) {
        const auto& pos = view.get<UiPosition>(entity);
        auto& label = view.get<UiLabel>(entity);
        for (const auto& row : kSettingsScreenSlots) {
            if (pos.x == row.x && pos.y == row.y) {
                row.fn(ctx, reg, entity, label);
                break;
            }
        }
    }
}
