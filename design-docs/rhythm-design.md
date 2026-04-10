# SHAPESHIFTER — Rhythm-Core Game Design
## Sr Game Designer Spec · v1.0

> **Core Insight:** The obstacles ARE the beat. The shape press IS the performance.
> **Technical spec:** See `rhythm-spec.md` for ECS components, system pipeline, and C++ implementation details.
> **Depends on:** Burnout Scoring (SPEC 2), Input System (SPEC 1)

```
  ┌─────────────────────────────────────────────────────────────┐
  │  SECTION 1:  Vision — This IS a Rhythm Game ............. 1 │
  │  SECTION 2:  Music Drives Everything .................... 2 │
  │  SECTION 3:  The Complete Lifecycle ..................... 3 │
  │  SECTION 4:  Shape Window Mapped to Beats .............. 4 │
  │  SECTION 5:  Scoring — Timing × Burnout ................ 5 │
  │  SECTION 6:  Hexagon — The Rest Between Beats .......... 6 │
  │  SECTION 7:  Beat Map Generation ..................... 7 │
  │  SECTION 8:  Acceptance Criteria ...................... 8 │
  │  APPENDIX A: Glossary ................................ A │
  │  APPENDIX B: User Stories ............................ B │
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

## What Changed — And Why

```
  ┌──────────────────────────────────────────────────────────────────┐
  │  OLD MODEL (shape-window.md):                                    │
  │                                                                  │
  │    ┌──────────┐    ┌──────────┐    ┌──────────┐                 │
  │    │ Obstacle │    │  Shape   │    │  Rhythm  │                 │
  │    │ Spawner  │    │  Window  │    │  Music?  │                 │
  │    └────┬─────┘    └────┬─────┘    └────┬─────┘                 │
  │         │               │               │                        │
  │         │  timer-based  │  independent  │ ???                    │
  │         │  (random)     │  (timed)      │ (not designed)         │
  │         │               │               │                        │
  │         └───────────────┴───────────────┘                        │
  │              Three separate concerns,                            │
  │              awkwardly bolted together                            │
  │                                                                  │
  │  NEW MODEL (this document):                                      │
  │                                                                  │
  │       ┌──────────┐              ┌────────────────┐               │
  │       │   BPM    │              │   BEAT MAP     │               │
  │       │ (tempo)  │              │  (authored)    │               │
  │       └────┬─────┘              └───────┬────────┘               │
  │            │                            │                        │
  │     ┌──────┼──────┐          ┌──────────┼──────────┐             │
  │     ▼      ▼      ▼          ▼          ▼          ▼             │
  │  ┌──────┐┌────┐┌──────┐  ┌──────┐ ┌────────┐ ┌────────┐        │
  │  │Scroll││Win-││Morph │  │Obst. │ │Pattern │ │Density │        │
  │  │Speed ││dow ││Dur   │  │Types │ │& Lanes │ │& Rests │        │
  │  │      ││Dur ││      │  │Shape │ │Spatial │ │Intensity│       │
  │  └──────┘└────┘└──────┘  └──────┘ └────────┘ └────────┘        │
  │                                                                  │
  │   TWO sources of truth (like Beat Saber):                        │
  │     BPM      → timing, speed, window sizing                      │
  │     Beat Map → what type, where, how dense, spatial layout       │
  └──────────────────────────────────────────────────────────────────┘
```

## Design Pillars

```
  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  PILLAR 1: SONGS ARE LEVELS                                      │
  │  ─────────────────────                                           │
  │  Each song has a hand-authored beat map that charts WHEN          │
  │  obstacles spawn and on which beats. The randomness comes        │
  │  from obstacle TYPES — the player must react to WHAT shape       │
  │  or obstacle is coming, not just WHEN it arrives.                │
  │  First play = discovery + reaction.                              │
  │  Replays = mastery through practice (same chart every time).     │
  │                                                                  │
  │  PILLAR 2: THE BEAT IS THE LAW                                   │
  │  ────────────────────────────                                    │
  │  Every obstacle spawns ON a beat. Every obstacle ARRIVES         │
  │  ON the beat it represents. Scroll speed is derived from        │
  │  BPM so this is mathematically guaranteed.                       │
  │                                                                  │
  │  PILLAR 3: SHAPE-SHIFTING IS PERFORMING                          │
  │  ─────────────────────────────────────                           │
  │  The player's button press IS their musical performance.         │
  │  PERFECT = on-beat. BAD = off-beat. MISS = wrong note.          │
  │  The hexagon is the rest between notes.                          │
  │                                                                  │
  │  PILLAR 4: RISK MEETS RHYTHM                                     │
  │  ──────────────────────────                                      │
  │  Burnout (how LATE you wait) and Timing (how ON-BEAT            │
  │  you are) CONVERGE at optimal play. Peak burnout IS             │
  │  peak timing IS the beat arrival. One moment of truth.          │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

## The Player Experience — A Song

