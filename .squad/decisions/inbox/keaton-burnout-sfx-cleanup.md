# Decision: Remove SFX::BurnoutBank — Use SFX::ScorePopup for Obstacle-Clear Audio

**Owner:** Keaton  
**Date:** 2026-05-17  
**Commit:** 9ab0a1c  
**Blocker:** Kujan rejection — BurnoutBank fires on every obstacle clear in a non-burnout scoring path

## Decision

Remove `SFX::BurnoutBank` from the `SFX` enum entirely. The slot was originally added for burnout-multiplier scoring feedback. After `#239` removed burnout altogether, the name became a semantic contradiction when fired in the standard scoring path.

`SFX::ScorePopup` (already present, 988 Hz Sine, 0.060 s) is the correct slot for "obstacle successfully cleared" audio feedback.

## Changes

| File | Change |
|------|--------|
| `app/components/audio.h` | Removed `BurnoutBank` from `SFX` enum; updated `enum_count` guard 10 → 9 |
| `app/systems/sfx_bank.cpp` | Removed BurnoutBank `SfxSpec` entry; array stays contiguous |
| `app/systems/scoring_system.cpp` | `audio_push(..., SFX::BurnoutBank)` → `audio_push(..., SFX::ScorePopup)` |
| `tests/test_audio_system.cpp` | Removed `SFX::BurnoutBank` from `kAllSfx[]`; count stays consistent |

## Rationale

- Removing > renaming: a renamed slot would still require updating all call sites and documentation. Since no production path needs a dedicated "burnout bank confirmation" sound post-#239, removal is cleaner.
- `SFX::ScorePopup` already existed and is the appropriate semantic slot.
- Enum contiguity and `static_assert` guards maintained.
- Build: zero compiler warnings. Tests: all `[audio]` and `[scoring]` cases pass.
