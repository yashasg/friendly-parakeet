#include "all_systems.h"
#include "../components/gameplay_intents.h"
#include "../components/system_scratch.h"

void runtime_system_scratch_init(entt::registry& reg) {
    reg.ctx().erase<ScoringSystemScratch>();
    reg.ctx().erase<PendingEnergyEffects>();
    reg.ctx().erase<ScorePopupRequestQueue>();
    reg.ctx().erase<ObstacleDespawnScratch>();
    reg.ctx().erase<PopupDisplayScratch>();
    reg.ctx().erase<ParticleSystemScratch>();

    reg.ctx().emplace<ScoringSystemScratch>();
    reg.ctx().emplace<PendingEnergyEffects>();
    reg.ctx().emplace<ScorePopupRequestQueue>();
    reg.ctx().emplace<ObstacleDespawnScratch>();
    reg.ctx().emplace<PopupDisplayScratch>();
    reg.ctx().emplace<ParticleSystemScratch>();
}