```
  Imagine playing a 3-minute song at 120 BPM:

  ♩ = 120  │ ♪♫ Music plays, bass thumps on each beat
           │
  Beat 1   │  ▲ gate scrolls down... player sees it coming...
  Beat 2   │    ...getting closer... they anticipate...
  Beat 3   │      ...player presses ▲... window opens...
  Beat 4   │        ★ PERFECT! Gate arrives ON the beat,
           │          right at the window PEAK!
           │
           │  ⬡ hexagon rests... next beat approaches...
           │
  Beat 5   │  ● gate scrolls down...
  Beat 6   │    ...player presses ● early...
  Beat 7   │      GOOD — close but not on-beat
           │
           │  ⬡ rests...
           │
  Beat 8   │  ■ gate arrives — player forgot to press!
           │    💀 MISS — hexagon crashes into gate

  The song ends. Grade screen: 47 PERFECT, 12 GOOD, 3 OK, 1 MISS
  Total score: 18,420 — rank B. "Try again for that S rank!"

  THIS is the game. Not an endless runner. A song you PLAY.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 2 — MUSIC DRIVES EVERYTHING
# ═══════════════════════════════════════════════════

## The Causal Chain

```
  SONG FILE (.ogg/.wav)
       │
       │  analyzed by aubio (offline, at author time)
       ▼
  ┌──────────────────────────────────────────────────────────┐
  │  AUDIO ANALYSIS PIPELINE (aubio)                         │
  │                                                          │
  │  ┌──────────┐  ┌───────────┐  ┌──────────┐              │
  │  │  tempo   │  │  onset    │  │ melbands │              │
  │  │  → BPM   │  │  → WHEN   │  │  → WHAT  │              │
  │  │  (fixed) │  │  (times)  │  │  (freq)  │              │
  │  └────┬─────┘  └─────┬─────┘  └────┬─────┘              │
  │       │              │             │                     │
  │       │              └──────┬──────┘                     │
  │       │                     │                            │
  │       │              classify onsets                     │
  │       │              by frequency band:                  │
  │       │                                                  │
  │       │              low  → ● (kick/bass)               │
  │       │              mid  → ■ (snare/clap)              │
  │       │              high → ▲ (hi-hat/synth)            │
  │       │                     │                            │
  └───────┼─────────────────────┼────────────────────────────┘
          │                     │
          ▼                     ▼
  ┌──────────────┐   ┌──────────────────────────┐
  │  BPM → game  │   │  BEAT MAP (JSON)         │
  │  constants:  │   │  onset times + shapes    │
  │  scroll spd, │   │  + lanes                 │
  │  window dur  │   │  (auto-generated, then   │
  │              │   │   hand-tuned)            │
  └──────────────┘   └──────────┬───────────────┘
                                │
                     spawns obstacles on beat...
                                ▼
  ┌──────────────────────────────────────────────────────────┐
  │  The obstacle scrolls at scroll_speed and ARRIVES at     │
  │  the player exactly at the onset time from the beat map. │
  └──────────────────────────────────────────────────────────┘
```

## BPM → Game Constants

```
  ┌──────────┬──────────┬──────────┬──────────┬──────────┐
  │          │  80 BPM  │ 120 BPM  │ 160 BPM  │ 200 BPM  │
  │          │  (easy)  │ (medium) │  (hard)  │ (insane) │
  ├──────────┼──────────┼──────────┼──────────┼──────────┤
  │beat_per  │ 0.750s   │ 0.500s   │ 0.375s   │ 0.300s   │
  │lead_time │ 3.000s   │ 2.000s   │ 1.500s   │ 1.200s   │
  │scroll_spd│ 347 px/s │ 520 px/s │ 693 px/s │ 867 px/s │
  │window_dur│ 1.20s    │ 0.80s    │ 0.60s    │ 0.48s    │
  │morph_dur │ 0.12s    │ 0.10s    │ 0.09s    │ 0.08s    │
  │PERF zone │ 0.15s    │ 0.10s    │ 0.075s   │ 0.06s    │
  └──────────┴──────────┴──────────┴──────────┴──────────┘

  approach_dist = |SPAWN_Y - PLAYER_Y|
                = |-120 - 920|
                = 1040 px

  scroll_speed  = approach_dist / lead_time
                = 1040 / (lead_beats * beat_period)

  lead_beats    = 4 beats (default, tunable per song/difficulty)
```

## Difficulty = BPM × Chart Design (Not a Timer)

```
  ┌──────────────────────────────────────────────────────────────────┐
  │  OLD: difficulty_system ramped speed over 180 seconds            │
  │       → random, no musical connection, no practice/mastery       │
  │                                                                  │
  │  NEW: Difficulty comes from TWO dimensions (like Beat Saber):    │
  │                                                                  │
  │  DIMENSION 1 — TEMPO (BPM):                                     │
  │       → 80 BPM = slow scrolling, generous windows               │
  │       → 200 BPM = fast scrolling, razor-thin PERFECT zones      │
  │       → Controls reaction time, scroll speed, window sizing      │
  │                                                                  │
  │  DIMENSION 2 — CHART DESIGN (Beat Map):                          │
  │       → Pattern complexity: shape sequences, lane patterns       │
  │       → Obstacle density: every 4th beat vs every beat           │
  │       → Type mixing: shape gates + lane blocks + bars            │
  │       → Spatial layout: center-only vs L-C-R movement           │
  │       → Rest distribution: breathing room vs relentless          │
  │       → Intensity curve: build-up, climax, cool-down sections   │
  │                                                                  │
  │  Same BPM, different chart = DIFFERENT difficulty.               │
  │  (An Easy chart at 120 BPM vs an Expert chart at 120 BPM.)      │
  │                                                                  │
  │  Difficulty is no longer time-based — it's song-based.           │
  │  Chart complexity is authored, not computed.                      │
  └──────────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 3 — THE COMPLETE LIFECYCLE
# ═══════════════════════════════════════════════════

## One Obstacle's Journey: Beat Map → Score

