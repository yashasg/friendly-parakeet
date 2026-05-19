#pragma once

#include <array>
#include <cstddef>
#include <cstdio>

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
//   * `UiAction*Tag`        — buttons only. Presence selects press behavior
//                             without dispatching on `ActionId`.
//
// Render and input systems consume these rows directly from the registry.

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

// One-source helper for the common "format a plain int into the slot's
// `UiLabel`" pattern used by terminal scoreboard binders (#1436 family —
// game_over / song_complete `bind_score` / `bind_high_score`). Lives in
// the components header so per-`*BindContext` binders can share one
// implementation without reintroducing an anonymous-namespace symbol
// name reuse across CMake Unity chunks (#1329).
inline void ui_label_set_int(UiLabel& label, int value) noexcept {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", value);
    ui_label_set(label, buf);
}

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

// Per-entity font-size override for `UiLabel` rendering (issue #1297).
// When absent the renderer falls back to the bounds-height heuristic
// (height ≥ 80 ⇒ 56, else 24). Written by per-screen bind systems for
// slots that need explicit sizes (gameplay HUD score / high score /
// chain / energy label). `size <= 0` is ignored (treated as "use the
// fallback") to keep the component cheap to default-construct.
struct UiLabelFontSize {
    int size = 0;
};

// Per-entity alpha override for `UiLabel` rendering (issue #1297). When
// absent the renderer uses 1.0. Written by per-screen bind systems for
// slots that need dimming (gameplay HUD high score 0.7, chain 0.7/0.95,
// energy label 0.8). Values outside [0, 1] are clamped at render time.
struct UiLabelAlpha {
    float value = 1.0f;
};

// Per-lane-button approach ring (issue #1297). Emplaced + updated each
// frame by `approach_ring_envelope_system` on every `LaneButtonTag`
// entity. The component is the contract between the envelope system
// (which knows the beat scheduler and obstacle distances) and the lane
// button render pass (which knows the button geometry).
//
// `ApproachRing` is the per-button envelope written by
// `approach_ring_envelope_system`; presence + the
// `ApproachRingVisibleTag` companion (in `app/tags/tags.h`) together
// drive the render pass. When the envelope is inactive the system
// removes the tag (and clears the row); the renderer joins on the tag
// so the per-frame check is a view filter, not a `try_get` + bool branch.
//   - `radius` is the ring radius around the button center.
//   - `alpha_scale` is the global multiplier on the ring color alpha.
//   - `color_*` are the precomputed tier color (Far / Near / Perfect —
//     the discriminator on the original `GameplayHudRingCue` is collapsed
//     into the data the renderer needs, matching Fabian's principle that
//     the consumer should not branch on labels).
struct ApproachRing {
    float         radius      = 0.0f;
    float         alpha_scale = 0.0f;
    unsigned char color_r     = 0;
    unsigned char color_g     = 0;
    unsigned char color_b     = 0;
    unsigned char color_a     = 0;
};
