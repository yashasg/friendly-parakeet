#pragma once

#include <cstdint>

// ActionId enumerates every button control name that appears across
// `content/ui/screens/*.rgl`. The rguilayout codegen verifies button names
// against this enum, then emits a matching `UiAction<Name>Tag` on the button
// entity (see `tools/rguilayout/codegen.py`).
//
// Convention (per #1193): the control name in the .rgl IS the ActionId
// enumerator name. Two buttons named identically on different screens map to
// the same ActionId and therefore the same generated action tag; consumer
// systems can further disambiguate by reading per-screen tags when needed
// (`GameOverScreenTag` vs `SongCompleteScreenTag`, etc., from
// `app/tags/tags.h`).
//
// Consumer systems do not dispatch on ActionId. It is a codegen validation
// label only; runtime press behavior is selected by ECS tag membership.
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