```
  BEAT MAP says: "Beat 16: Triangle gate, center lane"

  ════════════════════════════════════════════════════════════════

  STEP 1: SCHEDULE (beat_scheduler_system)
  ─────────────────────────────────────────

    beat_time(16) = song_offset + 16 * beat_period
                  = 0.200 + 16 * 0.500
                  = 8.200s into the song

    spawn_time    = 8.200 - lead_time
                  = 8.200 - 2.000
                  = 6.200s into the song

    At t=6.200s, spawn_system creates the obstacle entity.

  ════════════════════════════════════════════════════════════════

  STEP 2: SPAWN (beat_spawn_system)
  ──────────────────────────────────

    Entity created with:
      Position { x: 360, y: -120 }        ← SPAWN_Y (off-screen top)
      Velocity { dx: 0, dy: 520 }         ← scroll_speed
      RequiredShape { shape: Triangle }
      BeatInfo { beat_index: 16, arrival_time: 8.200 }
      Obstacle { kind: ShapeGate, base_points: 200 }

  ════════════════════════════════════════════════════════════════

  STEP 3: SCROLL (scroll_system, unchanged)
  ──────────────────────────────────────────

    Obstacle moves down at 520 px/s.

    t=6.200  y=-120   ┊           ▲gate          ┊ just spawned
    t=6.700  y= 140   ┊        ▲gate             ┊ player sees it
    t=7.200  y= 400   ┊     ▲gate                ┊ approaching...
    t=7.700  y= 660   ┊   ▲gate                  ┊ getting close!
    t=8.200  y= 920   ┊  ▲gate ← ON THE BEAT     ┊ arrival!

  ════════════════════════════════════════════════════════════════

  STEP 4: PLAYER ANTICIPATES
  ───────────────────────────

    Player sees ▲gate scrolling down. They know from the music:
    "The next beat is coming... I need to press ▲ so my window
     PEAKS when the gate arrives."

    window_duration = 0.80s at 120 BPM
    half_window     = 0.40s
    → player should press ▲ about 0.40s BEFORE the beat

    Ideal press time = 8.200 - 0.40 = 7.800s

  ════════════════════════════════════════════════════════════════

  STEP 5: PLAYER PRESSES SHAPE (player_action_system)
  ────────────────────────────────────────────────────

    Player presses ▲ at t=7.780s (close to ideal!)

    PlayerShape transitions:
      phase:        Idle → MorphIn
      target_shape: Triangle
      window_timer: 0.0
      window_start: 7.780

  ════════════════════════════════════════════════════════════════

  STEP 6: WINDOW OPENS (shape_window_system)
  ──────────────────────────────────────────

    t=7.780   MorphIn  begins (⬡ → ▲)     morph_dur = 0.10s
    t=7.880   Active   begins (▲ fully)    ← window is LIVE
    t=8.180   window PEAK (center of Active phase)
    t=8.480   Active   ends
    t=8.580   MorphOut ends → back to ⬡

    PEAK TIME = window_start + morph_in + (window_dur / 2)
              = 7.780 + 0.10 + 0.40
              = 8.280s

  ════════════════════════════════════════════════════════════════

  STEP 7: OBSTACLE ARRIVES → COLLISION (collision_system)
  ───────────────────────────────────────────────────────

    At t=8.200, obstacle reaches PLAYER_Y.
    Player is in Active phase. Shape = Triangle. Gate needs Triangle.
    MATCH ✓

    Now: HOW CLOSE to the window peak?

    time_from_peak = |8.280 - 8.200| = 0.080s
    half_window    = 0.40s
    pct_from_peak  = 0.080 / 0.40 = 20%

    20% < 25% → PERFECT! ★

  ════════════════════════════════════════════════════════════════

  STEP 8: GRADE + BURNOUT → SCORE (scoring_system)
  ─────────────────────────────────────────────────

    timing_mult   = 1.50  (PERFECT)
    burnout_mult  = 3.80  (obstacle was in DANGER zone at collision)

    final_score   = floor(200 * 1.50 * 3.80)
                  = floor(1140)
                  = 1140 pts!

    Popup: "★ PERFECT!" + "CLUTCH! ×3.8" → "1140"

  ════════════════════════════════════════════════════════════════

  STEP 9: REVERT → REST (shape_window_system)
  ────────────────────────────────────────────

    t=8.480  Active ends → MorphOut
    t=8.580  MorphOut complete → ⬡ Hexagon

    Player is at rest. Ready for the next beat.
```

## Lifecycle Diagram (One-Page Summary)

```
                    ┌────────────────────────────────────────┐
                    │            BEAT MAP JSON               │
                    │  { beat:16, shape:"triangle", lane:1 } │
                    └──────────────────┬─────────────────────┘
                                       │
                   ┌───────────────────┼──────────────────────┐
                   │           beat_scheduler_system           │
                   │  computes spawn_time from beat_time       │
                   │  and lead_time                            │
                   └───────────────────┬──────────────────────┘
                                       │ when song_time >= spawn_time
                                       ▼
                   ┌──────────────────────────────────────────┐
                   │           beat_spawn_system               │
                   │  creates entity: Position, Velocity,      │
                   │  RequiredShape, BeatInfo, Obstacle         │
                   └───────────────────┬──────────────────────┘
                                       │
                                       ▼
                   ┌──────────────────────────────────────────┐
                   │           scroll_system (unchanged)       │
                   │  obstacle moves toward PLAYER_Y           │
                   └───────────────────┬──────────────────────┘
                                       │
                 PLAYER SEES IT ◄──────┘
                   │
                   │ player anticipates the beat...
                   │ presses shape button ~half_window before arrival
                   │
                   ▼
                   ┌──────────────────────────────────────────┐
                   │           shape_window_system             │
                   │  IDLE → MORPH_IN → ACTIVE → MORPH_OUT    │
                   │  peak is at center of ACTIVE phase        │
                   └───────────────────┬──────────────────────┘
                                       │
                        obstacle arrives during ACTIVE phase
                                       │
                                       ▼
                   ┌──────────────────────────────────────────┐
                   │           collision_system (modified)     │
                   │  checks shape match                       │
                   │  computes timing_grade from peak distance  │
                   │  emplaces TimingGrade + ScoredTag          │
                   └───────────────────┬──────────────────────┘
                                       │
                                       ▼
                   ┌──────────────────────────────────────────┐
                   │           scoring_system (modified)       │
                   │  final = base × timing_mult × burnout     │
                   │  spawns popup: grade text + points         │
                   └──────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 4 — SHAPE WINDOW MAPPED TO BEATS
# ═══════════════════════════════════════════════════

## The Window IS the Performance

```
  In a traditional rhythm game (DDR, osu!):
    → Note scrolls toward a hit zone
    → Player presses button when note reaches zone
    → Grade = how close to the zone center

  In Shapeshifter:
    → Obstacle scrolls toward player
    → Player presses shape button BEFORE obstacle arrives
    → A window opens with a PEAK at its center
    → Grade = how close the OBSTACLE ARRIVAL is to the PEAK

  ┌───────────────────────────────────────────────────────────┐
  │  KEY DIFFERENCE: In DDR, you press ON the beat.           │
  │  In Shapeshifter, you press BEFORE the beat so your       │
  │  window peaks AT the beat. You're predicting the beat.    │
  └───────────────────────────────────────────────────────────┘
