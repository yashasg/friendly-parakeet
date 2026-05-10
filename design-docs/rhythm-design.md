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
  │  SECTION 7:  MISS = Game Over ........................... 7 │
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
  ║  4. Miss = instant end. No second chances.                       ║
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
    │    shape mismatch → MISS → game over            │
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
  ║  Any obstacle arriving while the player is ⬡ = MISS.       ║
  ║  The player must ALWAYS respond. There is no neutral.       ║
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
# SECTION 7 — MISS = GAME OVER
# ═══════════════════════════════════════════════════

## The Decision

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  There is NO HP bar. ONE miss ends the run.                  │
  │                                                              │
  │  Why:                                                        │
  │                                                              │
  │  • This is an arcade runner, not a 3-minute song test.       │
  │    Sessions are short. Restarts are fast and expected.        │
  │                                                              │
  │  • HP drain removes tension. Every obstacle would become     │
  │    "it's fine, I have 4 more." Instant fail means every      │
  │    obstacle matters every time.                              │
  │                                                              │
  │  • The proximity ring gives ample warning. A miss is a       │
  │    genuine error, not a surprise. The player had the cue.    │
  │                                                              │
  │  • This mirrors the best arcade rhythm games: one mistake,  │
  │    full restart, higher stakes, stronger memory formation.   │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

## What Counts as a MISS

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                                                                 │
  │  • Wrong shape when obstacle arrives         → MISS → END      │
  │  • ⬡ Hexagon when shape_gate arrives         → MISS → END      │
  │  • Timing outside BAD window (> 75% off peak) → MISS → END      │
  │  • Pressing shape but in wrong lane          → MISS → END      │
  │                                                                 │
  │  • low_bar / high_bar: no dodge action       → MISS → END      │
  │                                                                 │
  └─────────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 8 — OBSTACLE TYPES
# ═══════════════════════════════════════════════════

> ⚠️ **Shipped scope — only `shape_gate` ships today (issues #420, #446,
> #328, #479).** Mirroring the caveat in `game.md` "Difficulty
> Progression": across all 9 shipped beatmaps in `content/beatmaps/`
> (994 obstacles total), `tools/level_designer.py` emits 100%
> `shape_gate`. The `lane_push`, `low_bar`, and `high_bar` types
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

```
  EASY:    shape_gate only. Sparse. 2-beat minimum gap.
           Percussive + harmonic onsets only (full-spectrum
           catch-all suppressed). Learning the shape mechanic.

           Density scales with song intensity section.

  HARD:    All four types active. Dense.
           DROP sections: every beat, streams of shape_gates,
           low_bar and high_bar every 4th beat.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 9 — BEAT MAP GENERATION
# ═══════════════════════════════════════════════════

## Pipeline Overview

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
  │       │                                                     │
  │       ▼                                                     │
  │   snap to beat grid (80ms tolerance)                        │
  │       │                                                     │
  │       ▼                                                     │
  │   structure detection  (onset density → intensity →         │
  │                          intro/verse/chorus/drop/outro)     │
  │       │                                                     │
  │       ▼                                                     │
  │   Rhythm Designer agent  →  beatmap.json                    │
  │   (applies difficulty rules, outputs easy/medium/hard)      │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

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
               Any obstacle arriving while in ⬡ = MISS = game over.

  MISS         Shape mismatch, wrong lane, or late press (>75% off peak).
               Always instant game over. No HP. No second chances.

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
