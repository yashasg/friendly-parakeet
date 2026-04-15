# Beatmap Design Guidelines — Lessons from Test Player Analysis

> **Source**: Analysis of `session_pro_20260410_225016.log` on `1_stomper` medium difficulty.

---

## Issue 1: Two-Lane Jumps (Lane 0 ↔ Lane 2)

### Problem

20% of obstacles require the player to jump 2 lanes (left ↔ right).
The test player handles this mechanically in ~0.06s, but a human must
swipe twice — adding 0.3-0.5s of processing time.

```
  Human experience:

  Lane 0         Lane 1         Lane 2
    ◆ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ▶ ★
    │                                  │
    │  swipe 1       swipe 2           │
    │  (0.08s)  +  reaction (0.2s)     │
    │           +  swipe 2 (0.08s)     │
    │  = total ~0.36s                  │
    │                                  │
    └── vs obstacle at gap=2 (0.82s) ──┘
         only 0.82s total! tight!

  Guideline: For 2-lane jumps, give at least gap=3 (1.23s)
```

### Guideline

```
  ┌─────────────────────────────────────────────────────────────┐
  │  Lane Movement        │ Min Gap (beats) │ Time @ 146 BPM   │
  ├───────────────────────┼─────────────────┼───────────────────┤
  │  Same lane            │ 2               │ 0.82s             │
  │  Adjacent lane (0↔1)  │ 2               │ 0.82s             │
  │  Cross lane (0↔2)     │ 3               │ 1.23s             │
  │  Cross + shape change │ 4               │ 1.64s             │
  └───────────────────────┴─────────────────┴───────────────────┘
```

Prefer stepping through the center:

```
  ✗ BAD:   Lane 0 ─────────────────▶ Lane 2  (2-lane jump)
  ✓ GOOD:  Lane 0 ──▶ Lane 1 ──▶ Lane 2     (two 1-lane steps)
```

---

## Issue 2: Rhythm Integration — Player Should Feel the Music

### Problem

The current beatmap places obstacles at mechanically spaced intervals
(mostly every 2 beats) with no relationship to the song's musical
structure. Collisions land ~0.07s off-beat. The music feels like
background noise rather than the driving force.

### What "Feeling the Rhythm" Means

```
  CURRENT (mechanical):
  ─────────────────────────────────────────────
  Beat:  1  2  3  4  5  6  7  8  9 10 11 12
  Obs:         ★     ★     ★     ★     ★
  Gap:         2     2     2     2     2
  Feel:  monotonous, treadmill, music irrelevant

  IDEAL (musical):
  ─────────────────────────────────────────────
  Beat:  1  2  3  4  5  6  7  8  9 10 11 12
  Song:  ♩  ♩  ♩  ♩ |REST| ♩♩ ♩  ♩ |BUILD |
  Obs:      ★     ★        ★★ ★     ★  ★ ★
  Gap:      2     2        1  2     3  1 1
  Feel:  breathing, tension, release, climax
```

### Design Principles

```
  1. OBSTACLES ON THE BEAT
  ────────────────────────
  Collisions should land EXACTLY on beats, not 0.07s off.
  This is likely a calibration issue with offset/lead_time.

  2. FOLLOW THE SONG STRUCTURE
  ────────────────────────────
  ┌────────────┬─────────────────────────────────────────────┐
  │ Section    │ Obstacle Design                             │
  ├────────────┼─────────────────────────────────────────────┤
  │ Intro      │ Sparse, same shape, center lane only        │
  │ Verse      │ Steady rhythm, simple patterns              │
  │ Pre-chorus │ Density increases, new shapes introduced    │
  │ Chorus     │ Peak density, all shapes + lane changes     │
  │ Bridge     │ Breathing room, new obstacle types           │
  │ Outro      │ Callback to intro, wind down                │
  └────────────┴─────────────────────────────────────────────┘

  3. RHYTHMIC VARIETY
  ───────────────────
  Mix gaps to create musical phrases:

  Phrase A (4 beats): ★ . ★ .     (gap=2, steady)
  Phrase B (4 beats): ★ ★ . .     (gap=1+2, syncopated)
  Phrase C (4 beats): . . . ★     (gap=4, anticipation)
  Phrase D (4 beats): ★ ★ ★ .     (gap=1+1, rapid-fire)

  Combine phrases into musical sentences that follow the song:
  Verse:   A A B C | A A B C      (16 beats, repeating)
  Chorus:  D D B A | D D B A      (16 beats, intense)

  4. REST = MUSICAL RESTS
  ───────────────────────
  Gaps > 4 beats should align with actual musical breaks.
  Don't put a 7-beat gap in the middle of a chorus.
  Don't fill a musical break with obstacles.

  5. OBSTACLE TYPE MATCHES MUSICAL ELEMENT
  ─────────────────────────────────────────
  ┌──────────────┬────────────────────────────────────────┐
  │ Obstacle     │ Musical Mapping                        │
  ├──────────────┼────────────────────────────────────────┤
  │ ShapeGate    │ Melodic hits (main instrument)         │
  │ LanePush     │ Bass drops / percussion fills          │
  │ LowBar/Jump  │ Cymbal crashes / high notes            │
  │ HighBar/Slide│ Bass notes / kick drum                 │
  │ ComboGate    │ Complex chord hits                     │
  └──────────────┴────────────────────────────────────────┘
```

### Current vs Target Stats

```
  ┌────────────────────┬───────────┬───────────────────────┐
  │ Metric             │ Current   │ Target                │
  ├────────────────────┼───────────┼───────────────────────┤
  │ Obstacle variety   │ 96% shape │ 60% shape, 20% lane,  │
  │                    │  4% lane  │  10% jump, 10% combo  │
  │ Gap variety        │ 53% gap=2 │ No gap > 40% of total │
  │ 2-lane jumps       │ 20%       │ < 5% (with min gap=3) │
  │ On-beat collisions │ 3%        │ > 95%                 │
  │ Musical phrasing   │ None      │ Matches song sections │
  └────────────────────┴───────────┴───────────────────────┘
```

---

## Offset Calibration Note

Collisions consistently land 0.07s before the beat. This suggests the
`offset` value in the beatmap (1.694s) may need fine-tuning, or the
`lead_time` calculation introduces a small systematic error. Investigate
whether adjusting `offset += 0.07` brings collisions on-beat.