```

## Window Anatomy Mapped to Beat Arrival

```
  ← time ──────────────────────────────────────────────── →

  Player         Morph                                    Morph
  presses ▲      completes     PEAK (= beat arrival)     reverts
     │              │              │                        │
     ▼              ▼              ▼                        ▼
  ───⬡⬡⬡────────╱▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲★▲▲▲▲▲▲▲▲▲▲▲▲▲▲╲────⬡⬡⬡───
                 │                │                │
                 │    ACTIVE      │     ACTIVE     │
                 │   (pre-peak)   │   (post-peak)  │
                 │                │                │
                 ◄── half_window ─►── half_window ─►

  Intensity:
  1.0 ┤                ╭──────────╮
      │              ╭╯ PERFECT  ╰╮
  0.8 ┤            ╭╯    zone     ╰╮
      │          ╭╯                 ╰╮
  0.6 ┤        ╭╯  GOOD    GOOD      ╰╮
      │      ╭╯                        ╰╮
  0.4 ┤    ╭╯   OK          OK          ╰╮
      │  ╭╯                               ╰╮
  0.2 ┤╭╯    BAD               BAD          ╰╮
      │╯                                      ╰─
  0.0 ┤
      └──┬──────┬──────┬──────┬──────┬──────┬────
       morph          ACTIVE        ACTIVE      morph
        in          (pre-peak)     (post-peak)   out
                            │
                            ★ = beat arrival
                            ★ = obstacle at PLAYER_Y
                            ★ = OPTIMAL timing
```

## Ideal Press Timing

```
  The player should press the shape button so that:

    press_time + morph_in_dur + half_window = beat_arrival_time

  Therefore:

    ideal_press_time = beat_arrival_time - morph_in_dur - half_window

  At 120 BPM:
    beat_arrival = 8.200s
    morph_in     = 0.10s
    half_window  = 0.40s

    ideal_press  = 8.200 - 0.10 - 0.40 = 7.700s

  The player presses HALF A SECOND before the beat.
  This is the "anticipation" that makes it a rhythm game.
```

## Window Duration Formula (BPM-Driven)

```
  BASE window_duration = BASE_WINDOW_BEATS * beat_period

  where:
    BASE_WINDOW_BEATS = 1.6 beats (tunable)
    beat_period        = 60.0 / bpm

  ┌──────────┬──────────┬─────────────┬────────────┐
  │  BPM     │ beat_per │ window_dur  │ half_window│
  ├──────────┼──────────┼─────────────┼────────────┤
  │  80      │ 0.750s   │ 1.200s      │ 0.600s     │
  │ 120      │ 0.500s   │ 0.800s      │ 0.400s     │
  │ 160      │ 0.375s   │ 0.600s      │ 0.300s     │
  │ 200      │ 0.300s   │ 0.480s      │ 0.240s     │
  └──────────┴──────────┴─────────────┴────────────┘

  MIN_WINDOW = 0.36s (floor, never shorter)
```

## Window Scaling by Timing Grade

```
  ┌──────────────────────────────────────────────────────────────┐
  │  PROBLEM:                                                    │
  │  If the player nails a PERFECT, the obstacle arrives at the  │
  │  peak (center of window). But they're STUCK in the changed   │
  │  shape for the entire remaining window. Better timing =      │
  │  more time trapped. That's a penalty for precision.          │
  │                                                              │
  │  SOLUTION:                                                   │
  │  The base window duration is calibrated for OK timing.       │
  │  For grades ABOVE OK (GOOD, PERFECT), the remaining          │
  │  window SCALES DOWN — the shape snaps back to hexagon        │
  │  sooner. Better timing = faster recovery.                    │
  └──────────────────────────────────────────────────────────────┘

  ┌──────────┬─────────────────────────────────────────────────┐
  │  GRADE   │  WINDOW BEHAVIOR                                │
  ├──────────┼─────────────────────────────────────────────────┤
  │ BAD      │  Full remaining window (no scaling)             │
  │ OK       │  Full remaining window (this is the base case)  │
  │ GOOD     │  Remaining window shortened                     │
  │ PERFECT  │  Window snaps closed quickly                    │
  └──────────┴─────────────────────────────────────────────────┘

  ← time ────────────────────────────────────────────── →

  BAD / OK (base — full window plays out):
  ──╱▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲★▲▲▲▲▲▲▲▲▲▲▲▲▲▲╲────⬡⬡⬡───
                     │              │
                  collision    full window

  GOOD (scaled shorter):
  ──╱▲▲▲▲▲▲▲▲▲▲▲★▲▲▲▲▲▲▲▲╲────⬡⬡⬡──────────
                 │        │
              collision  shorter tail

  PERFECT (snaps shut):
  ──╱▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲★▲▲▲╲────⬡⬡⬡───────────
                     │   │
                  collision  snaps back

  ┌──────────────────────────────────────────────────────────────┐
  │  WHY THIS MATTERS:                                           │
  │                                                              │
  │  1. REWARDS PRECISION: Better timing = faster recovery       │
  │     = more prep time for the next obstacle.                  │
  │                                                              │
  │  2. SKILL CEILING: Expert players chain PERFECTs and get     │
  │     MORE hexagon rest between obstacles — room to breathe.   │
  │                                                              │
  │  3. NATURAL FEEL: You nailed it? Shape snaps back.           │
  │     You barely got it? You linger in the shape longer.       │
  │     Feels right intuitively.                                 │
  └──────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 5 — SCORING: TIMING × BURNOUT
# ═══════════════════════════════════════════════════

## Two Axes, One Moment

```
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  TIMING answers:  "HOW PRECISELY did you perform the beat?"  │
  │  BURNOUT answers: "HOW LATE did you wait to act?"            │
  │                                                              │
  │  The beautiful convergence:                                  │
  │                                                              │
  │  If the player presses so the window peaks ON the beat,      │
  │  and the obstacle arrives ON the beat...                      │
  │  then the obstacle is at its CLOSEST (max burnout)           │
  │  AND the timing is PERFECT (max grade).                      │
  │                                                              │
  │  PEAK TIMING = PEAK BURNOUT = THE BEAT                       │
  │  They are the SAME MOMENT.                                   │
  │                                                              │
  │  This is not a coincidence. This is the design.              │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

## Timing Grades

```
  When an obstacle collides with the player during an Active window,
  grade is determined by how close to the PEAK the collision occurs:

           0%    25%    50%    75%   100%
           │      │      │      │      │
  PERFECT  ├──────┤      │      │      │    ×1.50
           │ ★★★  │      │      │      │
  GOOD     │      ├──────┤      │      │    ×1.00
           │      │  ✦✦  │      │      │
  OK       │      │      ├──────┤      │    ×0.50
           │      │      │  ✧   │      │
  BAD      │      │      │      ├──────┤    ×0.25
           │      │      │      │  ·   │
           ▼      ▼      ▼      ▼      ▼
          PEAK                       EDGE

  pct_from_peak = |collision_time - peak_time| / half_window

  ┌──────────┬─────────────────┬───────────────┐
  │  GRADE   │  % FROM PEAK    │  TIMING MULT  │
  ├──────────┼─────────────────┼───────────────┤
  │ PERFECT  │   0% — 25%      │  ×1.50        │
  │ GOOD     │  25% — 50%      │  ×1.00        │
  │ OK       │  50% — 75%      │  ×0.50        │
  │ BAD      │  75% — 100%     │  ×0.25        │
  │ MISS     │  outside window  │  💀 collision  │
  └──────────┴─────────────────┴───────────────┘

  NOTE: GOOD is now ×1.00 (was ×0.75 in shape-window.md).
  Rationale: GOOD should be "neutral" — no bonus, no penalty.
  PERFECT is a BONUS. OK/BAD are PENALTIES.
