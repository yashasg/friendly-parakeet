#pragma once

#include "actions.h"

#include <array>
#include <cstddef>
#include <cstring>

// Atomic UI components produced by the rguilayout codegen pipeline (#1193).
// Each control row in a `.rgl` layout file becomes one ECS entity carrying:
//
//   * a per-screen tag      — `TitleScreenTag` / `SettingsScreenTag` / …
//                             (see `app/tags/tags.h`). Tag-set membership IS
//                             the "what screen does this control belong to"
//                             data.
//   * a per-kind tag        — `UiLabelTag` / `UiButtonTag` / `UiDummyRecTag`
//                             (see `app/tags/tags.h`). Replaces the spec's
//                             `UiKind` enum with existential processing so
//                             no system dispatches on a discriminator.
//   * `UiPosition`          — pixel offset (x, y) relative to the anchor.
//   * `UiBounds`            — pixel (w, h).
//   * `UiLabel`             — display text. Fixed-size char buffer (no heap),
//                             matching the `PopupDisplay::text` convention
//                             elsewhere in this codebase.
//   * `OnPress`             — buttons only. Carries the `ActionId` that the
//                             UI-input system will dispatch on press.
//
// No system consumes these components yet. The render / input systems that
// will read them are out of scope per #1193; see the issue body for the
// follow-up issue list.

inline constexpr std::size_t kUiLabelMaxLength = 63;

struct UiPosition {
    float x = 0.0f;
    float y = 0.0f;
};

struct UiBounds {
    float w = 0.0f;
    float h = 0.0f;
};

// Display text storage. Fixed-size buffer (kUiLabelMaxLength + null terminator)
// avoids heap allocation in the per-frame render path. Empty string indicates
// a dynamic-text slot whose value is bound at render time by the consumer
// system (`ScoreSlot`, `AudioOffsetDisplay`, etc.).
struct UiLabel {
    std::array<char, kUiLabelMaxLength + 1> text{};
};

// Helper for codegen and callers: copy a literal into `UiLabel::text` with
// truncation rather than UB on overflow. Codegen verifies length at codegen
// time, but the helper keeps runtime callers honest.
inline void ui_label_set(UiLabel& label, const char* src) noexcept {
    if (src == nullptr) {
        label.text[0] = '\0';
        return;
    }
    const std::size_t cap = label.text.size() - 1;
    std::size_t i = 0;
    for (; i < cap && src[i] != '\0'; ++i) {
        label.text[i] = src[i];
    }
    label.text[i] = '\0';
}

struct OnPress {
    ActionId action = ActionId::None;
};

// Per-entity row index for dynamic-spawn UI archetypes (level-select cards
// and difficulty buttons, issue #1296). Each level-select card carries
// `LevelIndex{i}` for i in [0, content_config::LEVEL_COUNT); each
// difficulty button carries both `LevelIndex` (which card it belongs to)
// and `DifficultyIndex{d}` for d in [0, content_config::DIFFICULTY_COUNT).
// Render and input systems use these to drive the "active card" selection
// against `LevelSelectState::selected_level/selected_difficulty` without
// any switch on an enum discriminator (Fabian Principle 1).
struct LevelIndex {
    int value = 0;
};

struct DifficultyIndex {
    int value = 0;
};

// Two-state toggle data for buttons carrying `UiToggleTag` (Settings
// screen, issue #1295). The bind system (e.g.
// `settings_screen_bind_system`) writes the current ON/OFF state each
// frame from the underlying domain data (`SettingsState::haptics_enabled`,
// `!SettingsState::reduce_motion`, …). `ui_render_system` reads it to
// pick the two-cue colour + icon-prefix row.
struct UiToggleState {
    bool on = false;
};
