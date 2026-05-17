#include "game_over_scoreboard_bind_system.h"

#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/ui.h"
#include "../tags/tags.h"

#include <array>
#include <cstdio>

namespace {

// Bind context — owns the per-frame singleton reads so the per-slot
// `bind_*` functions are pure transforms over their slot's `UiLabel`. The
// optional pointer mirrors the legacy controller's `ctx().find<...>()`
// semantics: a `TerminalResultState` may be absent during the Game Over
// debounce window before `game_state_enter_terminal_phase_game_over`
// populates it. `EnergyDepletedDeath` is a presence-only ctx tag (no
// payload) so we carry a bool for it.
//
// Per-binder prefix (`GameOver*`) avoids an anonymous-namespace ODR
// collision with the sibling `*_bind_system.cpp` files when CMake Unity
// chunking happens to place two binders in the same jumbo TU (issue #1329).
struct GameOverBindContext {
    const ScoreState& score;
    const CurrentSongHighScore& current;
    const TerminalResultState* result;
    bool energy_depleted;
};

using GameOverSlotBindFn = void (*)(const GameOverBindContext&, UiLabel&);

bool is_new_best(const GameOverBindContext& ctx) {
    return ctx.result != nullptr && ctx.result->new_best;
}

void bind_score(const GameOverBindContext& ctx, UiLabel& label) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", ctx.score.score);
    ui_label_set(label, buf);
}

void bind_high_score(const GameOverBindContext& ctx, UiLabel& label) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", ctx.current.value);
    ui_label_set(label, buf);
}

void bind_new_best(const GameOverBindContext& ctx, UiLabel& label) {
    ui_label_set(label, is_new_best(ctx) ? "NEW BEST!" : "");
}

void bind_prev_best(const GameOverBindContext& ctx, UiLabel& label) {
    if (!is_new_best(ctx)) {
        ui_label_set(label, "");
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "PREV %d", ctx.result->previous_best);
    ui_label_set(label, buf);
}

// Reason slot at y=685 — shown only when NOT new-best (legacy
// `reason_y = 685.0f` branch in `draw_game_over_scoreboard`).
void bind_reason(const GameOverBindContext& ctx, UiLabel& label) {
    if (!ctx.energy_depleted || is_new_best(ctx)) {
        ui_label_set(label, "");
        return;
    }
    ui_label_set(label, "ENERGY DEPLETED");
}

// Reason slot at y=742 — shown only when new-best (legacy
// `reason_y = 742.0f` branch in `draw_game_over_scoreboard`; the reason
// moves down to clear room for "NEW BEST!" + "PREV N").
void bind_reason_new_best(const GameOverBindContext& ctx, UiLabel& label) {
    if (!ctx.energy_depleted || !is_new_best(ctx)) {
        ui_label_set(label, "");
        return;
    }
    ui_label_set(label, "ENERGY DEPLETED");
}

// Per-slot bind table. Position-keyed identification matches the
// `content/ui/screens/game_over.rgl` rows; codegen bakes those exact (x, y)
// into the spawn block, so a layout edit that moves a slot will silently
// miss its bind (caught by a visible empty placeholder in playtest before
// any production build).
struct GameOverSlotRow {
    float x;
    float y;
    GameOverSlotBindFn fn;
};

constexpr std::array<GameOverSlotRow, 6> kGameOverSlots = {{
    {210.0f, 540.0f, &bind_score},
    {210.0f, 634.0f, &bind_high_score},
    {110.0f, 665.0f, &bind_new_best},
    {110.0f, 685.0f, &bind_reason},
    {110.0f, 712.0f, &bind_prev_best},
    {110.0f, 742.0f, &bind_reason_new_best},
}};

}  // namespace

void game_over_scoreboard_bind_system(entt::registry& reg) {
    auto view = reg.view<GameOverScreenTag, UiLabelTag, UiPosition, UiLabel>();
    if (view.begin() == view.end()) return;

    const GameOverBindContext ctx{
        reg.ctx().get<ScoreState>(),
        reg.ctx().get<CurrentSongHighScore>(),
        reg.ctx().find<TerminalResultState>(),
        reg.ctx().find<EnergyDepletedDeath>() != nullptr,
    };

    for (auto [e, pos, label] : view.each()) {
        (void)e;
        for (const auto& row : kGameOverSlots) {
            if (pos.x == row.x && pos.y == row.y) {
                row.fn(ctx, label);
                break;
            }
        }
    }
}