```

## Burnout Zones (Unchanged from SPEC 2)

```
  0%                     40%       70%     95% 100%
  ├───── SAFE ────────────┼── RISKY ─┼─ DANGER ┼─☠─┤
  │ ░░░░░░░░░░░░░░░░░░░░ │ ▓▓▓▓▓▓▓▓ │ ████████│☠☠ │
  │        ×1.0           │   ×1.5   │×2→×3→×5│DIE│
  └───────────────────────┴──────────┴─────────┴───┘

  Burnout is driven by obstacle proximity, same as before.
  The ONLY change: obstacles arrive on beats, so the burnout
  meter pulses WITH the music. It fills ON beat.
```

## Combined Scoring Matrix

```
  final_score = floor(base_pts × timing_mult × burnout_mult)

  base_pts = 200 (ShapeGate example)

  ┌──────────┬────────┬────────┬────────┬─────────┐
  │          │ SAFE   │ RISKY  │ DANGER │ CLUTCH  │
  │          │ ×1.0   │ ×1.5   │ ×3.0   │ ×5.0    │
  ├──────────┼────────┼────────┼────────┼─────────┤
  │ BAD ×0.25│   50   │   75   │  150   │   250   │
  │ OK  ×0.50│  100   │  150   │  300   │   500   │
  │ GOOD×1.00│  200   │  300   │  600   │  1000   │
  │ PERF×1.50│  300   │  450   │  900   │  1500   │
  └──────────┴────────┴────────┴────────┴─────────┘

  Score range per obstacle: 50 → 1500 (30× spread)
```

## Per-Song Grading (End of Song)

```
  Because songs are finite levels, we add a per-song grade screen:

  ┌───────────────────────────────────────────┐
  │  TOTAL SCORE            18,420            │
  │                                           │
  │  ★ PERFECT              47                │
  │  ✦ GOOD                 12                │
  │  ✧ OK                    3                │
  │  · BAD                   1                │
  │  💀 MISS                  0                │
  │                                           │
  │  MAX CHAIN              23                │
  │  BEST BURNOUT           ×4.8              │
  │                                           │
  │  ┌─────────────────────────────────┐      │
  │  │           RANK: S              │      │
  │  └─────────────────────────────────┘      │
  │                                           │
  │  S = 100% clear + avg grade > GOOD        │
  │  A = 100% clear + avg grade > OK          │
  │  B = 100% clear                           │
  │  C = ≥90% clear                           │
  │  D = ≥70% clear                           │
  │  F = <70% clear                           │
  └───────────────────────────────────────────┘

  "Clear" = obstacle passed without 💀.
  100% clear means no MISSes.
  Song ends at duration_sec — no game over from time.
  MISSes reduce HP but don't immediately end the song
  (see `rhythm-spec.md` Edge Cases for HP system).
```

## Chain Bonuses (Unchanged)

```
  Chain = consecutive clears without a MISS.
  Bonus is flat additive, AFTER timing × burnout:

  ┌──────────┬──────────┐
  │  CHAIN   │  BONUS   │
  ├──────────┼──────────┤
  │  2       │  +50     │
  │  3       │  +100    │
  │  4       │  +200    │
  │  5+      │  +100/ea │
  └──────────┴──────────┘

  Chain resets on MISS.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 6 — HEXAGON: THE REST BETWEEN BEATS
# ═══════════════════════════════════════════════════

## Musical Analogy

```
  In sheet music:

  ♩  ♩  𝄾  ♩  ♩  𝄾  ♩  ♩  ♩  𝄾
  C  E  -  G  C  -  E  G  C  -
         rest       rest          rest

  In Shapeshifter:

  ▲  ●  ⬡  ■  ▲  ⬡  ●  ■  ▲  ⬡
  △  ○  -  □  △  -  ○  □  △  -
  act act rest act act rest act act act rest

  The hexagon ⬡ IS the rest note.
  It's not silence — it's the breath between performances.
  Without it, there is no rhythm. Just constant noise.
```

## The Pulse

```
  ──⬡──╱▲╲──⬡──╱●╲──⬡──╱■╲──⬡──╱▲╲──⬡──
    ↑    ↑    ↑    ↑    ↑    ↑    ↑
   rest beat rest beat rest beat rest

  INHALE ─── EXHALE ─── INHALE ─── EXHALE

  This breathing pattern IS the game's rhythm.
  The music's BPM controls how fast you breathe.

  80 BPM  = relaxed meditation
  120 BPM = energetic performance
  200 BPM = hyperventilation (insane difficulty)
```

## Design Rules (Unchanged from shape-window.md)

```
  ┌───────────────────────────────────────────────────────────────┐
  │  1. Hexagon NEVER matches any ShapeGate                      │
  │     → ⬡ ≠ ▲/●/■. Always collision.                          │
  │                                                               │
  │  2. Hexagon DOES interact with non-shape obstacles normally  │
  │     → LaneBlocks: dodge by lane ✓                            │
  │     → LowBars: jump ✓                                        │
  │     → HighBars: slide ✓                                      │
  │                                                               │
  │  3. No "HexagonGates" — hex is REST, not a playable shape    │
  │                                                               │
  │  4. Hexagon = READY INDICATOR                                │
  │     → See ⬡ = "no window active, you can press"             │
  │     → See ▲/●/■ = "you're committed, window is live"        │
  │                                                               │
  │  5. Hexagon has a subtle PULSE synced to BPM                 │
  │     → The idle hex glows on each beat even during rests      │
  │     → Visual metronome: player always knows where the beat is│
  └───────────────────────────────────────────────────────────────┘
```

