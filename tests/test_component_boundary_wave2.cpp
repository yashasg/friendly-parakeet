// Wave-2 component boundary regression guards.
//
// Pins contracts for types that have been or are being relocated out of
// app/components/ (wave-2 cleanup: high_score.h, text.h).
//
// Includes are routed through stable aggregation headers so this file needs no
// changes when individual moves complete:
//
//   test_helpers.h
//     └─ systems/high_score_system.h → HighScoreState
//
// Static assertions catch silent breakage if constants are changed during or
// after the header reorganisation.

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

#include "test_helpers.h"        // HighScoreState (via high_score_system.h)
#include "systems/high_score_system.h"

// ── HighScoreState table constants ───────────────────────────────────────────
//
// MAX_ENTRIES == LEVEL_COUNT(3) × DIFFICULTY_COUNT(3) == 9.
// `set_score` / `ensure_entry` / `load_high_scores` reject inserts past this
// cap, so any change here must be paired with a persistence-format review.

static_assert(HighScoreState::MAX_ENTRIES == 9,
    "HighScoreState::MAX_ENTRIES must equal LEVEL_COUNT(3) x DIFFICULTY_COUNT(3)=9; "
    "update persistence format if this changes");

// KEY_CAP must accommodate the longest shipped key plus null terminator.
// Longest shipped key: '3_mental_corruption|hard\0' == 26 chars.

static_assert(HighScoreState::KEY_CAP == 32,
    "KEY_CAP must be >= 32 to fit all shipped 'song_id|difficulty' keys; "
    "longest is '3_mental_corruption|hard\\0' (26 chars)");

// HighScoreState is now constants-only (Fabian Principle 3 / issue #1560):
// the prior `std::array<Entry, MAX_ENTRIES> entries + int32_t entry_count`
// was a god-class array column and has been normalized into a
// `HighScoreEntry` row table walked via `view<HighScoreEntry>()`.

static_assert(std::is_empty_v<HighScoreState>,
    "HighScoreState must hold no instance state — entries live as HighScoreEntry rows "
    "in the registry (issue #1560 / Fabian Principle 3)");

// ── Runtime: HighScoreEntry row table ────────────────────────────────────────

TEST_CASE("HighScoreEntry: default-constructed row is empty", "[boundary_wave2][high_score]") {
    HighScoreEntry e{};
    CHECK(e.key[0] == '\0');
    CHECK(e.key_hash == 0u);
    CHECK(e.score == 0);
}

TEST_CASE("HighScoreSession: default-constructed has zero key hash", "[boundary_wave2][high_score]") {
    HighScoreSession session{};
    CHECK(session.key_hash == 0u);
}

TEST_CASE("HighScoreEntry: rows live as registry entities, not in ctx", "[boundary_wave2][high_score]") {
    auto reg = make_registry();
    // Fresh registry starts with zero entry rows.
    CHECK(high_score::entry_count(reg) == 0);
    CHECK(reg.view<HighScoreEntry>().empty());
    // HighScoreState is no longer a ctx singleton (constants-only struct).
    CHECK_FALSE(reg.ctx().contains<HighScoreState>());
}
