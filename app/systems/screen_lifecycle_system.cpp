#include "screen_lifecycle_system.h"

#include "../components/game_state.h"
#include "../tags/tags.h"
#include "../ui/generated/screen_spawners.h"

// Per Fabian existential processing (#1202/#1204): the mapping
// "active phase tag → which screen entity set must exist" is itself a
// table, indexed by row (one row per migrated screen). Each row references
// the matching phase tag, screen entity tag, and the spawn/despawn pair
// emitted by codegen. Convergence runs once per frame and short-circuits
// when the entity-set membership already matches the phase tag presence.
//
// Adding a new screen to the migration: append one row below. No `switch`,
// no `if`-ladder over phase, no virtual dispatch.

namespace {

using SpawnFn   = void (*)(entt::registry&);
using DespawnFn = void (*)(entt::registry&);
using PhaseProbeFn  = bool (*)(const entt::registry&);
using EntityProbeFn = bool (*)(entt::registry&);

struct ScreenLifecycleRow {
    PhaseProbeFn  phase_active;   // is the matching GamePhase*Tag present?
    EntityProbeFn entities_alive; // is the matching screen-tag view non-empty?
    SpawnFn       spawn;
    DespawnFn     despawn;
};

template <typename PhaseTag>
bool phase_active_probe(const entt::registry& reg) noexcept {
    return reg.ctx().contains<PhaseTag>();
}

template <typename ScreenTag>
bool entities_alive_probe(entt::registry& reg) noexcept {
    return reg.view<ScreenTag>().size() > 0;
}

// Pilot (#1287): Paused. Tutorial (#1291), Song Complete (#1292), Game
// Over (#1293), Title (#1294). Each subsequent per-screen migration
// sub-issue extends this table by one row.
constexpr ScreenLifecycleRow kLifecycleRows[] = {
    {
        &phase_active_probe<GamePhasePausedTag>,
        &entities_alive_probe<PausedScreenTag>,
        &spawn_paused_screen,
        &despawn_paused_screen,
    },
    {
        &phase_active_probe<GamePhaseTutorialTag>,
        &entities_alive_probe<TutorialScreenTag>,
        &spawn_tutorial_screen,
        &despawn_tutorial_screen,
    },
    {
        &phase_active_probe<GamePhaseSongCompleteTag>,
        &entities_alive_probe<SongCompleteScreenTag>,
        &spawn_song_complete_screen,
        &despawn_song_complete_screen,
    },
    {
        &phase_active_probe<GamePhaseGameOverTag>,
        &entities_alive_probe<GameOverScreenTag>,
        &spawn_game_over_screen,
        &despawn_game_over_screen,
    },
    {
        &phase_active_probe<GamePhaseTitleTag>,
        &entities_alive_probe<TitleScreenTag>,
        &spawn_title_screen,
        &despawn_title_screen,
    },
};

}  // namespace

void screen_lifecycle_system(entt::registry& reg) {
    for (const auto& row : kLifecycleRows) {
        const bool want = row.phase_active(reg);
        const bool have = row.entities_alive(reg);
        if (want && !have) {
            row.spawn(reg);
        } else if (!want && have) {
            row.despawn(reg);
        }
    }
}
