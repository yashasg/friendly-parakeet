# SHAPESHIFTER — Rhythm-Core Game Design
## Sr Game Designer Spec · v1.1

> **Technical spec:** See `rhythm-spec.md` for ECS components, system pipeline, and C++ implementation details.

```
  ┌─────────────────────────────────────────────────────────────┐
  │  SECTION 1:  Vision — This IS a Rhythm Game ............. 1 │
  │  SECTION 2:  Music Drives Everything .................... 2 │
  │  SECTION 3:  The Shape Window — Player Experience ....... 3 │
  │  SECTION 4:  The Proximity Ring ......................... 4 │
  │  SECTION 5:  Scoring — Timing Tiers ..................... 5 │
  │  SECTION 6:  Hexagon — The Rest Between Beats ........... 6 │
  │  SECTION 7:  MISS — Energy Drain ........................ 7 │
  │  SECTION 8:  Obstacle Types ............................. 8 │
  │  SECTION 9:  Beat Map Generation ........................ 9 │
  │  APPENDIX A: Glossary ................................... A │
  └─────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 1 — VISION: THIS IS A RHYTHM GAME
# ═══════════════════════════════════════════════════

## The Core Insight

```
  ╔═══════════════════════════════════════════════════════════════════╗
  ║                                                                   ║
  ║  "The obstacles ARE the beat.                                     ║
  ║   The shape press IS the player performing on beat.               ║
  ║   Without music driving obstacle spawning,                        ║
  ║   the timed shape window makes no sense."                         ║
  ║                                                                   ║
  ║   This is NOT "a bullet hell game with optional rhythm mode."     ║
  ║   This IS a rhythm game where shape-shifting is the performance.  ║
  ║                                                                   ║
  ╚═══════════════════════════════════════════════════════════════════╝
```

## Two Sources of Truth

```
  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │       ┌──────────────┐          ┌────────────────────┐          │
  │       │  BPM (fixed) │          │  BEAT MAP (authored)│         │
  │       └──────┬───────┘          └─────────┬──────────┘          │
  │              │                            │                      │
  │     ┌────────┼──────┐         ┌───────────┼──────────┐          │
  │     ▼        ▼      ▼         ▼           ▼          ▼          │
  │  ┌──────┐ ┌─────┐ ┌──────┐ ┌──────┐ ┌────────┐ ┌─────────┐    │
  │  │Scroll│ │Win- │ │Morph │ │Obst. │ │Pattern │ │Density/ │    │
  │  │Speed │ │dow  │ │Dur.  │ │Types │ │& Lanes │ │Rests    │    │
  │  └──────┘ └─────┘ └──────┘ └──────┘ └────────┘ └─────────┘    │
  │                                                                  │
  │   BPM:      governs timing windows, scroll speed, morph speed   │
  │   Beat map: governs WHAT appears and WHEN — authored per song   │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

## The Player's Contract

