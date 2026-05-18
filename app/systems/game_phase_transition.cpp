#include "game_phase_transition.h"

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
#include <emscripten/emscripten.h>
#include <string>
#endif

namespace phase_transition_detail {

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
namespace {

void set_web_title(const char* phase_name) {
    const std::string title = std::string("SHAPESHIFTER [") + phase_name + "]";
    EM_ASM(
        {
            if (typeof document !== 'undefined') {
                document.title = UTF8ToString($0);
            }
        },
        title.c_str());
}

}  // namespace
#endif

void notify_phase_changed(entt::registry& reg) {
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
    // Per Fabian's existential processing (issue #1202/#1204, PR G): the former
    // eight-case `switch (phase)` over the WASM smoke title is replaced by per-tag
    // transforms on the `GamePhase*Tag` ctx mirror seeded by `enter_phase<...>()`.
    // The mirror invariant pinned by `tests/test_game_phase_tags.cpp` guarantees
    // that exactly one `GamePhase*Tag` is present at any time, so these eight
    // `if` blocks are mutually exclusive even without `else` chaining and
    // dispatch on tag presence rather than on an enum compare.
    const auto& ctx = reg.ctx();
    if (ctx.contains<GamePhaseTitleTag>())        { set_web_title("Title"); }
    if (ctx.contains<GamePhaseLevelSelectTag>())  { set_web_title("LevelSelect"); }
    if (ctx.contains<GamePhasePlayingTag>())      { set_web_title("Playing"); }
    if (ctx.contains<GamePhasePausedTag>())       { set_web_title("Paused"); }
    if (ctx.contains<GamePhaseGameOverTag>())     { set_web_title("GameOver"); }
    if (ctx.contains<GamePhaseSongCompleteTag>()) { set_web_title("SongComplete"); }
    if (ctx.contains<GamePhaseSettingsTag>())     { set_web_title("Settings"); }
    if (ctx.contains<GamePhaseTutorialTag>())     { set_web_title("Tutorial"); }
#else
    (void)reg;
#endif
}

}  // namespace phase_transition_detail
