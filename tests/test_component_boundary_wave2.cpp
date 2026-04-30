// Wave-2 component boundary regression guards.
//
// Pins contracts for types that have been or are being relocated out of
// app/components/ (wave-2 cleanup: window_phase.h, high_score.h,
// text.h).
//
// Includes are routed through stable aggregation headers so this file needs no
// changes when individual moves complete:
//
//   test_helpers.h
//     └─ components/player.h → components/window_phase.h (WindowPhase)
//     └─ util/high_score_persistence.h → HighScoreState
//
// Static assertions catch silent breakage if constants or ordinals are changed
// during or after the header reorganisation.

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

#include "test_helpers.h"        // WindowPhase (via player.h), HighScoreState (via high_score_persistence.h)

// ── WindowPhase ordinal contract ─────────────────────────────────────────────
//
// rhythm_system and shape_window_system rely on the ordering of these values:
//   Idle → MorphIn → Active → MorphOut (→ Idle)
// Any reordering would silently break phase-progression logic in those systems
// because they compare and advance the enum numerically.

static_assert(static_cast<uint8_t>(WindowPhase::Idle)     == 0u,
    "WindowPhase::Idle must be 0; shape_window_system uses zero-init as the reset sentinel");

static_assert(static_cast<uint8_t>(WindowPhase::MorphIn)  == 1u,
    "WindowPhase ordinals must be sequential (Idle=0, MorphIn=1, Active=2, MorphOut=3)");

static_assert(static_cast<uint8_t>(WindowPhase::Active)   == 2u,
    "WindowPhase ordinals must be sequential (Idle=0, MorphIn=1, Active=2, MorphOut=3)");

static_assert(static_cast<uint8_t>(WindowPhase::MorphOut) == 3u,
    "WindowPhase ordinals must be sequential (Idle=0, MorphIn=1, Active=2, MorphOut=3)");

// ── HighScoreState table constants ───────────────────────────────────────────
//
// MAX_ENTRIES == LEVEL_COUNT(3) × DIFFICULTY_COUNT(3) == 9.
// Persistence serialises and deserialises up to this many entries by index; if
// it changes without resizing the array the table silently drops or corrupts
// entries.

static_assert(HighScoreState::MAX_ENTRIES == 9,
    "HighScoreState::MAX_ENTRIES must equal LEVEL_COUNT(3) x DIFFICULTY_COUNT(3)=9; "
    "update persistence format if this changes");

// KEY_CAP must accommodate the longest shipped key plus null terminator.
// Longest shipped key: '3_mental_corruption|hard\0' == 26 chars.

static_assert(HighScoreState::KEY_CAP == 32,
    "KEY_CAP must be >= 32 to fit all shipped 'song_id|difficulty' keys; "
    "longest is '3_mental_corruption|hard\\0' (26 chars)");

// ── Runtime: HighScoreState default state ────────────────────────────────────

TEST_CASE("HighScoreState: default-constructed is empty", "[boundary_wave2][high_score]") {
    HighScoreState hs{};
    CHECK(hs.entry_count == 0);
    CHECK(hs.current_key_hash == 0u);
    // All entry scores must be zero (no stale data)
    for (int i = 0; i < HighScoreState::MAX_ENTRIES; ++i) {
        CHECK(hs.entries[i].score == 0);
    }
}

TEST_CASE("HighScoreState: lives in ctx, not attached to entities", "[boundary_wave2][high_score]") {
    auto reg = make_registry();
    // ctx singleton must be present and accessible
    auto& hs = reg.ctx().get<HighScoreState>();
    CHECK(hs.entry_count == 0);
    // The type must never appear as an entity component — view must be empty
    CHECK(reg.view<HighScoreState>().empty());
}