```
  ╔═══════════════════════════════════════════════════════════════════╗
  ║                                                                   ║
  ║  1. Music tells you WHEN something is coming (the ring shrinks). ║
  ║  2. The obstacle tells you WHAT shape to be.                     ║
  ║  3. Press the right button at the right time.                    ║
  ║  4. Miss = energy loss. Energy at 0 ends the run.                ║
  ║                                                                   ║
  ╚═══════════════════════════════════════════════════════════════════╝
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 2 — MUSIC DRIVES EVERYTHING
# ═══════════════════════════════════════════════════

## Librosa Analysis Pipeline

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                                                                 │
  │   Audio file                                                    │
  │       │                                                         │
  │       ▼                                                         │
  │   librosa onset detection  →  onset events per broad layer     │
  │       │                                                         │
  │       ▼                                                         │
  │   Split into three broad layer classes (PASS_TO_LAYER):         │
  │                                                                 │
  │   ┌────────────────┬────────────────┬───────────────────┐      │
  │   │  PERCUSSIVE    │  FULL-SPECTRUM │  HARMONIC         │      │
  │   │  bass /        │  spectral flux │  low-mid          │      │
  │   │  broadband /   │  (catch-all)   │  (sustained tone) │      │
  │   │  high-mid      │                │                   │      │
  │   │  e.g. kick,    │  e.g. dense    │  e.g. melody,     │      │
  │   │  hi-hat (illus)│  mix (illus)   │  pad (illus)      │      │
  │   └──────┬─────────┴──────┬─────────┴──────────┬────────┘      │
  │          │                │                    │                │
  │          ▼                ▼                    ▼                │
  │       onsets           onsets               onsets              │
  │          │                │                    │                │
  │          ▼                ▼                    ▼                │
  │       TRIANGLE          SQUARE              CIRCLE              │
  │       LANE 0            LANE 1              LANE 2              │
  │                                                                 │
  │   Each onset = one obstacle spawn candidate.                    │
  │   Snap to beat grid (within 80ms). That's your note.            │
  │                                                                 │
  │   Note: instrument names above are illustrative only. The       │
  │   pipeline classifies by broad layer, not by raw drum/         │
  │   instrument identity. Legacy "kick / snare / hi-hat" aliases   │
  │   are read-only compatibility shims and are stripped from any   │
  │   newly serialized analysis output.                             │
  │                                                                 │
  └─────────────────────────────────────────────────────────────────┘
```

## The Music Encodes the Level

The shape and lane of every obstacle come directly from which broad layer fired the onset. This is not a design choice — it is a read from the audio. The music writes the level. The mapping below is the authoritative `PASS_TO_LAYER` defined in `tools/rhythm_pipeline.py`:

```
  ┌────────────────────────────────────────────────────────────────┐
  │  Percussive onset at beat 4?    → Triangle gate, lane 0 (LEFT) │
  │  Full-spectrum onset at beat 6? → Square gate,   lane 1 (CTR)  │
  │  Harmonic onset at beat 7?      → Circle gate,   lane 2 (RIGHT)│
  │  Multiple layers at beat 8?     → Separate candidates by layer │
  │                                                                │
  │  (Instrument names like "kick / snare / hi-hat" are not part   │
  │   of the public vocabulary — they are illustrative cues only.) │
  └────────────────────────────────────────────────────────────────┘
```

## Randomness Comes From Obstacle Type, Not Timing

The player can hear the beat coming. The timing is legible. What the player cannot predict in advance is **which obstacle type will appear**:

```
  ┌────────────────────────────────────────────────────────────────┐
  │  On the same beat, the designer chose:                         │
  │                                                                │
  │    shape_gate  →  player must morph to the right shape        │
  │    low_bar     →  player must duck under                      │
  │    high_bar    →  player must jump over                       │
  │                                                                │
  │  The beat is audible. The obstacle type is the surprise.      │
  └────────────────────────────────────────────────────────────────┘
```

## BPM Is Fixed

```
  The BPM for a song does not change.
  librosa beat tracking gives one fixed value per track.
  Scroll speed, window size, and morph duration all derive from BPM.
  There are no tempo ramps or ritardandos.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 3 — THE SHAPE WINDOW — PLAYER EXPERIENCE
# ═══════════════════════════════════════════════════

## What the Player Sees and Does

```
  TIME ──────────────────────────────────────────────────────────▶

  Obstacle approaching from top:

        ┌──────────────────────────────────────────────────────┐
        │  FAR AWAY                  CLOSE      AT PLAYER      │
        │  ring is large             ring small  ring = button  │
        │                                                       │
        │     ⭕⭕⭕               ⭕⭕          ⭕             │
        │    ⭕ ◯ ⭕             ⭕ ◯ ⭕       ◯             │
        │     ⭕⭕⭕               ⭕⭕          ⭕             │
        │                                                       │
        │  [press here = BAD]  [press = GOOD]  [press = PERF]  │
        └──────────────────────────────────────────────────────┘

  Player action:
    1. See ring begin to shrink
    2. Watch ring approach button size
    3. Press the correct shape button when ring hits button edge
    4. ⬡ Hexagon returns automatically after the window closes
