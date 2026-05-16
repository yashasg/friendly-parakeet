#pragma once

#include <cstdint>

// ActionId enumerates every button control name that appears across
// `content/ui/screens/*.rgl`. Carried by the `OnPress` component on button
// entities spawned by the rguilayout codegen (see `tools/rguilayout/codegen.py`).
//
// Convention (per #1193): the control name in the .rgl IS the ActionId
// enumerator name. Two buttons named identically on different screens map to
// the same ActionId; consumer systems disambiguate by also reading the
// entity's per-screen tag (`GameOverScreenTag` vs `SongCompleteScreenTag`,
// etc., from `app/tags/tags.h`).
//
// NOTE: no consumer system currently dispatches on ActionId. Building the
// UI-input system that reads this enum is the next ECS work item; see #1193
// "Out of scope" section for the follow-up issue list. Until then, the enum
// is data declaration only — no `switch` on it anywhere in `app/`.
//
// Maintenance: when a new button control name appears in a .rgl file the
// codegen verifies it against this enum and errors out if an enumerator is
// missing. Add the new enumerator here (sorted alphabetically) and regen.
enum class ActionId : uint8_t {
    None = 0,

    AudioOffsetMinus,
    AudioOffsetPlus,
    CloseButton,
    ContinueButton,
    DifficultyEasy,
    DifficultyHard,
    DifficultyMedium,
    ExitButton,
    HapticsToggle,
    LevelSelectButton,
    MenuButton,
    PauseButton,
    ReduceMotionToggle,
    RestartButton,
    ResumeButton,
    SettingsButton,
    StartButton,
};