## Collision Table

```
  ┌──────────────┬────┬────┬────┬────┐
  │ OBSTACLE     │  ⬡ │  ● │  ▲ │  ■ │
  ├──────────────┼────┼────┼────┼────┤
  │ ● Gate       │ 💀 │ ✓  │ 💀 │ 💀 │
  │ ▲ Gate       │ 💀 │ 💀 │ ✓  │ 💀 │
  │ ■ Gate       │ 💀 │ 💀 │ 💀 │ ✓  │
  │ LaneBlock    │ ✓* │ ✓* │ ✓* │ ✓* │  *if in clear lane
  │ LowBar       │ ✓* │ ✓* │ ✓* │ ✓* │  *if jumping
  │ HighBar      │ ✓* │ ✓* │ ✓* │ ✓* │  *if sliding
  │ ComboGate    │ 💀 │ ?  │ ?  │ ?  │  shape+lane req'd
  │ SplitPath    │ 💀 │ ?  │ ?  │ ?  │  shape+lane req'd
  └──────────────┴────┴────┴────┴────┘

  ⬡ is universally BLOCKED by shape-requiring obstacles.
  ⬡ is fine for pure-lane and pure-vertical obstacles.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 7 — BEAT MAP GENERATION
# ═══════════════════════════════════════════════════

## Audio Analysis Pipeline (aubio)

```
  Beat maps are GENERATED from audio, not authored from scratch.
  The aubio library analyzes the song file and produces the raw data.
  A designer then tunes the output.

  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  SONG FILE (.ogg/.wav)                                           │
  │       │                                                          │
  │       ├─── aubio tempo ──────→ BPM (fixed for entire song)      │
  │       │                                                          │
  │       ├─── aubio onset ──────→ onset timestamps (seconds)       │
  │       │    (when sounds start)                                   │
  │       │                                                          │
  │       ├─── aubio melbands ───→ frequency energy per band         │
  │       │    (what kind of sound at each onset)                    │
  │       │                                                          │
  │       └─── aubio mfcc ───────→ spectral shape coefficients      │
  │            (refine classification if needed)                     │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────────────────┐
  │  WHAT EACH TOOL GIVES US:                                        │
  │                                                                  │
  │  tempo    → BPM (one number, fixed for the song)                │
  │  onset    → list of timestamps where sounds begin               │
  │             these are the SPAWN POINTS for obstacles             │
  │  melbands → energy in low/mid/high frequency bands              │
  │             this tells us WHAT KIND of sound each onset is       │
  │  mfcc     → spectral fingerprint to REFINE type classification  │
  │                                                                  │
  │  pitch    → fundamental frequency (only useful if we ever       │
  │             add melodic lanes — skip for now)                    │
  │  notes    → MIDI-like note events (overkill — skip)             │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

## Onset → Obstacle Type Classification

```
  The core pipeline:

  ┌────────────┐    ┌───────────────┐    ┌──────────────┐    ┌──────────┐
  │    mel     │    │    onset      │    │   classify   │    │  map to  │
  │ spectrogram│───→│  detection    │───→│  onset type  │───→│  game    │
  │            │    │  per band     │    │  by frequency│    │  lane    │
  └────────────┘    └───────────────┘    └──────────────┘    └──────────┘

  Frequency band → shape + lane mapping:

  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  LOW  BAND (kick drum, bass)                                     │
  │  ─────────────────────────                                       │
  │       ● circle — heavy, grounded                                │
  │       Spawns in bottom or center lane                            │
  │       Feels like the "thump" of the music                       │
  │                                                                  │
  │  MID  BAND (snare, clap, vocal hits)                             │
  │  ───────────────────────────────────                              │
  │       ■ square — sharp, punchy                                  │
  │       Spawns in center lane                                      │
  │       Feels like the "snap" of the music                        │
  │                                                                  │
  │  HIGH BAND (hi-hat, cymbal, synth)                               │
  │  ─────────────────────────────────                                │
  │       ▲ triangle — bright, airy                                 │
  │       Spawns in top or side lanes                                │
  │       Feels like the "shimmer" of the music                     │
  │                                                                  │
  │  This creates a natural synesthesia:                              │
  │  you HEAR the kick and SEE the circle.                           │
  │  The obstacle type IS the sound.                                 │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

## Generation → Tuning Workflow

```
  ┌──────────────────────────────────────────────────────────────────┐
  │                                                                  │
  │  STEP 1: Run audio analysis                                      │
  │  ──────────────────────────                                      │
  │  • Feed song file through aubio pipeline                         │
  │  • Get BPM, onset times, mel band energies                       │
  │  • Auto-classify each onset by dominant frequency band           │
  │                                                                  │
  │  STEP 2: Auto-generate raw beat map                              │
  │  ─────────────────────────────────                                │
  │  • Each onset → obstacle entry with timestamp, shape, lane       │
  │  • Filter out onsets that are too close together                  │
  │    (minimum gap = 3 beats for different shapes)                  │
  │  • Filter out very quiet onsets (below energy threshold)         │
  │                                                                  │
  │  STEP 3: Designer tuning pass                                    │
  │  ────────────────────────────                                     │
  │  • Review auto-generated map                                     │
  │  • Remove obstacles that feel wrong or unfair                    │
  │  • Add lane blocks, low bars, and non-shape obstacles            │
  │  • Shape the intensity curve: build-up → climax → rest          │
  │  • Adjust obstacle density per section                           │
  │  • Playtest and iterate                                          │
  │                                                                  │
  │  STEP 4: Validate                                                │
  │  ─────────────────                                                │
  │  • No two different-shape gates too close together               │
  │  • No onset times beyond song duration                           │
  │  • All required fields present                                   │
  │  • Playtest: obstacles should feel synced to the music           │
  │                                                                  │
  └──────────────────────────────────────────────────────────────────┘

  The auto-generation gives you 80% of the work.
  The designer pass is where it goes from "correct" to "fun."
