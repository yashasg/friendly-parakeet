#include "game_phase_transition.h"

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
#include <emscripten/emscripten.h>
#endif

#include <string>

namespace {

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
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

// Per Fabian's existential processing (issue #1202/#1204, PR E): the former
// eight-case `switch (phase)` over the WASM smoke title is replaced by per-tag
// transforms on the `GamePhase*Tag` ctx mirror seeded by
// `sync_game_phase_tags()`. The mirror invariant pinned by
// `tests/test_game_phase_tags.cpp` guarantees that exactly one
// `GamePhase*Tag` is present at any time, so these eight `if` blocks are
// mutually exclusive even without `else` chaining and dispatch on tag
// presence rather than on an enum compare. Callers must invoke this only
// after `sync_game_phase_tags()` so the mirror reflects the new phase.
void update_web_title(entt::registry& reg) {
    const auto& ctx = reg.ctx();
    if (ctx.contains<GamePhaseTitleTag>())        { set_web_title("Title"); }
    if (ctx.contains<GamePhaseLevelSelectTag>())  { set_web_title("LevelSelect"); }
    if (ctx.contains<GamePhasePlayingTag>())      { set_web_title("Playing"); }
    if (ctx.contains<GamePhasePausedTag>())       { set_web_title("Paused"); }
    if (ctx.contains<GamePhaseGameOverTag>())     { set_web_title("GameOver"); }
    if (ctx.contains<GamePhaseSongCompleteTag>()) { set_web_title("SongComplete"); }
    if (ctx.contains<GamePhaseSettingsTag>())     { set_web_title("Settings"); }
    if (ctx.contains<GamePhaseTutorialTag>())     { set_web_title("Tutorial"); }
}
#endif

void erase_all_game_phase_tags(entt::registry& reg) {
    auto& ctx = reg.ctx();
    if (ctx.find<GamePhaseTitleTag>())        ctx.erase<GamePhaseTitleTag>();
    if (ctx.find<GamePhaseLevelSelectTag>())  ctx.erase<GamePhaseLevelSelectTag>();
    if (ctx.find<GamePhasePlayingTag>())      ctx.erase<GamePhasePlayingTag>();
    if (ctx.find<GamePhasePausedTag>())       ctx.erase<GamePhasePausedTag>();
    if (ctx.find<GamePhaseGameOverTag>())     ctx.erase<GamePhaseGameOverTag>();
    if (ctx.find<GamePhaseSongCompleteTag>()) ctx.erase<GamePhaseSongCompleteTag>();
    if (ctx.find<GamePhaseSettingsTag>())     ctx.erase<GamePhaseSettingsTag>();
    if (ctx.find<GamePhaseTutorialTag>())     ctx.erase<GamePhaseTutorialTag>();
}

void erase_all_next_phase_tags(entt::registry& reg) {
    auto& ctx = reg.ctx();
    if (ctx.find<NextPhaseTitleTag>())        ctx.erase<NextPhaseTitleTag>();
    if (ctx.find<NextPhaseLevelSelectTag>())  ctx.erase<NextPhaseLevelSelectTag>();
    if (ctx.find<NextPhasePlayingTag>())      ctx.erase<NextPhasePlayingTag>();
    if (ctx.find<NextPhasePausedTag>())       ctx.erase<NextPhasePausedTag>();
    if (ctx.find<NextPhaseGameOverTag>())     ctx.erase<NextPhaseGameOverTag>();
    if (ctx.find<NextPhaseSongCompleteTag>()) ctx.erase<NextPhaseSongCompleteTag>();
    if (ctx.find<NextPhaseSettingsTag>())     ctx.erase<NextPhaseSettingsTag>();
    if (ctx.find<NextPhaseTutorialTag>())     ctx.erase<NextPhaseTutorialTag>();
}

}  // namespace

void sync_game_phase_tags(entt::registry& reg, GamePhase phase) {
    erase_all_game_phase_tags(reg);
    auto& ctx = reg.ctx();
    switch (phase) {
        case GamePhase::Title:        ctx.emplace<GamePhaseTitleTag>();        break;
        case GamePhase::LevelSelect:  ctx.emplace<GamePhaseLevelSelectTag>();  break;
        case GamePhase::Playing:      ctx.emplace<GamePhasePlayingTag>();      break;
        case GamePhase::Paused:       ctx.emplace<GamePhasePausedTag>();       break;
        case GamePhase::GameOver:     ctx.emplace<GamePhaseGameOverTag>();     break;
        case GamePhase::SongComplete: ctx.emplace<GamePhaseSongCompleteTag>(); break;
        case GamePhase::Settings:     ctx.emplace<GamePhaseSettingsTag>();     break;
        case GamePhase::Tutorial:     ctx.emplace<GamePhaseTutorialTag>();     break;
    }
}

void sync_next_phase_tags(entt::registry& reg, GamePhase phase) {
    erase_all_next_phase_tags(reg);
    auto& ctx = reg.ctx();
    switch (phase) {
        case GamePhase::Title:        ctx.emplace<NextPhaseTitleTag>();        break;
        case GamePhase::LevelSelect:  ctx.emplace<NextPhaseLevelSelectTag>();  break;
        case GamePhase::Playing:      ctx.emplace<NextPhasePlayingTag>();      break;
        case GamePhase::Paused:       ctx.emplace<NextPhasePausedTag>();       break;
        case GamePhase::GameOver:     ctx.emplace<NextPhaseGameOverTag>();     break;
        case GamePhase::SongComplete: ctx.emplace<NextPhaseSongCompleteTag>(); break;
        case GamePhase::Settings:     ctx.emplace<NextPhaseSettingsTag>();     break;
        case GamePhase::Tutorial:     ctx.emplace<NextPhaseTutorialTag>();     break;
    }
}

void clear_next_phase_tags(entt::registry& reg) {
    erase_all_next_phase_tags(reg);
}

void enter_phase(entt::registry& reg, GameState& gs, GamePhase next_phase) {
    gs.phase = next_phase;
    gs.phase_timer = 0.0f;
    sync_game_phase_tags(reg, gs.phase);
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
    update_web_title(reg);
#endif
}
