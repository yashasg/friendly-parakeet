// Per-phase ctx-tag mirror invariant — issue #1202 / #1204 (GamePhase PR A).
//
// Per Fabian's existential processing chapter, each former enum value gets its
// own zero-column table. For GamePhase, the value-typed `gs.phase` field is
// mirrored 1:1 by a ctx-tag — exactly one of GamePhase{Title,...,Tutorial}Tag
// is present on `reg.ctx()` at any time. `enter_phase()` and the public
// `sync_game_phase_tags()` are the only writers that keep the mirror correct;
// these tests pin the invariant so the mirror cannot drift as subsequent
// migration PRs migrate consumers off `gs.phase`.
//
// PR C (`NextPhase*Tag`) adds a second mirror for the pending-transition
// target (`gs.next_phase`). The mirror is pulsed by `sync_next_phase_tags()`
// at the top of the dispatch block and drained by `clear_next_phase_tags()`
// at the bottom — exactly one `NextPhase*Tag` between those two points,
// zero outside the block. These tests also pin that invariant so the
// pending-transition dispatch (per-tag transforms in `game_state_system`)
// cannot drift either.

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

#include "components/game_state.h"
#include "systems/game_phase_transition.h"

namespace {

int count_phase_tags(entt::registry& reg) {
    auto& ctx = reg.ctx();
    int n = 0;
    if (ctx.find<GamePhaseTitleTag>())        ++n;
    if (ctx.find<GamePhaseLevelSelectTag>())  ++n;
    if (ctx.find<GamePhasePlayingTag>())      ++n;
    if (ctx.find<GamePhasePausedTag>())       ++n;
    if (ctx.find<GamePhaseGameOverTag>())     ++n;
    if (ctx.find<GamePhaseSongCompleteTag>()) ++n;
    if (ctx.find<GamePhaseSettingsTag>())     ++n;
    if (ctx.find<GamePhaseTutorialTag>())     ++n;
    return n;
}

int count_next_phase_tags(entt::registry& reg) {
    auto& ctx = reg.ctx();
    int n = 0;
    if (ctx.find<NextPhaseTitleTag>())        ++n;
    if (ctx.find<NextPhaseLevelSelectTag>())  ++n;
    if (ctx.find<NextPhasePlayingTag>())      ++n;
    if (ctx.find<NextPhasePausedTag>())       ++n;
    if (ctx.find<NextPhaseGameOverTag>())     ++n;
    if (ctx.find<NextPhaseSongCompleteTag>()) ++n;
    if (ctx.find<NextPhaseSettingsTag>())     ++n;
    if (ctx.find<NextPhaseTutorialTag>())     ++n;
    return n;
}

}  // namespace

TEST_CASE("game_phase_tags: sync emplaces exactly the matching tag",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;
    reg.ctx().emplace<GameState>();

    sync_game_phase_tags(reg, GamePhase::Title);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseTitleTag>() != nullptr);

    sync_game_phase_tags(reg, GamePhase::Playing);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseTitleTag>() == nullptr);
    CHECK(reg.ctx().find<GamePhasePlayingTag>() != nullptr);

    sync_game_phase_tags(reg, GamePhase::GameOver);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseGameOverTag>() != nullptr);

    sync_game_phase_tags(reg, GamePhase::SongComplete);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseSongCompleteTag>() != nullptr);
}

TEST_CASE("game_phase_tags: enter_phase keeps the tag mirror in lockstep",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;
    auto& gs = reg.ctx().emplace<GameState>();
    sync_game_phase_tags(reg, gs.phase);
    REQUIRE(reg.ctx().find<GamePhaseTitleTag>() != nullptr);

    enter_phase(reg, gs, GamePhase::Playing);
    CHECK(gs.phase == GamePhase::Playing);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhasePlayingTag>() != nullptr);
    CHECK(reg.ctx().find<GamePhaseTitleTag>() == nullptr);

    enter_phase(reg, gs, GamePhase::Paused);
    CHECK(gs.phase == GamePhase::Paused);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhasePausedTag>() != nullptr);

    enter_phase(reg, gs, GamePhase::GameOver);
    CHECK(gs.phase == GamePhase::GameOver);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseGameOverTag>() != nullptr);

    enter_phase(reg, gs, GamePhase::Tutorial);
    CHECK(gs.phase == GamePhase::Tutorial);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseTutorialTag>() != nullptr);
}

TEST_CASE("game_phase_tags: every phase value has a matching tag",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;
    auto& gs = reg.ctx().emplace<GameState>();

    const GamePhase all[] = {
        GamePhase::Title,
        GamePhase::LevelSelect,
        GamePhase::Playing,
        GamePhase::Paused,
        GamePhase::GameOver,
        GamePhase::SongComplete,
        GamePhase::Settings,
        GamePhase::Tutorial,
    };
    for (GamePhase p : all) {
        enter_phase(reg, gs, p);
        CHECK(count_phase_tags(reg) == 1);
    }
}

TEST_CASE("next_phase_tags: sync emplaces exactly the matching tag",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;

    sync_next_phase_tags(reg, GamePhase::Title);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseTitleTag>() != nullptr);

    sync_next_phase_tags(reg, GamePhase::Playing);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseTitleTag>() == nullptr);
    CHECK(reg.ctx().find<NextPhasePlayingTag>() != nullptr);

    sync_next_phase_tags(reg, GamePhase::GameOver);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseGameOverTag>() != nullptr);

    sync_next_phase_tags(reg, GamePhase::SongComplete);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseSongCompleteTag>() != nullptr);
}

TEST_CASE("next_phase_tags: clear drains every tag",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;
    sync_next_phase_tags(reg, GamePhase::Tutorial);
    REQUIRE(count_next_phase_tags(reg) == 1);

    clear_next_phase_tags(reg);
    CHECK(count_next_phase_tags(reg) == 0);

    // Idempotent — clearing an already-clear mirror is a no-op.
    clear_next_phase_tags(reg);
    CHECK(count_next_phase_tags(reg) == 0);
}

TEST_CASE("next_phase_tags: every phase value has a matching tag",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;

    const GamePhase all[] = {
        GamePhase::Title,
        GamePhase::LevelSelect,
        GamePhase::Playing,
        GamePhase::Paused,
        GamePhase::GameOver,
        GamePhase::SongComplete,
        GamePhase::Settings,
        GamePhase::Tutorial,
    };
    for (GamePhase p : all) {
        sync_next_phase_tags(reg, p);
        CHECK(count_next_phase_tags(reg) == 1);
    }
}