```

## Charting Patterns (Templates)

```
  PATTERN 1: Quarter-Note Basic (Easy)
  ────────────────────────────────────
  Every 4th beat has a shape gate. Lots of breathing room.

  Beat:  1   2   3   4   5   6   7   8   9  10  11  12
         .   .   .   ●   .   .   .   ▲   .   .   .   ■

  PATTERN 2: Half-Note Groove (Medium)
  ────────────────────────────────────
  Every 2nd beat. Steady rhythm, player anticipates.

  Beat:  1   2   3   4   5   6   7   8   9  10  11  12
         .   ●   .   ▲   .   ■   .   ●   .   ▲   .   ■

  PATTERN 3: Mixed Actions (Medium-Hard)
  ──────────────────────────────────────
  Shape gates + lane blocks + vertical actions.

  Beat:  1   2   3   4   5   6   7   8   9  10  11  12
         .   ●   .   ╫   .   ▲   .   ═   .   ■   .   ╫
              │       │       │       │       │       │
           shape   lane    shape   lowbar  shape   lane
            gate   block   gate            gate   block

  PATTERN 4: Double-Time Rush (Hard)
  ──────────────────────────────────
  Same-shape pairs on consecutive beats.

  Beat:  1   2   3   4   5   6   7   8
         ●   ●   .   ▲   ▲   .   ■   ■

  PATTERN 5: Piano Keys (Insane)
  ──────────────────────────────
  Different shapes with minimum 3-beat gap enforced.

  Beat:  1   2   3   4   5   6   7   8   9  10  11  12
         ▲   .   .   ●   .   .   ■   .   .   ▲   .   .

  Can overlap with lane/vertical for maximum density.
```

## Example: Full Tutorial Song Beat Map

```json
{
  "song_id": "tutorial_groove",
  "title": "Tutorial Groove",
  "artist": "Shapeshifter OST",
  "bpm": 80,
  "offset": 0.400,
  "lead_beats": 4,
  "difficulty": "easy",
  "duration_sec": 60.0,

  "beats": [
    // ── Section 1: Circle only (beats 4-16) ──
    { "beat": 4,  "kind": "shape_gate", "shape": "circle",   "lane": 1 },
    { "beat": 8,  "kind": "shape_gate", "shape": "circle",   "lane": 1 },
    { "beat": 12, "kind": "shape_gate", "shape": "circle",   "lane": 1 },
    { "beat": 16, "kind": "shape_gate", "shape": "circle",   "lane": 1 },

    // ── Section 2: Add square (beats 20-32) ──
    { "beat": 20, "kind": "shape_gate", "shape": "square",   "lane": 1 },
    { "beat": 24, "kind": "shape_gate", "shape": "circle",   "lane": 1 },
    { "beat": 28, "kind": "shape_gate", "shape": "square",   "lane": 1 },
    { "beat": 32, "kind": "shape_gate", "shape": "circle",   "lane": 1 },

    // ── Section 3: Add triangle (beats 36-48) ──
    { "beat": 36, "kind": "shape_gate", "shape": "triangle", "lane": 1 },
    { "beat": 40, "kind": "shape_gate", "shape": "square",   "lane": 1 },
    { "beat": 44, "kind": "shape_gate", "shape": "triangle", "lane": 1 },
    { "beat": 48, "kind": "shape_gate", "shape": "circle",   "lane": 1 },

    // ── Section 4: Add lane movement (beats 52-64) ──
    { "beat": 52, "kind": "shape_gate", "shape": "circle",   "lane": 0 },
    { "beat": 56, "kind": "shape_gate", "shape": "triangle", "lane": 2 },
    { "beat": 58, "kind": "lane_block", "blocked": [0, 1]              },
    { "beat": 60, "kind": "shape_gate", "shape": "square",   "lane": 1 },
    { "beat": 64, "kind": "shape_gate", "shape": "circle",   "lane": 1 }
  ]
}
```

## Beat Map Validation Rules

```
  ┌─────────────────────────────────────────────────────────────┐
  │  VALIDATION RULES (checked at song load):                   │
  │                                                             │
  │  1. beat_index values must be monotonically increasing      │
  │  2. No beat_index > floor(duration_sec / beat_period)       │
  │  3. shape_gate/combo_gate/split_path must have "shape"     │
  │  4. lane_block/combo_gate must have "blocked"              │
  │  5. shape_gate/split_path must have "lane" (0-2)           │
  │  6. Different-shape gates must be ≥ 3 beats apart          │
  │  7. BPM must be in range [60, 300]                         │
  │  8. offset must be in range [0.0, 5.0]                     │
  │  9. lead_beats must be in range [2, 8]                     │
  │ 10. At least 1 beat entry must exist                        │
  │                                                             │
  │  Validation failures are logged with beat index and reason. │
  │  Invalid beat maps are rejected at load time.               │
  └─────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 8 — ACCEPTANCE CRITERIA
# ═══════════════════════════════════════════════════

