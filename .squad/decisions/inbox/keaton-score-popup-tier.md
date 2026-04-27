# Decision: ScorePopup::timing_tier is now std::optional<TimingTier>

**Date:** 2025  
**Author:** Keaton  
**Issue:** #288

## Decision

`ScorePopup::timing_tier` was changed from `uint8_t` (with magic sentinel `255`) to `std::optional<TimingTier>`.

## Rationale

The sentinel value `255` duplicated enum knowledge outside the type system. Any code reading `timing_tier` had to know that 255 means "no grade" — and `popup_display_system` then had to map raw integers 3/2/1/0 back to enum meanings. Using `std::optional<TimingTier>` makes "no grade" explicit at the type level and lets the switch use named enum cases.

## Impact on other agents

- Any code constructing a `ScorePopup` must use `std::nullopt` (no grade) or `std::make_optional(tier)` — not a raw uint8_t cast.
- Any code reading `timing_tier` must call `.has_value()` / `*timing_tier` — not compare against 255.
- `scoring.h` now includes `rhythm.h`; downstream includes get `TimingTier` transitively.
