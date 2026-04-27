# TestFlight DoD Execution Queue — After #288

**Date:** 2026-05-17  
**Author:** Keyser  
**Status:** PROPOSED — for coordinator routing

## Decision

The TestFlight queue of 20 open issues has been prioritized. The ordering rule is:

1. **DoD purity first:** system-responsibility violations before component-purity violations before platform/lifecycle issues.
2. **Dependency safety:** issues that touch the same file as active work (#288 is in `rendering.h`) are deferred to the next batch.
3. **Small before large:** single-file fixes before multi-system refactors; prove the ownership model before splitting god systems.
4. **Energy ownership must settle before dependent splits:** #278 (scoring→EnergyState) and #276 (energy→GameOver) must merge before #282 (collision→EnergyState) and #280 (cleanup→miss grading) are safe to execute.

## Batch Ordering

### Q1 — Start immediately (safe, no conflict with active #288)
| # | Owner | Risk |
|---|-------|------|
| #283 song_playback_system inline logging | baer | Low |
| #285 TestPlayerState fat singleton | keaton | Low |
| #289 beat_map_loader hidden singleton | hockney | Low |

### Q2 — After #288 merges
| # | Owner | Risk | Note |
|---|-------|------|------|
| #278 + #279 scoring_system energy+popup | keaton | Medium | Do together (same file) |
| #276 energy_system GameOver transition | mcmanus | Medium | Upstream of #278 semantically |
| #223 window_scale_for_tier inverted (bug) | mcmanus | Medium | Separate file (rhythm.h) |

### Q3 — After Q2 energy ownership settled
| # | Owner | Risk | Note |
|---|-------|------|------|
| #282 collision_system energy/GameOver | keaton | Medium | Depends on #278 first |
| #280 cleanup_system miss grading | keaton | Medium | Needs Saul sign-off on energy curve |
| #294 ObstacleChildren overflow | keaton | Low-Med | rendering.h, after #288 merged |

### Q4 — Large splits (plan carefully, pair with reviewer)
| # | Owner | Risk | Note |
|---|-------|------|------|
| #296 mesh child signal wiring | keaton | Medium | After #294 |
| #295 unchecked mesh indices | mcmanus | Medium | After #296 |
| #277 game_state_system god class | mcmanus | High | Pair with Kujan review |
| #281 setup_play_session omnibus | mcmanus | High | Pair with Kujan review |

### Q5 — Platform / persistence
| # | Owner | Risk |
|---|-------|------|
| #287 UIState::load_screen I/O | redfoot | Low |
| #297 persistence iOS path | hockney | Low |
| #298 persistence failures ignored | hockney | Low |
| #304 WASM lifecycle bypasses shutdown | hockney | Medium |

### Deferred / Low priority
| # | Owner | Note |
|---|-------|------|
| #236 haptics_enabled dead | kujan | Feature gap, not blocking |

## Dependency Graph
- #278 must precede #282 (EnergyState single-writer invariant)
- #276 must precede or pair with #278 (GameOver transition ownership)
- #288 must merge before #294 (same file: rendering.h)
- #294 should precede #296 (ObstacleChildren ownership → signal wiring)
- #278+#279 should precede #281 (session setup omnibus needs clean scoring interface)
- #280 requires Saul sign-off before execution (gameplay balance change)
- #277 requires Kujan review (god-system split, high surface area)
