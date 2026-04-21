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

## Replaces

| Before (current)                | After (energy bar)                 |
|---------------------------------|------------------------------------|
| `HPState { int current, max }`  | `EnergyState { float energy }`     |
| Miss → instant GameOver         | Miss → energy drain                |
| No reward for timing accuracy   | Ok/Good/Perfect → energy recovery  |
| `hp_system.cpp` (stub)          | `energy_system.cpp` (full)         |
| `MAX_HP = 5` (unused int)       | `ENERGY_MAX = 1.0f`               |

The existing `HPState` struct, `hp_system.cpp`, and related constants
(`MAX_HP`, `HP_DRAIN_ON_MISS`, `HP_RECOVER_ON_PERFECT`) are removed.

---

## Data Design

### EnergyState (registry singleton, replaces HPState)

```cpp
struct EnergyState {
    float energy      = 1.0f;   // [0.0, 1.0] — current energy
    float display     = 1.0f;   // smoothed for rendering (lerps toward energy)
    float flash_timer = 0.0f;   // > 0 when bar should flash (drain event)
};
```

**Why float, not int?**  Continuous bar feels juicier, allows fractional
drain/recovery tuning, and maps directly to a 0–1 bar width.

### Constants (in `constants.h`)

```cpp
// ── Energy Bar ────────────────────────────────────
constexpr float ENERGY_MAX              = 1.0f;
constexpr float ENERGY_START            = 1.0f;

// Drain amounts (subtracted from energy)
constexpr float ENERGY_DRAIN_MISS       = 0.20f;   // missed obstacle
constexpr float ENERGY_DRAIN_BAD        = 0.10f;   // Bad timing tier

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
  │       │                      └─ Bad     ──► -0.10    │
  │       │                                              │
  │       └── MISS ──────────────────────────► -0.20     │
  │                                                      │
  └──────────────┬───────────────────────────────────────┘
                 │
                 ▼
  ┌──────────────────────────────────────────────────────┐
  │                   ENERGY SYSTEM                       │
  │                                                      │
  │  Runs after scoring_system, reads EnergyState        │
  │                                                      │
  │  if energy <= 0  ──► GamePhase::GameOver              │
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

Energy is modified in **two places** only:

### 1. collision_system.cpp — on MISS

Currently a miss triggers instant GameOver.  Change to:

```
Before:  gs.next_phase = GamePhase::GameOver;
After:   energy.energy -= ENERGY_DRAIN_MISS;
         energy.flash_timer = ENERGY_FLASH_DURATION;
```

The miss no longer kills the player directly.  The `energy_system`
handles the GameOver transition when energy reaches zero.

Miss still increments `results->miss_count` and still emplaces
`ScoredTag` (obstacle is consumed, not re-triggered).

### 2. scoring_system.cpp — on scored obstacle with TimingGrade

After computing points, apply energy delta based on timing tier:

```
switch (timing->tier) {
    case TimingTier::Perfect: energy.energy += ENERGY_RECOVER_PERFECT; break;
    case TimingTier::Good:    energy.energy += ENERGY_RECOVER_GOOD;    break;
    case TimingTier::Ok:      energy.energy += ENERGY_RECOVER_OK;      break;
    case TimingTier::Bad:     energy.energy -= ENERGY_DRAIN_BAD;
                              energy.flash_timer = ENERGY_FLASH_DURATION;
                              break;
}
energy.energy = clamp(energy.energy, 0.0f, ENERGY_MAX);
```

Non-timed obstacles (LanePush, LaneBlock) do NOT affect energy.

---

## System Execution Order

```
  ... (existing systems) ...
  collision_system    ← modifies energy on MISS
  scoring_system      ← modifies energy on timed obstacle clear
  energy_system       ← checks depletion → GameOver, smooths display
  lifetime_system
  ...
```

The `energy_system` slot replaces the current `hp_system` slot
in `tick_fixed_systems()` and `all_systems.h`.

---

## HUD Visual

```
  ┌─────────────────────────────────────────────┐
  │  SCORE: 12450        ★ HI: 18200            │
  │                                              │
  │                                              │
  │         (gameplay area)                      │
  │                                              │
  │                                              │
  │                                              │
  │                                              │
  │  ┌─────────────────────────────────────────┐ │
  │  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░│ │  ← ENERGY BAR
  │  └─────────────────────────────────────────┘ │
  │   [ ● ]        [ ■ ]        [ ▲ ]           │
  └─────────────────────────────────────────────┘

  Energy bar color ramp (left = empty, right = full):
  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  0%  — RED
  ▒▒▒▒▒▒▒▒▒▒▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  25% — ORANGE (critical)
  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░  50% — YELLOW
  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░  75% — YELLOW-GREEN
  ████████████████████████████████████████████████ 100% — GREEN
```

The energy bar reuses the existing burnout bar HUD position
(`BURNOUT_BAR_Y_N`).  If the burnout bar is kept, the energy bar
sits directly above it.

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
  Energy Δ = (2×0.10) + (3×0.05) + (3×0.02) + (1×-0.10) + (1×-0.20)
           = 0.20 + 0.15 + 0.06 - 0.10 - 0.20
           = +0.11  (net positive — player survives but barely)

  Scenario B — "Struggling"
  ─────────────────────────────────────
  10 notes:  1 Perfect, 2 Good, 2 Ok, 3 Bad, 2 Miss
  Energy Δ = (1×0.10) + (2×0.05) + (2×0.02) + (3×-0.10) + (2×-0.20)
           = 0.10 + 0.10 + 0.04 - 0.30 - 0.40
           = -0.46  (draining fast — will die in ~20 notes)

  Scenario C — "Pro player"
  ─────────────────────────────────────
  10 notes:  7 Perfect, 2 Good, 1 Ok, 0 Bad, 0 Miss
  Energy Δ = (7×0.10) + (2×0.05) + (1×0.02)
           = 0.70 + 0.10 + 0.02
           = +0.82  (bar stays pinned at max)
```

These values should be exposed in `content/constants.json` for
runtime tuning without recompilation.

---

## Files Changed (Implementation Guide for DoD-Architect)

| File | Change |
|------|--------|
| `app/components/song_state.h` | Replace `HPState` with `EnergyState` |
| `app/constants.h` | Replace `MAX_HP`/`HP_*` with `ENERGY_*` constants |
| `app/systems/hp_system.cpp` | Rename to `energy_system.cpp`, rewrite logic |
| `app/systems/all_systems.h` | Replace `hp_system` decl with `energy_system` |
| `app/systems/collision_system.cpp` | Miss → drain energy instead of instant GameOver |
| `app/systems/scoring_system.cpp` | Add energy recovery/drain on TimingGrade |
| `app/systems/play_session.cpp` | Init `EnergyState` instead of `HPState` |
| `app/systems/render_system.cpp` | Draw energy bar (color ramp, flash, pulse) |
| `app/main.cpp` | Emplace `EnergyState`, call `energy_system`, update tick order |
| `CMakeLists.txt` | Rename source file if needed |
| `content/constants.json` | Add energy tuning values |
| `tests/*` | Update any test referencing `HPState` |

---

## Non-Goals (out of scope)

- **Difficulty-based energy tuning** — all difficulties use the same
  drain/recovery values for now.  Can be added later via `DifficultyConfig`.
- **Energy powerups / pickups** — not in this feature.
- **Energy affects scoring** — energy is purely survival.  Burnout
  multiplier remains the scoring risk/reward mechanic.
- **Passive energy drain over time** — energy only changes on note events.