```

## Window Phase State Machine

```
  ┌────────┐  [press]   ┌──────────┐  [timer]   ┌────────┐
  │        │ ─────────▶ │          │ ─────────▶ │        │
  │  IDLE  │            │ MORPH IN │            │ ACTIVE │
  │   ⬡   │            │  ⬡→◯   │            │   ◯    │
  │        │            │          │            │        │
  └────────┘            └──────────┘            └────────┘
       ▲                                             │
       │                                          [timer]
       │                ┌──────────┐                │
       └─────────────── │ MORPH OUT│ ◀──────────────┘
         [hexagon back] │  ◯→⬡   │
                        └──────────┘

  Durations:
    MORPH IN:   0.150s  (visual blend from hexagon to target)
    ACTIVE:     (OK window × 2) × window_scale (scales UP with better timing)
    MORPH OUT:  0.150s  (visual blend back to hexagon)
```

## Window Scaling — The Perfect Press Reward

```
  ┌──────────────────────────────────────────────────────────────┐
  │  BAD timing  = remaining active window × 0.50               │
  │  OK timing   = remaining active window × 0.75               │
  │  GOOD timing = remaining active window × 1.00 (full time)   │
  │  PERFECT     = remaining active window × 1.50               │
  │                                                              │
  │  A PERFECT press extends the remaining window, giving the    │
  │  player extra time in the target shape. Better timing =      │
  │  more generous window. This is reward, not punishment.       │
  │                                                              │
  │  "I nailed it — I've got breathing room for the next one."  │
  └──────────────────────────────────────────────────────────────┘
```

## Interrupting a Window

```
  If the player presses a DIFFERENT shape while already in a window:

    ┌─────────────────────────────────────────────────┐
    │  Current window: ◯ (circle), Active phase       │
    │  Player presses: □ (square)                     │
    │                                                 │
    │  → Circle window CANCELS immediately             │
    │  → Square MorphIn begins                        │
    │  → If the circle obstacle arrives during this:  │
    │    shape mismatch → MISS → energy drain         │
    └─────────────────────────────────────────────────┘

  Indecision is punished. Commit to your shape.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 4 — THE PROXIMITY RING
# ═══════════════════════════════════════════════════

## What It Is

```
  ┌─────────────────────────────────────────────────────────────┐
  │                                                             │
  │  Each shape button has an outer ring that shrinks as the    │
  │  matching obstacle approaches. When the ring reaches the    │
  │  button's edge, it's the perfect moment to press.           │
  │                                                             │
  │  Three buttons. Three independent rings.                    │
  │  Only the nearest obstacle of each shape drives its ring.   │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

## Ring Lifecycle

```
  Obstacle spawns (y = SPAWN_Y, top of screen)
       │
       │  ring_radius = btn_radius × 2.0   [blue-grey]
       │
       ▼
  Obstacle travels down...
       │
       │  ring shrinks proportionally
       │
       ▼  ratio < 0.3 from perfect distance
       │
       │  [yellow-green — almost time]
       │
       ▼  obstacle at perfect_dist = (morph_dur + half_window) × scroll_speed
       │
       │  ring = btn_radius              [GREEN — press now!]
       │
       ▼
  Obstacle arrives at player y=920
       │
       ▼
  ScoredTag emplaced, ring disappears
```

## Why Not a Bar?

```
  ┌────────────────────────────────────────────────────────────┐
  │  A bottom bar fills up generically for "the nearest        │
  │  obstacle." It conflates all three shapes into one cue.    │
  │                                                            │
  │  The ring is per-shape:                                    │
  │    ◯ ring — tells you about the circle obstacle            │
  │    □ ring — tells you about the square obstacle            │
  │    △ ring — tells you about the triangle obstacle          │
  │                                                            │
  │  The player can have three different obstacles approaching  │
  │  simultaneously, each at a different distance.             │
  │  Three rings. Three independent countdowns.                │
  └────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 5 — SCORING — TIMING TIERS
