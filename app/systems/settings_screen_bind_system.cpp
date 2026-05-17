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

void format_offset(int16_t offset_ms, UiLabel& label) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%+d ms", static_cast<int>(offset_ms));
    ui_label_set(label, buf);
}

void format_toggle(UiLabel& label, const char* name, bool on) {
    // Two-cue ON/OFF (issue #390): icon-prefix `[X]` vs `[ ]` plus the
    // explicit ON/OFF suffix. The colour cue lives in `ui_render_system`
    // (`UiToggleTag` view selects the green/grey row).
    char buf[32];
    std::snprintf(buf, sizeof(buf), "[%s] %s: %s",
                  on ? "X" : " ", name, on ? "ON" : "OFF");
    ui_label_set(label, buf);
}

}  // namespace

void settings_screen_bind_system(entt::registry& reg) {
    auto view = reg.view<SettingsScreenTag, UiPosition, UiBounds, UiLabel>();
    if (view.begin() == view.end()) return;

    const auto* state = find_settings_state(reg);
    if (state == nullptr) return;

    const bool haptics_on = state->haptics_enabled;
    // Display-side inversion: the visible toggle reads "MOTION: ON" when
    // motion is NOT reduced. Matches the legacy controller (#1295 spec).
    const bool motion_on  = !state->reduce_motion;

    for (auto entity : view) {
        const auto& pos = view.get<UiPosition>(entity);
        auto& label = view.get<UiLabel>(entity);

        if (pos.x == kAudioOffsetDisplayX && pos.y == kAudioOffsetDisplayY) {
            format_offset(state->audio_offset_ms, label);
        } else if (pos.x == kHapticsToggleX && pos.y == kHapticsToggleY) {
            format_toggle(label, "HAPTICS", haptics_on);
            reg.emplace_or_replace<UiToggleState>(entity, UiToggleState{haptics_on});
        } else if (pos.x == kReduceMotionToggleX && pos.y == kReduceMotionToggleY) {
            format_toggle(label, "MOTION", motion_on);
            reg.emplace_or_replace<UiToggleState>(entity, UiToggleState{motion_on});
        }
    }
}