```
  RHYTHM ENGINE
  ─────────────
  [ ] Song audio plays via SDL_mixer (or equivalent)
  [ ] SongState.song_time advances in sync with audio
  [ ] SongState.current_beat increments on each beat boundary
  [ ] Song ends when song_time >= duration_sec → results screen
  [ ] Song fails when HPState.current <= 0 → fail screen

  BEAT MAP LOADING
  ────────────────
  [ ] beatmap.json parsed into BeatMap struct at song load
  [ ] BPM-derived constants computed once: beat_period,
      lead_time, scroll_speed, window_duration, morph_duration
  [ ] Validation rules enforced on load (see §7)
  [ ] Invalid beat maps logged and rejected

  BEAT-DRIVEN SPAWNING
  ────────────────────
  [ ] beat_scheduler_system spawns obstacles at spawn_time
  [ ] Obstacles arrive at PLAYER_Y at beat_time (within 1 frame)
  [ ] obstacle_spawn_system returns early when SongState.playing
  [ ] No random spawning during charted song play

  SHAPE WINDOW (carried forward from shape-window.md)
  ────────────
  [ ] Player starts as Hexagon
  [ ] Pressing shape button starts a timed window
  [ ] 4-phase lifecycle: IDLE → MORPH_IN → ACTIVE → MORPH_OUT
  [ ] Player reverts to Hexagon after window expires
  [ ] Hexagon collides with ALL shape gates (💀 → HP drain)
  [ ] Correct shape during Active phase passes (✓)
  [ ] Shape during MorphIn/MorphOut = MISS (💀)

  TIMING GRADES
  ─────────────
  [ ] TimingGrade computed from |collision_time - peak_time|
  [ ] PERFECT (0-25%): ×1.50
  [ ] GOOD (25-50%): ×1.00
  [ ] OK (50-75%): ×0.50
  [ ] BAD (75-100%): ×0.25
  [ ] MISS (outside window): 💀, HP drain
  [ ] Grade popup VFX appears at obstacle position

  COMBINED SCORING
  ────────────────
  [ ] final = floor(base_pts × timing_mult × burnout_mult)
  [ ] Chain bonus applied after timing × burnout
  [ ] SongResults accumulates grade counters
  [ ] Per-song grade (S/A/B/C/D/F) computed at song end

  HP SYSTEM
  ─────────
  [ ] Player starts with MAX_HP (5)
  [ ] MISS drains HP_DRAIN_ON_MISS (1)
  [ ] PERFECT restores HP_RECOVER_ON_PERFECT (1), capped at MAX_HP
  [ ] HP <= 0 → Song Fail
  [ ] HP bar visible in HUD

  HEXAGON
  ───────
  [ ] Hexagon rendered as 6-sided shape, neutral grey/white
  [ ] Hexagon pulses on each beat (synced to BPM)
  [ ] Hexagon is the ONLY shape between windows

  BPM-DERIVED CONSTANTS
  ─────────────────────
  [ ] scroll_speed = APPROACH_DIST / lead_time
  [ ] window_duration = max(BASE_WINDOW_BEATS * beat_period, MIN)
  [ ] morph_duration = max(BASE_MORPH_BEATS * beat_period, MIN)
  [ ] All scale correctly at 80, 120, 160, and 200 BPM

  WINDOW SCALING
  ──────────────
  [ ] Base window duration calibrated for OK grade
  [ ] GOOD grade: remaining window time scales down
  [ ] PERFECT grade: window snaps closed quickly
  [ ] BAD/OK: full window plays out (no scaling)

  EDGE CASES
  ──────────
  [ ] Same-shape consecutive gates: one window covers both
  [ ] Different-shape gates ≥ 3 beats apart (validated on load)
  [ ] Same-shape button spam during window is ignored
  [ ] Different-shape press mid-window interrupts (cancel + restart)
  [ ] Cooldown prevents immediate re-activation after window ends
  [ ] Last-frame collision resolves before window revert
  [ ] Audio desync: song_time slaved to audio engine position

  BACKWARDS COMPATIBILITY
  ──────────────────────
  [ ] obstacle_spawn_system returns early when SongState.playing
  [ ] Old random spawning still works when no beat map is loaded
  [ ] DifficultyConfig populated from SongState at song load
```

---
---
---

## Appendix A — Glossary

```
  ┌──────────────────┬────────────────────────────────────────────────┐
  │  TERM             │  DEFINITION                                    │
  ├──────────────────┼────────────────────────────────────────────────┤
  │  BPM              │  Beats Per Minute. Tempo of the song.          │
  │  Beat Period      │  Duration of one beat: 60.0 / BPM seconds.    │
  │  Lead Beats       │  How many beats ahead obstacles are visible.  │
  │  Lead Time        │  lead_beats × beat_period. Scroll duration.   │
  │  Approach Dist    │  PLAYER_Y - SPAWN_Y = 1040 pixels.           │
  │  Scroll Speed     │  approach_dist / lead_time. Pixels/second.   │
  │  Beat Time        │  When beat N occurs: offset + N * beat_period.│
  │  Spawn Time       │  beat_time - lead_time. When to create entity.│
  │  Window Duration  │  How long the Active shape phase lasts.       │
  │  Half Window      │  window_duration / 2. Used for grade calc.   │
  │  Peak Time        │  Center of the Active window. Ideal collision.│
  │  Timing Grade     │  PERFECT/GOOD/OK/BAD based on peak distance. │
  │  Burnout Mult     │  How close the obstacle was (risk/reward).   │
  │  Timing Mult      │  How on-beat the collision was (precision).  │
  │  HP               │  Hit Points. 5 max. Drains on MISS.          │
  │  Chart / Beat Map │  The JSON file defining when obstacles appear.│
  │  Clear Rate       │  % of obstacles passed without MISS.          │
  │  Song Grade       │  S/A/B/C/D/F based on clear rate + avg grade.│
  └──────────────────┴────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# APPENDIX B — USER STORIES
# ═══════════════════════════════════════════════════

```
  RHYTHM ENGINE
  ─────────────
  • As a player, I want music to play during gameplay so that
    obstacles feel synchronized to the beat.
  • As a player, I want the song to end after its full duration
    so I get a results screen with my grade.

  BEAT MAP / SPAWNING
  ───────────────────
  • As a player, I want obstacles to arrive on the beat so that
    I can anticipate them using the music.
  • As a level designer, I want to author beat maps in JSON so
    that I can chart obstacle patterns to any song.
  • As a level designer, I want validation errors on load so I
    catch charting mistakes early.

  SHAPE WINDOW
  ────────────
  • As a player, I want my character to be a hexagon by default
    so I know I'm in a "rest" state between actions.
  • As a player, I want shape changes to be temporary so I must
    time my press to line up with the obstacle's arrival.
  • As a player, I want to see a morph animation so I have
    visual feedback that my shape is transitioning.

  TIMING GRADES
  ─────────────
  • As a player, I want to see PERFECT/GOOD/OK/BAD popups so
    I know how precisely I timed my shape change.
  • As a player, I want PERFECT timing to give bonus points so
    I'm rewarded for precision.
  • As a player, I want timing and burnout to stack so that
    risk-taking AND precision are both rewarded.

  HP SYSTEM
  ─────────
  • As a player, I want HP instead of instant death so a single
    miss doesn't end a 3-minute song.
  • As a player, I want PERFECT hits to restore 1 HP so I can
    recover from mistakes through skilled play.
  • As a player, I want a "Song Failed" screen with partial
    results so I can see how far I got.

  SONG FLOW
  ─────────
  • As a player, I want a results screen at the end of a song
    showing my grade (S/A/B/C/D/F) and breakdown.
  • As a player, I want to retry a failed song immediately so
    I can practice without navigating menus.
```

---

*This document defines the game design for the rhythm-core mechanic.
For technical implementation details (ECS components, system pipeline, C++ constants),
see `rhythm-spec.md`.*