# ═══════════════════════════════════════════════════

## Timing Windows

```
  ┌─────────────────────────────────────────────────────────────┐
  │                                                             │
  │  t_peak  =  moment the obstacle centre crosses player y     │
  │                                                             │
  │    pct_from_peak       Grade      Multiplier  Window Scale  │
  │   ─────────────────   ────────   ──────────  ────────────  │
  │       ≤ 25%           PERFECT     × 1.50      × 1.50      │
  │       ≤ 50%           GOOD        × 1.00      × 1.00      │
  │       ≤ 75%           OK          × 0.50      × 0.75      │
  │       > 75%           BAD         × 0.25      × 0.50      │
  │                                                             │
  │  Thresholds are percentage of half-window, not fixed ms.    │
  │  They scale dynamically with BPM.                           │
  │  Score = base_points × timing_multiplier × chain_multiplier. │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

## Chain Multiplier

```
  Each consecutive HIT (PERFECT, GOOD, or OK) grows the chain.
  Score = base_pts × (1 + chain × CHAIN_BONUS / 100)

  Chain resets on any MISS.

  Goal: play clean, stay in the zone, watch the multiplier climb.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 6 — HEXAGON — THE REST BETWEEN BEATS
# ═══════════════════════════════════════════════════

## Why Hexagon?

```
  ╔═════════════════════════════════════════════════════════════╗
  ║                                                             ║
  ║  The player's default state is ⬡ Hexagon.                  ║
  ║                                                             ║
  ║  Hexagon is NOT a shape the player can "stay as."           ║
  ║  No obstacle requires Hexagon. It has no scoring value.     ║
  ║  It is purely the REST state — the space between notes.     ║
  ║                                                             ║
  ║  Any obstacle arriving while the player is ⬡ = MISS        ║
  ║  (drains energy — see §7). The player must ALWAYS respond. ║
  ║  There is no neutral.                                       ║
  ║                                                             ║
  ╚═════════════════════════════════════════════════════════════╝
```

## Hexagon Design Principles

```
  1.  Distinct silhouette  — 6 sides, pointy-top orientation.
      Instantly recognisable as "I'm at rest."

  2.  Returns automatically  — after MorphOut, the player
      doesn't press anything. Hexagon is the default.
      Good timing (PERFECT/GOOD) returns to it faster.

  3.  No scoring  — Hexagon state contributes nothing to score.
      Every moment as Hexagon is an opportunity to be ready.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 7 — MISS — ENERGY DRAIN
# ═══════════════════════════════════════════════════

> **Shipped model:** SHAPESHIFTER uses the **Energy Bar** survival
> resource (see [`energy-bar.md`](energy-bar.md) and
> [`game.md`](game.md) §"Game Loop" / §"HUD Elements"). A MISS does
> **not** end the run on its own — it drains energy. The run ends only
> when energy reaches zero.
>
> **Legacy / superseded:** Earlier drafts of this section described an
> instant-death "ONE miss ends the run, no HP" model. That model is
> superseded by the energy bar shipped in `app/systems/energy_system.cpp`
> and the game-over check in
> [`app/systems/game_state_system.cpp:105`](../app/systems/game_state_system.cpp)
> (`energy && song && song->playing && energy->energy <= 0.0f`). Do not
> reintroduce instant-death framing in design docs without first
> revisiting `energy-bar.md`.

## The Decision

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  Misses cost energy. Energy at 0 ends the run.               │
  │                                                              │
  │  Why energy, not instant death:                              │
  │                                                              │
  │  • Forgiveness window for new players: a single mistimed     │
  │    press doesn't terminate the song. Learning is possible    │
  │    inside one run.                                           │
  │                                                              │
  │  • Every note still matters: misses cost a meaningful slice  │
  │    of energy and break the chain (see §5). Sloppy play       │
  │    bleeds out within a handful of misses.                    │
  │                                                              │
  │  • The proximity ring still gives ample warning. A miss is   │
  │    a genuine error, not a surprise — but the run can         │
  │    survive long enough for the player to recover.            │
  │                                                              │
  │  • Hits feed energy back (PERFECT/GOOD/OK recover; see       │
  │    `energy-bar.md`). Clean play sustains the run.            │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

## What Counts as a MISS

Each of the following emits a `MissTag` (see
`app/components/obstacle.h`), which `scoring_system` and
`energy_system` consume to drain energy and reset the chain. None of
them ends the run on their own — only `energy <= 0.0f` does.

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                                                                 │
  │  • Wrong shape when obstacle arrives         → MISS → drain    │
  │  • ⬡ Hexagon when shape_gate arrives         → MISS → drain    │
  │  • Active window closed without a valid press → MISS → drain    │
  │  • Pressing shape but in wrong lane          → MISS → drain    │
  │                                                                 │
  │  • low_bar / high_bar: no dodge action       → MISS → drain    │
  │    (future obstacle kinds — see §8 shipped-scope note)         │
  │                                                                 │
  │  Run ends only when energy reaches 0 (see `energy-bar.md`).    │
  │                                                                 │
  └─────────────────────────────────────────────────────────────────┘
```

