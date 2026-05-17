#pragma once

#include <cstdint>

// `Count` is a sentinel (always last); `static_cast<int>(SFX::Count)` replaces
// the former `magic_enum::enum_count<SFX>()` lookup (issue #1204 bonus).
enum class SFX : uint8_t {
    ShapeShift = 0,
    Crash,
    UITap,
    ChainBonus,
    ScorePopup,
    GameStart,
    Count,
};

// Compile-time guard: SFX values must be contiguous and zero-based so that
// static arrays indexed by static_cast<int>(sfx) are always in-bounds.
static_assert(static_cast<int>(SFX::Count) == 6,
              "SFX enum changed — update systems/sfx_bank.cpp SFX_SPECS and this guard");
static_assert(static_cast<int>(SFX::ShapeShift) == 0,
              "SFX enum must be zero-based for array indexing");

// The resident sound bank that owns raylib Sound handles for each SFX lives
// in `app/systems/sfx_bank_resources.h` (issue #1358). `app/components/audio.h`
// stays plain data — see .squad/decisions.md §"app/components parallel audit
// verdict (consolidated)".
