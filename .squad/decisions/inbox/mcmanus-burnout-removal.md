# Decision: Burnout Multiplier Removed (#239)

**Author:** McManus  
**Date:** 2026-04-27  
**Status:** Implemented

## Decision

Burnout penalties have been removed. Scoring now uses a flat 1.0× base multiplier for all obstacle clears, regardless of proximity at press time.

## What Changed

- `burnout_system` is a no-op (removed from frame update path).
- `player_input_system` no longer banks `BankedBurnout` on obstacles.
- `scoring_system` ignores `BankedBurnout` and always applies `burnout_mult = 1.0f`.
- `SongResults::best_burnout` always stays 0.0.
- `ScorePopup::tier` (burnout tier) always emits 0.

## What Was NOT Removed

- `BurnoutState`, `BankedBurnout`, `BurnoutZone` structs in `burnout.h` — still compiled; safe to remove in a later pass once all reference sites are cleared.
- `burnout_helpers.h` — no longer included by any gameplay code; can be deleted in a later cleanup.
- `burnout_system.cpp` function body — no-op stub kept for link compatibility.
- `ScorePopup::tier` field — kept as zero; can be removed in UI/scoring cleanup.

## Rationale

Per issue #239: the multiplier system taught players to delay shape changes until obstacles were imminent, directly conflicting with the beat-mastery design pillar. On-beat shape changes with no nearby obstacle were functionally penalized by missing the ×1.5–×5.0 bonus window.

## Downstream Notes

- UI systems rendering `BurnoutZone` meter or `ScorePopup::tier` will silently display zero/None — no crash risk.
- The SFX enum values (`SFX::ZoneRisky`, `SFX::ZoneDanger`, etc.) are untouched.
- Keaton's magic_enum audio refactor is unaffected: no audio.h or sfx_bank.cpp changes.
