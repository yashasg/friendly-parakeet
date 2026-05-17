#include "song_complete_scoreboard_bind_system.h"

#include "../components/energy_bar.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/song_state.h"
#include "../components/ui.h"
#include "../tags/tags.h"

#include <array>
#include <cstdio>

namespace {

// Bind context — owns the per-frame singleton reads so the per-slot
// `bind_*` functions are pure transforms over their slot's `UiLabel`. The
// optional pointers mirror the legacy controller's `ctx().find<...>()`
// semantics: a SongResults / EnergyState / TerminalResultState may be
// absent during the Song Complete debounce window before
// `game_state_enter_terminal_phase_song_complete` populates them.
//
// Per-binder prefix (`SongComplete*`) avoids an anonymous-namespace ODR
// collision with the sibling `*_bind_system.cpp` files when CMake Unity
// chunking happens to place two binders in the same jumbo TU (issue #1329).
struct SongCompleteBindContext {
    const ScoreState& score;
    const CurrentSongHighScore& current;
    const TerminalResultState* result;
    const SongResults* results;
    const EnergyState* energy;
};

using SongCompleteSlotBindFn = void (*)(const SongCompleteBindContext&, UiLabel&);

void bind_score(const SongCompleteBindContext& ctx, UiLabel& label) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", ctx.score.score);
    ui_label_set(label, buf);
}

void bind_high_score(const SongCompleteBindContext& ctx, UiLabel& label) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", ctx.current.value);
    ui_label_set(label, buf);
}

void bind_new_best(const SongCompleteBindContext& ctx, UiLabel& label) {
    if (ctx.result == nullptr || !ctx.result->new_best) {
        ui_label_set(label, "");
        return;
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "NEW BEST! PREV %d", ctx.result->previous_best);
    ui_label_set(label, buf);
}

void bind_perfect_good(const SongCompleteBindContext& ctx, UiLabel& label) {
    const int p = ctx.results ? ctx.results->perfect_count : 0;
    const int g = ctx.results ? ctx.results->good_count : 0;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "PERFECT %d     GOOD %d", p, g);
    ui_label_set(label, buf);
}

void bind_ok_bad_miss(const SongCompleteBindContext& ctx, UiLabel& label) {
    const int o = ctx.results ? ctx.results->ok_count   : 0;
    const int b = ctx.results ? ctx.results->bad_count  : 0;
    const int m = ctx.results ? ctx.results->miss_count : 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "OK %d     BAD %d     MISS %d", o, b, m);
    ui_label_set(label, buf);
}

void bind_max_chain(const SongCompleteBindContext& ctx, UiLabel& label) {
    const int c = ctx.results ? ctx.results->max_chain : 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "MAX CHAIN %d", c);
    ui_label_set(label, buf);
}

void bind_energy(const SongCompleteBindContext& ctx, UiLabel& label) {
    if (ctx.energy == nullptr) {
        ui_label_set(label, "");
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf),
                  "ENERGY %.0f%%",
                  static_cast<double>(ctx.energy->energy * 100.0f));
    ui_label_set(label, buf);
}

// Per-slot bind table. Position-keyed identification matches the
// `content/ui/screens/song_complete.rgl` rows; codegen bakes those exact
// (x, y) into the spawn block, so a layout edit that moves a slot will
// silently miss its bind (caught by a visible "0" / empty placeholder in
// playtest before any production build).
struct SongCompleteSlotRow {
    float x;
    float y;
    SongCompleteSlotBindFn fn;
};

constexpr std::array<SongCompleteSlotRow, 7> kSongCompleteSlots = {{
    {160.0f, 463.0f, &bind_score},
    {160.0f, 573.0f, &bind_high_score},
    {120.0f, 620.0f, &bind_new_best},
    {120.0f, 650.0f, &bind_perfect_good},
    {120.0f, 684.0f, &bind_ok_bad_miss},
    {120.0f, 718.0f, &bind_max_chain},
    {120.0f, 752.0f, &bind_energy},
}};

}  // namespace

void song_complete_scoreboard_bind_system(entt::registry& reg) {
    auto view = reg.view<SongCompleteScreenTag, UiLabelTag, UiPosition, UiLabel>();
    if (view.begin() == view.end()) return;

    const SongCompleteBindContext ctx{
        reg.ctx().get<ScoreState>(),
        reg.ctx().get<CurrentSongHighScore>(),
        reg.ctx().find<TerminalResultState>(),
        reg.ctx().find<SongResults>(),
        reg.ctx().find<EnergyState>(),
    };

    for (auto [e, pos, label] : view.each()) {
        (void)e;
        for (const auto& row : kSongCompleteSlots) {
            if (pos.x == row.x && pos.y == row.y) {
                row.fn(ctx, label);
                break;
            }
        }
    }
}
