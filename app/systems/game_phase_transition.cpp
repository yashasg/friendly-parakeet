#include "game_phase_transition.h"

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
#include <emscripten/emscripten.h>
#endif

#include <string>

namespace {

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
const char* game_phase_name(GamePhase phase) {
    switch (phase) {
        case GamePhase::Title:
            return "Title";
        case GamePhase::LevelSelect:
            return "LevelSelect";
        case GamePhase::Playing:
            return "Playing";
        case GamePhase::Paused:
            return "Paused";
        case GamePhase::GameOver:
            return "GameOver";
        case GamePhase::SongComplete:
            return "SongComplete";
        case GamePhase::Settings:
            return "Settings";
        case GamePhase::Tutorial:
            return "Tutorial";
        default:
            return "Unknown";
    }
}

void update_web_title(GamePhase phase) {
    const std::string title = std::string("SHAPESHIFTER [") + game_phase_name(phase) + "]";
    EM_ASM(
        {
            if (typeof document !== 'undefined') {
                document.title = UTF8ToString($0);
            }
        },
        title.c_str());
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

void enter_phase(entt::registry& reg, GameState& gs, GamePhase next_phase) {
    gs.phase = next_phase;
    gs.phase_timer = 0.0f;
    sync_game_phase_tags(reg, gs.phase);
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
    update_web_title(gs.phase);
#endif
}