> **Note on timing tiers:** §5's timing table has no upper bound above
> BAD — any press inside the active window grades as PERFECT / GOOD /
> OK / BAD. A "late press" only becomes a MISS when the active window
> closes with no valid press registered (i.e. it collapses into the
> "Hexagon when shape_gate arrives" case above).

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 8 — OBSTACLE TYPES
# ═══════════════════════════════════════════════════

> ⚠️ **Shipped scope — only `shape_gate` ships today (issues #420, #446,
> #328, #479).** Mirroring the caveat in `game.md` "Difficulty
> Progression": across all 9 shipped beatmap arrays in
> `content/beatmaps/` (**1046 obstacles total** as of Round 6 audit;
> see per-difficulty table in §8 "Difficulty Progression" below),
> `tools/level_designer.py` emits 100% `shape_gate`. The `lane_push`, `low_bar`, and `high_bar` types
> described below are **not currently produced** by the generator and
> are not part of any shipped run; `LanePush` is additionally queued
> for removal/rework (#328). They are retained here as forward design
> space for if/when committed plans to reintroduce them land. Treat
> Section 8 (and the "Difficulty Progression" block that follows) as
> the catalog of intended types, not a description of current shipped
> content.

## The Four Types

```
  ┌──────────────────────────────────────────────────────────────┐
  │  shape_gate   ← most common                                  │
  │  ─────────────────────────────────────────────────────────   │
  │                                                              │
  │     ┌───┐                                                    │
  │     │ ◯ │  ← obstacle in lane 0                             │
  │     └───┘                                                    │
  │                                                              │
  │  Player must be the matching shape (◯/□/△) in the           │
  │  correct lane when it arrives.                               │
  │  Shape/lane come from the broad layer onset:                 │
  │  percussive=triangle/0, full-spectrum=square/1, harmonic=◯/2.│
  │                                                              │
  ├──────────────────────────────────────────────────────────────┤
  │  ─────────────────────────────────────────────────────────   │
  │                                                              │
  │     ▲         ▼                                              │
  │     ▲  left   ▼  right                                      │
  │     ▲         ▼                                              │
  │                                                              │
  │  Passive obstacle — auto-pushes the player one lane in the   │
  │  indicated direction on beat arrival. No player action.      │
  │  Triggered by harmonic-layer onsets (illustrative: cymbal-     │
  │  like sustain). Edge pushes (left on Lane 0, right on Lane 2) │
  │  are no-ops. Awards 0 points.  (Replaces legacy lane_block.)  │
  │                                                              │
  ├──────────────────────────────────────────────────────────────┤
  │  low_bar                                                     │
  │  ─────────────────────────────────────────────────────────   │
  │                                                              │
  │  ══════════════════  ← bar at ground level                   │
  │                                                              │
  │  Player must jump over. Spans all lanes.                     │
  │  Used sparingly in hard mode drop sections.                  │
  │                                                              │
  ├──────────────────────────────────────────────────────────────┤
  │  high_bar                                                    │
  │  ─────────────────────────────────────────────────────────   │
  │                                                              │
  │  ══════════════════  ← bar at ceiling level                  │
  │                                                              │
  │  Player must duck under. Spans all lanes.                    │
  │  Used sparingly in hard mode drop sections.                  │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

## Difficulty Progression

> **Onset-only invariant (directive 2026-05-10).** Under the active
> `design_level_segment_focus` path in `tools/level_designer.py`
> (see §9 "Pipeline Overview"), an obstacle is emitted **only at
> beats where a real audio onset survives the flux threshold**.
> Beats with no detected onset stay empty regardless of difficulty,
> so phrasings like "every beat" or "streams every beat" cannot be
> produced by the shipped generator and have been retired below.

```
  EASY:    shape_gate only. Sparsest tier. Highest flux threshold,
           so only the strongest onsets survive into the beatmap;
           percussive + harmonic onset classes only (full-spectrum
           catch-all suppressed). 2-beat minimum gap kept where the
           onset density allows. Learning the shape mechanic.

  MEDIUM:  shape_gate only. Moderate flux threshold; more surviving
           onsets become obstacles. Density scales with the segment's
           detected intensity (verse vs. chorus/drop).

  HARD:    shape_gate only (in shipped content — see Section 8
           caveat). Highest-density tier: lowest flux threshold
           (~5%), so the largest set of detected onsets survives
           into obstacles. Density tracks segment intensity, with
           the densest sections appearing in the song's actual
           drops/choruses — but never exceeds the song's onset
           count, and any beat without a real onset remains empty.
           lane_push / low_bar / high_bar entries shown in §8 are
           forward design space and are not produced today.
```

### Shipped per-difficulty obstacle counts (Round 6 audit)

| Song                  | easy | medium | hard |
|-----------------------|-----:|-------:|-----:|
| 1_stomper             |   60 |     60 |   83 |
| 2_drama               |  122 |    125 |  180 |
| 3_mental_corruption   |  130 |    130 |  156 |
| **Total (all 9 arrays)** |      |        | **1046** |

Even Hard never approaches "every beat" — `1_stomper` Hard averages
roughly one obstacle every ~2.5 musical beats. Treat these counts as
the live ceiling on shipped difficulty density; regenerate from
`content/beatmaps/*beatmap.json` if the generator changes.

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 9 — BEAT MAP GENERATION
# ═══════════════════════════════════════════════════

## Pipeline Overview

> **Active path: onset-only (`design_level_segment_focus`,
> directive 2026-05-10).** This is the generator that ships today.
> See `tools/level_designer.py:125-134, :741, :1996, :3180`.
>
> **Invariant:** An obstacle is emitted **only at beats where a real
> audio onset survives the flux threshold**. Beat indices without a
> surviving onset produce no obstacle at any difficulty. Per-difficulty
> density is therefore bounded above by the song's surviving onset
> count, not by the beat count, and post-hoc cleanup passes are
> intentionally disabled on this path. Any "every beat" / "streams
> every beat" wording predates this directive and does not match the
> shipped output.

```
  ┌─────────────────────────────────────────────────────────────┐
  │                                                             │
  │   YouTube URL                                               │
  │       │                                                     │
  │       ▼                                                     │
  │   yt-dlp  →  WAV file                                       │
  │       │                                                     │
  │       ▼                                                     │
  │   librosa tempo / beat tracking → beat timestamps           │
  │   librosa STFT + mel features  → broad-layer onset pools    │
  │       │                                                     │
  │       ▼                                                     │
  │   percussive / harmonic / full-spectrum passes → onsets     │
  │   (flux thresholding — only surviving onsets continue)      │
  │       │                                                     │
  │       ▼                                                     │
  │   snap surviving onsets to beat grid (80ms tolerance)       │
  │   ── beats without a surviving onset stay empty ──          │
  │       │                                                     │
  │       ▼                                                     │
  │   structure detection  (onset density → intensity →         │
  │                          intro/verse/chorus/drop/outro)     │
  │       │                                                     │
  │       ▼                                                     │
  │   design_level_segment_focus (onset-only path, 2026-05-10)  │
  │   per-difficulty flux threshold selects which surviving     │
  │   onsets become obstacles → easy / medium / hard            │
  │       │                                                     │
  │       ▼                                                     │
  │   Rhythm Designer agent  →  beatmap.json                    │
  │   (cleanup passes intentionally DISABLED on this path)      │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

For shipped per-difficulty obstacle counts, see the table in
§8 "Difficulty Progression" above (regenerate from
`content/beatmaps/*beatmap.json` if the generator changes).

## Beat Map File

```
  content/beatmaps/<song_id>.json

  Contains:
    • song_id, title, bpm, offset, duration_sec
    • difficulties: easy / medium / hard obstacle arrays
    • structure: section breakdown with intensity labels

  Generated by: .github/agents/rhythm-designer.agent.md
```

---
---
---

# ═══════════════════════════════════════════════════
# APPENDIX A — GLOSSARY
# ═══════════════════════════════════════════════════

```
  Beat map     Authored JSON file mapping song timestamps to obstacles.
               Source of truth for what spawns and when.

  BPM          Beats per minute. Fixed per song. Drives all timing.

  Shape gate   Obstacle requiring the player to match its shape in its lane.

  Lane push    Passive obstacle that moves the player one lane in a
               direction on beat arrival (replaces legacy Lane Block).

  Proximity    Ring around a shape button that shrinks as the matching
  ring         obstacle approaches. Reaches button size at perfect timing.

  Window       The time interval during which a shape press is valid.
  (active)     Default duration = OK window × 2. Scales up on PERFECT/GOOD.

  MorphIn      Phase where player visually transitions from ⬡ to target shape.
               Duration = 0.150s. Shape is not yet active for collision.

  Active       Phase where player is fully in target shape. Collision accepted.

  MorphOut     Phase where player transitions back to ⬡ after window closes.

  window_scale Factor applied to remaining active window based on timing.
               PERFECT=1.50, GOOD=1.00, OK=0.75, BAD=0.50.

  Hexagon (⬡)  Default/rest state. No obstacle ever requires it.
               Any obstacle arriving while in ⬡ = MISS (energy drain).
               See §7 and `energy-bar.md`.

  MISS         Shape mismatch, wrong lane, or active window closing
               with no valid press. Drains energy and resets the chain
               (see `energy_system.cpp` / `scoring_system.cpp`). The
               run ends only when energy reaches 0 — see §7 and
               `energy-bar.md`. The "instant game over, no HP" model
               is legacy and superseded by the energy bar.

  Onset        Moment of sudden energy increase detected by librosa
               on one of the broad layers (percussive / harmonic /
               full-spectrum). Percussive → triangle / lane 0.
               Full-spectrum → square / lane 1.
               Harmonic → circle / lane 2.

  Layer        One of three broad librosa layer classes used for
               classification: percussive, harmonic, full-spectrum.
               Replaces the legacy low/mid/high frequency-band model;
               raw drum/instrument names (kick/snare/hi-hat) are
               read-only legacy aliases only.
```
