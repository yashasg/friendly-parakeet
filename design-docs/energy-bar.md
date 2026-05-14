# SHAPESHIFTER — Energy Bar System
## Design Specification · v1.0

---

## Concept

The Energy Bar replaces instant-death-on-miss with a **continuous survival
resource** tied to rhythm accuracy.  Hit beats well → energy recovers.
Miss beats or time them badly → energy drains.  Energy hits zero → game over.

This creates a **forgiveness window** for new players while rewarding
accuracy for advanced players.  It also turns every single note into
a meaningful choice: "can I afford a sloppy hit here, or do I need
to nail this one?"

```
  ENERGY BAR — PLAYER LIFECYCLE

  ████████████████████  1.0  FULL — song start
  ██████████████████░░  0.9  Perfect hit (+recovery)
  ████████████████░░░░  0.8  Good hit (+recovery)
  ██████████░░░░░░░░░░  0.5  Ok hit (+small recovery)
  ████░░░░░░░░░░░░░░░░  0.2  MISS! (big drain)
  ██░░░░░░░░░░░░░░░░░░  0.1  CRITICAL — bar flashes red
  ░░░░░░░░░░░░░░░░░░░░  0.0  GAME OVER
```

---

## Data Design

### EnergyState (registry singleton)

```cpp
struct EnergyState {
    float energy      = constants::ENERGY_START;   // [0.0, 1.0] — current energy
    float display     = constants::ENERGY_START;   // smoothed for rendering (lerps toward energy)
    float flash_timer = 0.0f;   // > 0 when bar should flash (drain event)
};
```

**Why float, not int?**  Continuous bar feels juicier, allows fractional
drain/recovery tuning, and maps directly to a 0–1 bar width.

### Constants (in `constants.h`, mirrored in `content/constants.json`)

```cpp
// ── Energy Bar ────────────────────────────────────
constexpr float ENERGY_MAX              = 1.0f;
constexpr float ENERGY_START            = 1.0f;

// Drain amounts (subtracted from energy)
constexpr float ENERGY_DRAIN_MISS       = 0.20f;   // missed obstacle
constexpr float ENERGY_DRAIN_BAD        = 0.05f;   // Bad timing tier (see #395/#408 — drained from 0.10f to 0.05f to soften Bad-tier punishment)

// Recovery amounts (added to energy)
constexpr float ENERGY_RECOVER_OK       = 0.02f;   // Ok timing
constexpr float ENERGY_RECOVER_GOOD     = 0.05f;   // Good timing
constexpr float ENERGY_RECOVER_PERFECT  = 0.10f;   // Perfect timing

// Visual
constexpr float ENERGY_DISPLAY_SPEED    = 3.0f;    // lerp speed for smooth bar
constexpr float ENERGY_FLASH_DURATION   = 0.3f;    // seconds bar flashes on drain
constexpr float ENERGY_CRITICAL_THRESH  = 0.25f;   // below this → bar pulses red
```

---

## Energy Flow

```
  ┌──────────────────────────────────────────────────────┐
  │                   COLLISION SYSTEM                    │
  │                                                      │
  │  obstacle reaches player                             │
  │       │                                              │
  │       ├── shape matches? ──► TimingGrade emplaced     │
  │       │                      │                       │
  │       │                      ├─ Perfect ──► +0.10    │
  │       │                      ├─ Good    ──► +0.05    │
  │       │                      ├─ Ok      ──► +0.02    │
  │       │                      └─ Bad     ──► -0.05    │
  │       │                                              │
  │       └── MISS ──────────────────────────► -0.20     │
  │                                                      │
  └──────────────┬───────────────────────────────────────┘
                 │
                 ▼
  ┌──────────────────────────────────────────────────────┐
  │                   ENERGY SYSTEM                       │
  │                                                      │
  │  Runs after scoring_system, applies queued effects   │
  │                                                      │
  │  Smooth display toward energy (lerp)                 │
  │  Tick flash_timer countdown                          │
  │                                                      │
  └──────────────────────────────────────────────────────┘
                 │
                 ▼
  ┌──────────────────────────────────────────────────────┐
  │                   RENDER SYSTEM                       │
  │                                                      │
  │  Draw energy bar at HUD position                     │
  │  Color = lerp(RED, GREEN, display)                   │
  │  If flash_timer > 0 → pulse alpha                    │
  │  If energy < CRITICAL → slow pulse red               │
  │                                                      │
  └──────────────────────────────────────────────────────┘
```

---

## Energy Modification Points

Energy deltas are enqueued by gameplay systems and applied by
`energy_system` through `PendingEnergyEffects`.  Gameplay systems should not
mutate `EnergyState::energy` directly.

### 1. collision_system.cpp — on MISS

Current shipped behavior:

```
MISS: enqueue_energy_effect(reg, -ENERGY_DRAIN_MISS, true)
```

The miss no longer kills the player directly. `energy_system` requests the
`GamePhase::GameOver` transition in the same fixed tick that drains energy
to ≤ 0 (provided a `SongState` exists), setting
`GameOverState::cause = DeathCause::EnergyDepleted`. `game_state_system`
retains a fallback check that catches pre-existing `energy <= 0` while phase
is `Playing` (defense-in-depth, not the primary path).

Miss still increments `results->miss_count` and still emplaces
`ScoredTag` (obstacle is consumed, not re-triggered).

### 2. scoring_system.cpp — on scored obstacle with TimingGrade

After computing points, enqueue an energy delta based on timing tier:

```
switch (timing->tier) {
    case TimingTier::Perfect: enqueue_energy_effect(reg, ENERGY_RECOVER_PERFECT); break;
    case TimingTier::Good:    enqueue_energy_effect(reg, ENERGY_RECOVER_GOOD);    break;
    case TimingTier::Ok:      enqueue_energy_effect(reg, ENERGY_RECOVER_OK);      break;
    case TimingTier::Bad:     enqueue_energy_effect(reg, -ENERGY_DRAIN_BAD, true);
                              break;
}
```


---

## System Execution Order

```
  game_state_system        ← catches pre-existing energy <= 0 → GameOver
  song_playback_system
  tick_playing_systems
    collision_system       ← tags MISS / ScoredTag
    scoring_system         ← enqueues energy effects
  obstacle_despawn_system
  popup_feedback_system
  popup_display_system
  energy_system            ← applies queued effects, requests GameOver on depletion, smooths display
  energy_bar_system        ← drives the on-HUD energy-bar visual
  particle_system
```

`collision_system` tags MISS/HIT outcomes and `scoring_system` enqueues
timing-grade energy drain/recovery inside `tick_playing_systems()`. The
`energy_system` then runs after popup updates in `tick_fixed_systems()`.
Energy depletion always wins over song completion while the phase is Playing,
including when playback has already marked the song finished in the same fixed
tick. The popup/energy placement is a cache-locality choice, not a semantic
dependency.

---

## HUD Visual

```
  ┌─────────────────────────────────────────────┐
  │  SCORE: 12450        ★ HI: 18200            │
  │                                              │
  │  ┌─ ENERGY BAR                              │
  │  │ █                                        │
  │  │ █     (gameplay area)                    │
  │  │ █                                        │
  │  │ ▓                                        │
  │  │ ░                                        │
  │  └─░                                        │
  │                                              │
  │   [ ● ]        [ ■ ]        [ ▲ ]           │
  └─────────────────────────────────────────────┘

  Energy bar color ramp (bottom = empty, top = full):
  ░  0%  — RED
  ▒  25% — ORANGE (critical)
  ▓  50% — YELLOW
  ▓  75% — YELLOW-GREEN
  █  100% — GREEN
```

The shipped energy bar is a vertical segmented meter on the left edge
of the play area. The burnout bar has been removed; the energy bar is
now the only survival meter.

### Flash Effect (on drain)

```
  Normal:   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░
  Flash:    ░░░░░░░░░░░░░░░░░░░░░░░░  ← bar blinks white for 0.3s
            ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░
```

### Critical Pulse (energy < 25%)

```
  Frame 0:  ▓▓▓▓░░░░░░░░░░░░░░░░░░░░  (normal red)
  Frame 5:  ████░░░░░░░░░░░░░░░░░░░░  (bright red pulse)
  Frame 10: ▓▓▓▓░░░░░░░░░░░░░░░░░░░░  (back to normal)
```

---

## Song Results Integration

`SongResults` already tracks `perfect_count`, `good_count`, `ok_count`,
`bad_count`, `miss_count`.  No changes needed.  The Song Complete
screen can show an "Energy" stat:

```
  ┌────────────────────────────────┐
  │        SONG COMPLETE!          │
  │                                │
  │  Score:     12,450             │
  │  High:      18,200             │
  │                                │
  │  Perfect:   42                 │
  │  Good:      28                 │
  │  Ok:        15                 │
  │  Bad:        3                 │
  │  Miss:       2                 │
  │                                │
  │  Energy:    ▓▓▓▓▓▓▓░░░  68%   │  ← final energy remaining
  │  Chain:     12                 │
  │                                │
  │  [ RETRY ]  [ LEVELS ]  [ ← ] │
  └────────────────────────────────┘
```

---

## Tuning Notes

The drain/recovery values are designed so that:

```
  Scenario A — "Sloppy but surviving"
  ─────────────────────────────────────
  10 notes:  2 Perfect, 3 Good, 3 Ok, 1 Bad, 1 Miss
  Energy Δ = (2×0.10) + (3×0.05) + (3×0.02) + (1×-0.05) + (1×-0.20)
           = 0.20 + 0.15 + 0.06 - 0.05 - 0.20
           = +0.16  (net positive — player recovers slowly)

  Scenario B — "Struggling"
  ─────────────────────────────────────
  10 notes:  1 Perfect, 2 Good, 2 Ok, 3 Bad, 2 Miss
  Energy Δ = (1×0.10) + (2×0.05) + (2×0.02) + (3×-0.05) + (2×-0.20)
           = 0.10 + 0.10 + 0.04 - 0.15 - 0.40
           = -0.31  (draining — Bad tier softened from -0.46 by #395/#408)

  Scenario C — "Pro player"
  ─────────────────────────────────────
  10 notes:  7 Perfect, 2 Good, 1 Ok, 0 Bad, 0 Miss
  Energy Δ = (7×0.10) + (2×0.05) + (1×0.02)
           = 0.70 + 0.10 + 0.02
           = +0.82  (bar stays pinned at max)
```

These values are mirrored in `content/constants.json` so tools and docs can
consume the same tuning table. Runtime C++ currently uses the matching
`constants.h` constexpr values for zero-overhead access.

---

## Non-Goals (out of scope)

- **Difficulty-based energy tuning** — all difficulties use the same
  drain/recovery values for now.  Can be added later via `DifficultyConfig`.
- **Energy powerups / pickups** — not in this feature.
- **Energy affects scoring** — energy is purely survival.  Scoring
  is owned by the rhythm timing × chain pipeline (see `rhythm-design.md`).
- **Passive energy drain over time** — energy only changes on note events.
