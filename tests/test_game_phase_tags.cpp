// Per-phase ctx-tag mirror invariant — issue #1202 / #1204 (GamePhase PR G).
//
// Per Fabian's existential processing chapter, each former enum value gets its
// own zero-column table. For the active game phase, exactly one of
// `GamePhase{Title,...,Tutorial}Tag` is present on `reg.ctx()` at any time;
// `enter_phase<...>()` and `sync_game_phase_tags<...>()` are the only writers
// that keep the mirror correct. These tests pin the invariant so it cannot
// drift as future systems migrate or new phases are introduced.
//
// A second per-tag table (`NextPhase*Tag`) carries the pending-transition
// target. The mirror is pulsed by `request_phase_transition<...>` at the
// request site and drained by `clear_next_phase_tags()` at the bottom of the
// dispatch block — exactly one `NextPhase*Tag` between those two points,
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

    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseTitleTag>() != nullptr);

    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseTitleTag>() == nullptr);
    CHECK(reg.ctx().find<GamePhasePlayingTag>() != nullptr);

    sync_game_phase_tags<GamePhaseGameOverTag>(reg);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseGameOverTag>() != nullptr);

    sync_game_phase_tags<GamePhaseSongCompleteTag>(reg);
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseSongCompleteTag>() != nullptr);
}

TEST_CASE("game_phase_tags: enter_phase keeps the tag mirror in lockstep",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;
    reg.ctx().emplace<GameState>();
    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    REQUIRE(reg.ctx().find<GamePhaseTitleTag>() != nullptr);

    enter_phase<GamePhasePlayingTag>(reg);
    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhasePlayingTag>() != nullptr);
    CHECK(reg.ctx().find<GamePhaseTitleTag>() == nullptr);

    enter_phase<GamePhasePausedTag>(reg);
    CHECK(reg.ctx().contains<GamePhasePausedTag>());
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhasePausedTag>() != nullptr);

    enter_phase<GamePhaseGameOverTag>(reg);
    CHECK(reg.ctx().contains<GamePhaseGameOverTag>());
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseGameOverTag>() != nullptr);

    enter_phase<GamePhaseTutorialTag>(reg);
    CHECK(reg.ctx().contains<GamePhaseTutorialTag>());
    CHECK(count_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<GamePhaseTutorialTag>() != nullptr);
}

TEST_CASE("game_phase_tags: every phase tag exercise keeps mirror count at one",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;
    reg.ctx().emplace<GameState>();

    enter_phase<GamePhaseTitleTag>(reg);        CHECK(count_phase_tags(reg) == 1);
    enter_phase<GamePhaseLevelSelectTag>(reg);  CHECK(count_phase_tags(reg) == 1);
    enter_phase<GamePhasePlayingTag>(reg);      CHECK(count_phase_tags(reg) == 1);
    enter_phase<GamePhasePausedTag>(reg);       CHECK(count_phase_tags(reg) == 1);
    enter_phase<GamePhaseGameOverTag>(reg);     CHECK(count_phase_tags(reg) == 1);
    enter_phase<GamePhaseSongCompleteTag>(reg); CHECK(count_phase_tags(reg) == 1);
    enter_phase<GamePhaseSettingsTag>(reg);     CHECK(count_phase_tags(reg) == 1);
    enter_phase<GamePhaseTutorialTag>(reg);     CHECK(count_phase_tags(reg) == 1);
}

TEST_CASE("next_phase_tags: request_phase_transition emplaces exactly the matching tag",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;

    request_phase_transition<NextPhaseTitleTag>(reg);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseTitleTag>() != nullptr);

    request_phase_transition<NextPhasePlayingTag>(reg);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseTitleTag>() == nullptr);
    CHECK(reg.ctx().find<NextPhasePlayingTag>() != nullptr);

    request_phase_transition<NextPhaseGameOverTag>(reg);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseGameOverTag>() != nullptr);

    request_phase_transition<NextPhaseSongCompleteTag>(reg);
    CHECK(count_next_phase_tags(reg) == 1);
    CHECK(reg.ctx().find<NextPhaseSongCompleteTag>() != nullptr);
}

TEST_CASE("next_phase_tags: clear drains every tag",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;
    request_phase_transition<NextPhaseTutorialTag>(reg);
    REQUIRE(count_next_phase_tags(reg) == 1);

    clear_next_phase_tags(reg);
    CHECK(count_next_phase_tags(reg) == 0);

    // Idempotent — clearing an already-clear mirror is a no-op.
    clear_next_phase_tags(reg);
    CHECK(count_next_phase_tags(reg) == 0);
}

TEST_CASE("next_phase_tags: every phase tag exercise keeps mirror count at one",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;

    request_phase_transition<NextPhaseTitleTag>(reg);        CHECK(count_next_phase_tags(reg) == 1);
    request_phase_transition<NextPhaseLevelSelectTag>(reg);  CHECK(count_next_phase_tags(reg) == 1);
    request_phase_transition<NextPhasePlayingTag>(reg);      CHECK(count_next_phase_tags(reg) == 1);
    request_phase_transition<NextPhasePausedTag>(reg);       CHECK(count_next_phase_tags(reg) == 1);
    request_phase_transition<NextPhaseGameOverTag>(reg);     CHECK(count_next_phase_tags(reg) == 1);
    request_phase_transition<NextPhaseSongCompleteTag>(reg); CHECK(count_next_phase_tags(reg) == 1);
    request_phase_transition<NextPhaseSettingsTag>(reg);     CHECK(count_next_phase_tags(reg) == 1);
    request_phase_transition<NextPhaseTutorialTag>(reg);     CHECK(count_next_phase_tags(reg) == 1);
}

TEST_CASE("phase_transition: is_phase_transition_pending reflects NextPhase*Tag presence",
          "[game_phase][tags][ecs_refactor][issue_1202]") {
    entt::registry reg;

    CHECK_FALSE(is_phase_transition_pending(reg));

    request_phase_transition<NextPhasePlayingTag>(reg);
    CHECK(is_phase_transition_pending(reg));

    clear_next_phase_tags(reg);
    CHECK_FALSE(is_phase_transition_pending(reg));

    request_phase_transition<NextPhaseGameOverTag>(reg);
    CHECK(is_phase_transition_pending(reg));
}
