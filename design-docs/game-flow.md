# SHAPESHIFTER — Game Flow & UX Specification
## v1.0 — The Definitive Player Experience Blueprint

> This document defines **every screen, every transition, every piece of feedback**
> the player experiences from first launch to 100th death.
> If it's on screen, it's in here. If the player feels it, it's in here.

> ⚠️ **PARTIAL SUPERSESSION (issue #239).**
> All references in this document to a **burnout meter**, burnout zones
> (Safe/Risky/Danger/Crit), burnout popups (×1.5 / ×3 / ×5 "CLUTCH" /
> "LEGENDARY"), burnout-zone heartbeat audio, and the "Burnout intro"
> tutorial run are **stale**. The burnout system has been removed from
> the game design.
>
> What replaces it on the HUD: the **proximity ring** around shape
> buttons (see `rhythm-spec.md` §6) is the live timing cue, and the
> **energy bar** (see `energy-bar.md`) is the survival meter. Scoring is
> driven by on-beat timing grades (Perfect/Good/Ok/Bad) × chain — not by
> a fill-the-meter risk/reward. On-beat shape changes are valid play
> even when no obstacle is arriving, so any tutorial/FTUE language that
> teaches "wait until the meter fills" should be ignored. See the
> rewritten Run 4 below for the current FTUE intent.

---

## TABLE OF CONTENTS

```
  ╔═══════════════════════════════════════════════════╗
  ║  1. MASTER SCREEN FLOW MAP                        ║
  ║  2. SCREEN SPECIFICATIONS (ASCII wireframes)      ║
  ║     2a. Title Screen                              ║
  ║     2b. Gameplay Screen (HUD formalized)          ║
  ║     2c. Pause Screen                              ║
  ║     2d. Game Over Screen                          ║
  ║     2e. Song Complete Screen                      ║
  ║  3. FIRST-TIME USER EXPERIENCE (FTUE)             ║
  ║     Tutorial Runs 1–5 with wireframes             ║
  ║  4. FEEDBACK & JUICE SPECIFICATION                ║
  ║     Every action → visual/audio/haptic response   ║
  ║  5. HUD STATE MACHINE                             ║
  ║     What's visible per game state                 ║
  ║  6. TRANSITION ANIMATIONS                         ║
  ║     Frame-by-frame screen transitions             ║
  ╚═══════════════════════════════════════════════════╝
```

---

## 1. MASTER SCREEN FLOW MAP

> **Source of truth:** `app/components/game_state.h` defines the
> `GamePhase` enum; `app/ui/screen_controllers/*` and
> `app/systems/game_state_system.cpp` define the actual transitions.
> The diagram below mirrors shipped routing as of Round 6 audit.

```
                          ┌─────────────────┐
                          │   APP LAUNCH     │
                          │  (splash/load)   │
                          └────────┬────────┘
                                   │
                          ┌────────▼────────┐
                     ┌───▶│  TITLE SCREEN   │◄────────────────────┐
                     │    │                 │                      │
                     │    │  ● logo         │                      │
                     │    │  ● tap to start │                      │
                     │    │  ● best score   │                      │
                     │    │  ● settings ⚙   │                      │
                     │    └──┬──────────┬───┘                      │
                     │       │          │                          │
                     │  [TAP BODY]  [TAP ⚙ ICON]                   │
                     │       │          │                          │
                     │       │          ▼                          │
                     │       │   ┌──────────────┐                  │
                     │       │   │  SETTINGS    │──[BACK]──────────┤
                     │       │   │  (overlay/   │                  │
                     │       │   │   screen)    │                  │
                     │       │   └──────────────┘                  │
                     │       │                                     │
                     │       ▼                                     │
                     │  ┌──────────────────┐                       │
                     │  │  LEVEL SELECT    │──[BACK]───────────────┤
                     │  │  (song + diff)   │                       │
                     │  └────────┬─────────┘                       │
                     │           │                                 │
                     │      [SELECT SONG]                          │
                     │           │                                 │
                     │           ▼                                 │
                     │     ┌──────────┐                            │
                     │     │ GAMEPLAY │◄────[RESUME]───┐           │
                     │     │  SCREEN  │                │           │
                     │     │ (Playing)│───[PAUSE btn]──┤           │
                     │     └────┬─────┘                │           │
                     │          │                ┌─────┴───────┐   │
                     │   ┌──────┴────────┐       │   PAUSED    │   │
                     │   │               │       │  (overlay)  │   │
                     │   │ on energy=0   │ song  │             │   │
                     │   │               │ ends  │ [RESUME]    │   │
                     │   ▼               ▼       │ [RESTART]───┼───┤
                     │ ┌──────────┐ ┌──────────┐ │ [QUIT] ─────┼───┘
                     │ │ GAMEOVER │ │   SONG   │ └─────────────┘
                     │ │  SCREEN  │ │ COMPLETE │
                     │ │          │ │  SCREEN  │
                     │ │ ● score  │ │          │
                     │ │ ● stats  │ │ ● score  │
                     │ │[RETRY]   │ │ ● stats  │
                     │ │[MENU]    │ │ [MENU]   │
                     │ └────┬─────┘ └────┬─────┘
                     │      │            │
                     └──────┴────────────┘
                          (back to Title)
```

> **Tutorial phase — defined but unreachable today.** `GamePhase::Tutorial`
> exists in `game_state.h` and is wired into render/state systems, but
> no screen controller currently issues `next_phase = GamePhase::Tutorial`
> (`grep -rnE "next_phase = GamePhase::Tutorial" app/` returns no hits).
> The "FTUE CHECK" branch shown in earlier revisions of this map is
> **not shipped**: `Settings::ftue_run_count` is persisted but never
> read by gameplay/UI systems. The wired-up FTUE/Tutorial flow is
> tracked in #76 (FTUE not implemented); reinstate the FTUE-check
> decision node only once a controller transitions into Tutorial.

### State Enumeration (for ECS implementation)

> **Source of truth:** `app/components/game_state.h:6-15`. If this list
> drifts from the header, the header wins.

```
  enum class GamePhase : uint8_t {
      Title        = 0,   // main menu
      LevelSelect  = 1,   // song/difficulty selection
      Playing      = 2,   // core gameplay
      Paused       = 3,   // overlay on gameplay
      GameOver     = 4,   // results screen
      SongComplete = 5,   // song finished successfully
      Settings     = 6,   // settings screen (entered from Title gear)
      Tutorial     = 7    // FTUE/tutorial run — defined but
                          // currently unreachable; no controller
                          // issues a transition into it (see #76)
  };
```

Shipped transitions, by controller, as of Round 6 audit:

- `Title → LevelSelect` — body tap (`title_screen_controller.cpp:62-72`).
- `Title → Settings` — gear-icon tap (same controller).
- `Settings → Title` — back action (`ui_render_system.cpp:117-122`).
- `LevelSelect → Playing` — song selection.
- `Playing ↔ Paused` — pause button / resume.
- `Paused → Title` — quit action.
- `Playing → GameOver` — energy reaches zero.
- `Playing → SongComplete` — song reaches end.
- `GameOver | SongComplete → Title` — menu action.
- `* → Tutorial` — **none today** (see #76).

---

## 2. SCREEN SPECIFICATIONS

All layouts use **proportional positioning** based on screen height (H)
and screen width (W) for portrait mode (9:16 aspect ratio target).

```
  POSITIONING REFERENCE:
  ╔══════════════════════════════╗  ─── y = 0.00 H
  ║                              ║
  ║  ┌── SAFE ZONE ───────────┐  ║  ─── y = 0.02 H (status bar inset)
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  │                        │  ║
  ║  └────────────────────────┘  ║
  ║                              ║
  ╚══════════════════════════════╝  ─── y = 1.00 H

  HORIZONTAL:
  ║← 0.05W →║←── content ──→║← 0.05W →║
               (0.90W wide)
```

---

### 2a-bis. LEVEL SELECT SCREEN

After tapping "start" on the title screen, the player is taken to the
**LevelSelect** screen (`GamePhase::LevelSelect`). This screen presents
a list of available songs and difficulty options. The layout is defined in
`content/ui/screens/level_select.json`. Confirming a selection transitions
to `GamePhase::Playing`.

---

### 2a. TITLE SCREEN

```
  ╔══════════════════════════════════════╗
  ║                                      ║  ← y = 0.00
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║  ← y = 0.15
  ║         ╔═══════════════╗            ║
  ║         ║               ║            ║
  ║         ║  S H A P E    ║            ║  ← y = 0.20  LOGO
  ║         ║  SHIFTER      ║            ║     centered X
  ║         ║               ║            ║     0.60W wide
  ║         ╚═══════════════╝            ║
  ║                                      ║  ← y = 0.30
  ║          ●    ■    ▲                 ║  ← y = 0.33  SHAPE TRIAD
  ║         idle animation               ║     orbiting/morphing
  ║         (shapes morph                ║     centered X
  ║          in slow loop)               ║
  ║                                      ║
  ║                                      ║  ← y = 0.45
  ║                                      ║
  ║       ╭─────────────────────╮        ║  ← y = 0.52
  ║       │   ▸ TAP TO START    │        ║     PULSE OPACITY
  ║       ╰─────────────────────╯        ║     sin(t) → 0.4–1.0
  ║                                      ║     centered X
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║  ← y = 0.75
  ║       ┌─────────────────────┐        ║
  ║       │  ◆ BEST: 24,850    │        ║  ← y = 0.78
  ║       └─────────────────────┘        ║     centered X
  ║                                      ║     only shows if > 0
  ║                                      ║
  ║                                      ║  ← y = 0.90
  ║                           ⚙         ║  ← SETTINGS GEAR
  ║                                      ║     x = 0.90W
  ║                                      ║     y = 0.92H
  ╚══════════════════════════════════════╝     tap target: 48×48 dp
```

#### Title Screen Animation Detail

```
  SHAPE TRIAD — idle morphing loop (3 seconds per cycle):

  t=0.0s        t=0.5s        t=1.0s        t=1.5s
  ● · ·         · ● ·         · · ●         · · ·
  · ■ ·         · · ■         ■ · ·         · ■ ·
  · · ▲         ▲ · ·         · ▲ ·         · · ▲

  Each shape orbits in a slow triangle path.
  Subtle trail effect behind each shape.
  Colors pulse gently (not distracting).

  "TAP TO START" — breathing animation:
  Frame 0 (t=0.0s):  opacity 40%   ░░░ TAP TO START ░░░
  Frame 1 (t=0.5s):  opacity 70%   ▒▒▒ TAP TO START ▒▒▒
  Frame 2 (t=1.0s):  opacity 100%  ███ TAP TO START ███
  Frame 3 (t=1.5s):  opacity 70%   ▒▒▒ TAP TO START ▒▒▒
  Frame 4 (t=2.0s):  opacity 40%   ░░░ TAP TO START ░░░
  ...repeat
```

#### Settings Panel (slides up from bottom on gear tap)

```
  ╔══════════════════════════════════════╗
  ║   (title content, dimmed 50%)        ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║┌────────────────────────────────────┐║  ← y = 0.55
  ║│            SETTINGS                │║
  ║│                                    │║
  ║│   Audio Offset                     │║
  ║│        [ − ]  +12 ms  [ + ]        │║
  ║│                                    │║
  ║│   [   HAPTICS: ON   ]              │║
  ║│                                    │║
  ║│   [   MOTION:  OFF  ]              │║
  ║│                                    │║
  ║│          [  BACK  ]                │║
  ║│                                    │║
  ║└────────────────────────────────────┘║
  ╚══════════════════════════════════════╝
```

Shipped controls (source: `content/ui/screens/settings.rgl`):

- **Audio Offset** — ± nudger (`AudioOffsetMinus` / `AudioOffsetDisplay` / `AudioOffsetPlus`); display shows current offset in ms, bound at render time.
- **Haptics toggle** — single button labeled `HAPTICS: ON` / `HAPTICS: OFF` (state dynamic via `HapticsValue`).
- **Reduce Motion toggle** — single button labeled `MOTION: ON` / `MOTION: OFF` (state dynamic via `ReduceMotionValue`).
- **BACK** — close button (label is literally `BACK`, not `CLOSE`).

Not shipped (intentionally absent from this wireframe):

- **Sound / Music volume sliders** — no volume controls exist in `settings.rgl`.
- **Reset Tutorial [ RESET ]** — `ftue_run_count` / `mark_ftue_complete` plumbing exists in `app/util/settings.h` and `app/util/settings_persistence.h`, but no UI surface ships. If this is still desired, file a separate feature issue rather than implying it ships here.

---

### 2b. GAMEPLAY SCREEN — Formalized HUD Layout

```
  ╔══════════════════════════════════════╗
  ║ ┌──────────────────────────────────┐ ║ ← y = 0.02 H
  ║ │ SCORE: 04,720   ⏸  ◆BEST:12.3K │ ║    SCORE BAR
  ║ └──────────────────────────────────┘ ║    h = 0.04 H
  ║ ┌──────────────────────────────────┐ ║ ← y = 0.06 H
  ║ │ SPD ████████░░░░░░░░░░░░ ×1.8   │ ║    SPEED BAR
  ║ └──────────────────────────────────┘ ║    h = 0.03 H
  ║                                      ║ ← y = 0.10 H
  ║                                      ║
  ║ ┌──────────┬──────────┬──────────┐   ║
  ║ │          │          │          │   ║
  ║ │  LANE 1  │  LANE 2  │  LANE 3  │   ║    GAMEPLAY
  ║ │  (left)  │  (center)│  (right) │   ║    AREA
  ║ │          │          │          │   ║    y: 0.10–0.78 H
  ║ │          │          │          │   ║    = 68% of screen
  ║ │          │          │          │   ║
  ║ │          │          │          │   ║    Each lane:
  ║ │          │    ●     │          │   ║    0.30W wide
  ║ │          │  (you)   │          │   ║    0.05W gutter
  ║ └──────────┴──────────┴──────────┘   ║
  ║                                      ║ ← y = 0.78 H
  ║ ┌──────────────────────────────────┐ ║ ← y = 0.79 H
  ║ │ ENERGY ████████████████░░░░░░░░  │ ║    ENERGY
  ║ │ (proximity ring around buttons)  │ ║    BAR
  ║ └──────────────────────────────────┘ ║    h = 0.05 H
  ║ ┌──────────────────────────────────┐ ║ ← y = 0.85 H
  ║ │                                  │ ║
  ║ │   ┌──────┐ ┌──────┐ ┌──────┐    │ ║    SHAPE
  ║ │   │  ●   │ │ ■■■  │ │  ▲   │    │ ║    BUTTONS
  ║ │   │      │ │active │ │      │    │ ║    h = 0.13 H
  ║ │   └──────┘ └──────┘ └──────┘    │ ║
  ║ │                                  │ ║    Each button:
  ║ └──────────────────────────────────┘ ║    0.25W × 0.10H
  ╚══════════════════════════════════════╝ ← y = 1.00 H
```

#### Exact Proportional Layout Table

```
  ╔═══════════════════════╦══════════════════╦═══════════════╗
  ║  ELEMENT              ║  POSITION        ║  SIZE         ║
  ╠═══════════════════════╬══════════════════╬═══════════════╣
  ║  Score bar            ║  x:0.05W y:0.02H ║  0.90W × 0.04H║
  ║  ├─ Score text        ║  left-aligned    ║  font: 0.03H  ║
  ║  ├─ Pause icon (⏸)    ║  x:0.50W center  ║  0.04H × 0.04H║
  ║  └─ Best score text   ║  right-aligned   ║  font: 0.02H  ║
  ╠═══════════════════════╬══════════════════╬═══════════════╣
  ║  Speed bar            ║  x:0.05W y:0.06H ║  0.90W × 0.03H║
  ║  ├─ "SPD" label       ║  left-aligned    ║  font: 0.02H  ║
  ║  ├─ Progress fill     ║  x:0.15W         ║  0.60W × 0.02H║
  ║  └─ Multiplier text   ║  right-aligned   ║  font: 0.02H  ║
  ╠═══════════════════════╬══════════════════╬═══════════════╣
  ║  Gameplay area        ║  x:0.05W y:0.10H ║  0.90W × 0.68H║
  ║  ├─ Lane 1 (left)     ║  x:0.05W         ║  0.28W         ║
  ║  ├─ Lane 2 (center)   ║  x:0.36W         ║  0.28W         ║
  ║  ├─ Lane 3 (right)    ║  x:0.67W         ║  0.28W         ║
  ║  ├─ Lane dividers     ║  x:0.34W, 0.65W  ║  0.01W (dotted)║
  ║  └─ Player position   ║  y:0.70H         ║  0.06H × 0.06H║
  ╠═══════════════════════╬══════════════════╬═══════════════╣
  ║  Energy bar           ║  x:0.05W y:0.79H ║  0.90W × 0.05H║
  ║  ├─ Track background  ║  full width      ║  rounded rect  ║
  ║  ├─ Fill bar          ║  left→right      ║  depletes on hit║
  ║  └─ (proximity ring)  ║  around buttons  ║  see rhythm-spec║
  ╠═══════════════════════╬══════════════════╬═══════════════╣
  ║  Shape buttons        ║  x:0.05W y:0.85H ║  0.90W × 0.13H║
  ║  ├─ ● button          ║  x:0.08W         ║  0.25W × 0.10H║
  ║  ├─ ■ button          ║  x:0.38W         ║  0.25W × 0.10H║
  ║  └─ ▲ button          ║  x:0.68W         ║  0.25W × 0.10H║
  ╚═══════════════════════╩══════════════════╩═══════════════╝

  NOTE: All tap targets ≥ 48dp minimum for mobile usability.
  Buttons have 0.03W horizontal gap between them.
```

#### Player Position in Lanes

```
  The player avatar sits at y = 0.70H (fixed vertical).
  Only horizontal position changes with lane swipes.

  LANE 1 (left):     player x = 0.19W  (center of lane 1)
  LANE 2 (center):   player x = 0.50W  (center of lane 2)
  LANE 3 (right):    player x = 0.81W  (center of lane 3)

  Lane switch animation: 0.12 seconds ease-out
  (snappy, responsive — never sluggish)

  ╔══════════════════════════════════════╗
  ║                                      ║
  ║  Lane1     Lane2      Lane3          ║
  ║   │         │          │             ║
  ║   │         │          │             ║
  ║   │         │          │             ║
  ║   ●─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─●   ║
  ║   ▲                            ▲     ║
  ║   x=0.19W                x=0.81W     ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

---

### 2c. PAUSE SCREEN (Overlay)

```
  ╔══════════════════════════════════════╗
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ← gameplay frozen
  ║▓▓▓▓  (gameplay screenshot, ▓▓▓▓▓▓▓▓▓║     behind 60%
  ║▓▓▓▓   blurred + dimmed    ▓▓▓▓▓▓▓▓▓║     black overlay
  ║▓▓▓▓   to 40% brightness) ▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║                                      ║
  ║                                      ║
  ║           ║  PAUSED  ║              ║  ← y = 0.30H
  ║                                      ║
  ║                                      ║
  ║        ╔══════════════════╗          ║  ← y = 0.42H
  ║        ║   ▸ RESUME       ║          ║     button: 0.50W × 0.08H
  ║        ╚══════════════════╝          ║
  ║                                      ║
  ║        ┌──────────────────┐          ║  ← y = 0.54H
  ║        │   ↺ RESTART      │          ║     button: 0.50W × 0.08H
  ║        └──────────────────┘          ║
  ║                                      ║
  ║        ┌──────────────────┐          ║  ← y = 0.66H
  ║        │   ✕ QUIT         │          ║     button: 0.50W × 0.08H
  ║        └──────────────────┘          ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  RESUME button = bright/highlighted (double-bordered)
  RESTART, QUIT = dim/secondary (single-bordered)

  Trigger: tap ⏸ icon at top-center during gameplay
  Resume also triggers on: tap anywhere outside buttons
```

---

### 2d. GAME OVER SCREEN

> **Trigger:** `Playing → GameOver` occurs only when energy reaches 0.
> A MISS drains energy and resets chain, but a miss is not itself a
> terminal transition.
>
> ⚠️ **PARTIAL ARCHIVED — issue #239.** The `Best Burnout` and
> `Avg Burnout` stat rows in the Game Over wireframe and the
> "Stats Breakdown" table below describe stats derived from the
> removed burnout-multiplier system. The rest of the screen
> (final score, NEW BEST badge, Distance / Time / Obstacles /
> Shapes Shifted / Longest Chain rows, PLAY AGAIN / MENU buttons,
> highlight rules for Longest Chain and the SURVIVOR badge) is
> still current. New Game Over code must not surface burnout
> stats; replacement scoring will come from energy-bar telemetry
> (`design-docs/energy-bar.md`) and rhythm timing (`design-docs/
> rhythm-spec.md` §6).

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║          G A M E   O V E R           ║  ← y = 0.08H
  ║                                      ║     font: 0.05H
  ║         ─ ─ ─ ─ ─ ─ ─ ─ ─          ║     color: white
  ║                                      ║
  ║     ┌────────────────────────────┐   ║  ← y = 0.18H
  ║     │                            │   ║
  ║     │         ★ 24,850 ★         │   ║  ← FINAL SCORE
  ║     │       (rolls up to this)   │   ║     font: 0.07H
  ║     │                            │   ║     gold color
  ║     │    ◆ NEW BEST! ◆           │   ║  ← only if new record
  ║     │     (prev: 18,200)         │   ║     rainbow pulse
  ║     │                            │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ║     ┌────────────────────────────┐   ║  ← y = 0.42H
  ║     │  STATS                     │   ║
  ║     │  ─────────────────────     │   ║
  ║     │                            │   ║
  ║     │  Distance      1,247 m     │   ║  ← stat rows
  ║     │  Time          02:34       │   ║     slide in
  ║     │  Obstacles     47          │   ║     sequentially
  ║     │  Shapes Shifted 31         │   ║     0.1s apart
  ║     │  Best Burnout   ×4.0       │   ║   ← ARCHIVED #239
  ║     │  Longest Chain  5          │   ║
  ║     │  Avg Burnout    ×2.3       │   ║   ← ARCHIVED #239
  ║     │                            │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║  ← y = 0.78H
  ║                                      ║
  ║     ╔════════════════════════════╗   ║  ← y = 0.82H
  ║     ║     ▸ PLAY AGAIN          ║   ║     PRIMARY button
  ║     ╚════════════════════════════╝   ║     0.80W × 0.08H
  ║                                      ║
  ║     ┌────────────────────────────┐   ║  ← y = 0.92H
  ║     │        MENU               │   ║     secondary button
  ║     └────────────────────────────┘   ║     0.80W × 0.05H
  ║                                      ║
  ╚══════════════════════════════════════╝
```

#### Stats Breakdown — What Each Row Shows

```
  ╔══════════════════════════════════════════════════════╗
  ║  STAT ROW           ║  SOURCE             ║ FORMAT  ║
  ╠══════════════════════╬══════════════════════╬═════════╣
  ║  Distance            ║  meters traveled     ║ "1,247m"║
  ║  Time                ║  seconds survived    ║ "02:34" ║
  ║  Obstacles           ║  total cleared       ║ "47"    ║
  ║  Shapes Shifted      ║  shape button taps   ║ "31"    ║
  ║  Best Burnout        ║  highest single mult ║ "×4.0"  ║   ← ARCHIVED #239
  ║  Longest Chain       ║  max consecutive     ║ "5"     ║
  ║  Avg Burnout         ║  mean of all mults   ║ "×2.3"  ║   ← ARCHIVED #239
  ╚══════════════════════╩══════════════════════╩═════════╝

  HIGHLIGHT RULES:
  ─────────────────
  • If Best Burnout ≥ ×4.0  → show in GOLD with ★ icon  ← ARCHIVED #239
  • If Longest Chain ≥ 5    → show in GOLD with ★ icon
  • If new personal best    → "◆ NEW BEST! ◆" rainbow text
  • If first time > 120s    → show "🔥 SURVIVOR" badge
```

---

### 2e. SONG COMPLETE SCREEN

> **Trigger:** `Playing → SongComplete` occurs when the song reaches its
> authored duration while the player still has energy remaining.

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║       S O N G   C O M P L E T E      ║  ← y = 0.08H
  ║                                      ║
  ║         ─ ─ ─ ─ ─ ─ ─ ─ ─          ║
  ║                                      ║
  ║     ┌────────────────────────────┐   ║
  ║     │                            │   ║
  ║     │         ★ 31,420 ★         │   ║  ← FINAL SCORE
  ║     │       (rolls up to this)   │   ║
  ║     │                            │   ║
  ║     │      CLEAR BONUS APPLIED   │   ║
  ║     │                            │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ║     ┌────────────────────────────┐   ║
  ║     │  RESULTS                   │   ║
  ║     │  ─────────────────────     │   ║
  ║     │  Accuracy      91%         │   ║
  ║     │  Misses        3           │   ║
  ║     │  Longest Chain 18          │   ║
  ║     │  Energy Left   42%         │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ║     ╔════════════════════════════╗   ║
  ║     ║     ▸ PLAY AGAIN          ║   ║
  ║     ╚════════════════════════════╝   ║
  ║                                      ║
  ║     ┌────────────────────────────┐   ║
  ║     │        MENU               │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

Terminal split:
- `energy <= 0` before the song ends → Game Over.
- Song duration reached with energy remaining → Song Complete.

---

## 3. FIRST-TIME USER EXPERIENCE (FTUE)

### Design Philosophy

```
  ╔═══════════════════════════════════════════════════════╗
  ║                                                       ║
  ║   WE TEACH THROUGH PLAY, NEVER THROUGH TEXT WALLS.    ║
  ║                                                       ║
  ║   ✗ BAD:  "Tap the circle button to become a circle"  ║
  ║   ✓ GOOD:  Only show ● gates. Only show ● button.     ║
  ║            Player figures it out in 2 seconds.         ║
  ║                                                       ║
  ║   RULE: If we need more than 6 words on screen,       ║
  ║         our design has failed.                         ║
  ║                                                       ║
  ╚═══════════════════════════════════════════════════════╝
```

### FTUE State Tracking

```
  Persistent storage key: "ftue_run_count" (int, starts at 0)

  Run 0:  Never played before → Tutorial Run 1
  Run 1:  Completed 1 run     → Tutorial Run 2
  Run 2:  Completed 2 runs    → Tutorial Run 3
  Run 3:  Completed 3 runs    → Tutorial Run 4
  Run 4:  Completed 4 runs    → Tutorial Run 5
  Run 5+: Full game unlocked  → No more tutorial
```

---

### TUTORIAL RUN 1 — "Match the Shape"

**Goal:** Teach shape matching. Only ■ gates appear. Only ■ button visible.

```
  WHAT'S DIFFERENT:
  ─────────────────
  • Only SQUARE gates spawn (no other obstacles)
  • Only [ ■ ] button visible (● and ▲ hidden)
  • Player starts as ● (wrong shape on purpose)
  • Speed is VERY slow (×0.6)
  • (Burnout meter hidden — ARCHIVED #239: meter removed; no energy bar in Run 1)
  • First gate doesn't kill — it bounces player back
  • Gentle arrow pointing at the ■ button pulses

  WHAT PLAYER LEARNS:
  ───────────────────
  "I need to be the same shape as the gate"
  "I tap the button to change shape"
```

#### Run 1 — Frame 1: First gate approaches

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║███┌──┐███║  ← ■ gate      ║
  ║         ║███│  │███║                 ║
  ║         ║███└──┘███║                 ║
  ║         ╚══════════╝                 ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              ●  ← you (circle)       ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                ↓                     ║  ← pulsing arrow
  ║           ╔══════════╗               ║     points at
  ║           ║    ■     ║               ║     the ONE button
  ║           ║  (tap!)  ║               ║
  ║           ╚══════════╝               ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

#### Run 1 — Frame 2: Player taps ■ — SUCCESS!

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║            ✨  ✓  ✨                  ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║███    ███║                 ║
  ║         ║███ ■→ ███║  THROUGH!       ║
  ║         ║███    ███║                 ║
  ║         ╚══════════╝                 ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              ■  ← now square!        ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║           ╔══════════╗               ║
  ║           ║   ■■■    ║  ← glowing!   ║
  ║           ║  active  ║               ║
  ║           ╚══════════╝               ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  The run continues with 4 more ■ gates,
  increasing speed slightly each time.
  After gate 5, auto-end → "GREAT START!" message.
```

#### Run 1 — Completion Screen

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║          ★ GREAT START! ★            ║
  ║                                      ║
  ║     You learned to shift shapes!     ║
  ║                                      ║
  ║           ● → ■ → ▲                 ║
  ║                                      ║
  ║     ╔════════════════════════════╗   ║
  ║     ║     ▸ NEXT LESSON         ║   ║
  ║     ╚════════════════════════════╝   ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

---

### TUTORIAL RUN 2 — "Two Shapes"

**Goal:** Teach switching between shapes. ● and ■ gates appear. Two buttons visible.

```
  WHAT'S DIFFERENT:
  ─────────────────
  • CIRCLE and SQUARE gates alternate
  • Two buttons visible: [ ● ] [ ■ ]
  • Speed: ×0.7
  • (Burnout meter still hidden — ARCHIVED #239: meter removed)
  • No other obstacle types

  WHAT PLAYER LEARNS:
  ───────────────────
  "I need to switch between shapes depending on what's coming"
```

#### Run 2 — Frame 1: Circle gate, player is ■

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║███╭──╮███║  ← ● gate      ║
  ║         ║███│  │███║                 ║
  ║         ║███╰──╯███║                 ║
  ║         ╚══════════╝                 ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              ■  ← you (wrong!)       ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║      ↓                               ║  ← arrow blinks
  ║   ┌──────┐    ┌──────┐              ║     at correct
  ║   │  ●   │    │ ■■■  │              ║     button
  ║   │ tap! │    │active│              ║
  ║   └──────┘    └──────┘              ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

#### Run 2 — Frame 2: Next obstacle is ■ gate — switch back!

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║███┌──┐███║  ← ■ gate!     ║
  ║         ║███│  │███║    switch back! ║
  ║         ║███└──┘███║                 ║
  ║         ╚══════════╝                 ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              ●  ← you (circle now)   ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                 ↓                    ║
  ║   ┌──────┐    ┌──────┐              ║
  ║   │ ●●●  │    │  ■   │              ║
  ║   │active│    │ tap! │              ║
  ║   └──────┘    └──────┘              ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  After 6 alternating gates, auto-end.
  Completion unlocks ▲ for all future runs.
```

---

### TUTORIAL RUN 3 — "Dodge!"

> ⚠️ **NOT SHIPPED — lane_push tutorial content (issues #328, #470).**
> The "Dodge!" run below teaches a `lane_push` obstacle that automatically
> shoves the player one lane on beat arrival. **No `lane_push` obstacle
> ships in the runtime today** — `LanePush` is queued for removal/rework
> (#328) and no shipped beatmap (`content/beatmaps/`) emits one. The
> shipped tutorial therefore graduates from Run 2 ("Two Shapes") directly
> to Run 4 ("Stay on the Beat"); shape_gate is the only obstacle the
> tutorial currently teaches. The frames below are retained as
> forward-design context for if/when a committed lane-push (or successor
> dodge mechanic) plan lands. Do not treat them as a description of
> current player-facing tutorial content.


```
  WHAT'S DIFFERENT:
  ─────────────────
  • All 3 shape buttons visible
  • Speed: ×0.8
  • (Burnout meter still hidden — ARCHIVED #239: meter removed)

  WHAT PLAYER LEARNS (future / not shipped):
  ──────────────────────────────────────────
  "Some obstacles push me to a different lane automatically"
```


```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║           ╔═══╗                      ║
  ║           ║ ▶ ║  ← PUSH RIGHT!      ║
  ║           ╚═══╝    (in your lane)    ║
  ║  ─────────┬─────────┬─────────      ║
  ║           │    :    │                ║
  ║           │    :    │                ║
  ║           │    :    │                ║
  ║           │    ●    │                ║
  ║           │  (you)  │                ║
  ║  ─────────┴─────────┴─────────      ║
  ║                                      ║
  ║        "You'll be pushed right!"     ║  ← hint text
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │  ●   │ │  ■   │ │  ▲   │        ║
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```


```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║  ─────────┬─────────┬─────────      ║
  ║           │  ╔═══╗  │               ║
  ║           │  ║ ▶ ║  │    ✨          ║
  ║           │  ╚═══╝  │     ●         ║
  ║           │         │   (pushed!)   ║
  ║  ─────────┴─────────┴─────────      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │  ●   │ │  ■   │ │  ▲   │        ║
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  Run continues with mix of gates + pushes.
  After 8 obstacles, auto-end.
```

---

### TUTORIAL RUN 4 — "Stay on the Beat"

**Goal:** Introduce on-beat timing as the scoring axis. Show that hitting the
shape closer to the beat gives a higher grade (Perfect > Good > Ok > Bad).

```
  WHAT'S DIFFERENT:
  ─────────────────
  • Score counter visible
  • Proximity ring around the active shape button NOW VISIBLE
  • Speed: ×0.8 (still gentle)
  • Timing-grade popups appear on every clear (PERFECT / GOOD / OK / BAD)
  • One-time hint text: "Hit on the beat!" (4 words, under the limit)

  WHAT PLAYER LEARNS:
  ───────────────────
  "The ring shrinks toward the button as the beat approaches"
  "Pressing when the ring is tight = PERFECT"
  "Earlier or later = lower grade"
  "Changing shape on the beat is fine even if no obstacle is here"
```

#### Run 4 — Frame 1: Proximity ring appears for the first time

```
  ╔══════════════════════════════════════╗
  ║  SCORE: 00,000                       ║
  ║                                      ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║███╭──╮███║  ← ● gate      ║
  ║         ╚══════════╝                 ║
  ║              :                       ║
  ║              :                       ║
  ║              :                       ║
  ║              ■  ← you               ║
  ║                                      ║
  ║         "Hit on the beat!"           ║  ← one-time hint
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │((●)) │ │ ■■■  │ │  ▲   │        ║  ← ring around target
  ║   └──────┘ └──────┘ └──────┘        ║     shape begins wide
  ║                                      ║
  ╚══════════════════════════════════════╝
```

#### Run 4 — Frame 2: Ring tightens as the beat approaches

```
  ╔══════════════════════════════════════╗
  ║  SCORE: 00,000                       ║
  ║                                      ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║███╭──╮███║  ← closer      ║
  ║         ╚══════════╝                 ║
  ║              :                       ║
  ║              ■  ← still on beat-2    ║
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │ (●)  │ │ ■■■  │ │  ▲   │        ║  ← ring tighter
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ║      Ring touching the button =       ║
  ║      press NOW for PERFECT.           ║
  ╚══════════════════════════════════════╝

  The ring is the live timing cue: tight = on the beat.
  No "fill the meter" — earlier presses are not penalised
  more than later ones.
```

#### Run 4 — Frame 3: Player presses on the beat — PERFECT!

```
  ╔══════════════════════════════════════╗
  ║  SCORE: 00,300  ★+300★   chain ×2   ║
  ║                                      ║
  ║          ✨ PERFECT! ✨               ║
  ║             +300 pts                  ║
  ║                                      ║
  ║                                      ║
  ║              ●  ← switched on beat   ║
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │ ●●●  │ │  ■   │ │  ▲   │        ║
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

---

### TUTORIAL RUN 5+ — "Full Game"

```
  WHAT'S DIFFERENT:
  ─────────────────
  • ALL systems active
  • Full HUD: score, energy, proximity rings, all buttons
  • Normal difficulty ramp
  • No more tutorial hints
  • This IS the real game now

  WHAT PLAYER HAS LEARNED:
  ────────────────────────
  Run 1: Shape matching (tap button to change)
  Run 2: Shape switching (read what's coming, react)
  Run 3: Lane dodging (swipe to avoid)
  Run 4: On-beat timing (Perfect/Good/Ok/Bad grades + chain)
  Run 5: Put it all together → PLAY
```

### Tutorial Flow Summary

> ⚠️ **PARTIAL ARCHIVED — issue #239.** The Run 4 row below ("Burnout intro
> / + burnout meter visible") and the corresponding `Burnout` step in the
> teaching-cadence diagram describe the removed burnout system. Run 4 has
> been rewritten as **"Stay on the Beat"** (on-beat timing with
> Perfect / Good / Ok / Bad grades — see the §3 Run 4 prose above and
> `design-docs/rhythm-spec.md` §6). Treat the burnout entries here as
> historical context only; do not implement.

```
  ╔══════════════════════════════════════════════════╗
  ║  RUN  ║ CONCEPT        ║ MECHANICS ACTIVE        ║
  ╠═══════╬════════════════╬═════════════════════════╣
  ║   1   ║ Match shape    ║ ■ gates, 1 button       ║
  ║   2   ║ Switch shapes  ║ ●■ gates, 2 buttons     ║
  ║   4   ║ Burnout intro  ║ + burnout meter visible  ║
  ║   5+  ║ FULL GAME      ║ Everything               ║
  ╚═══════╩════════════════╩═════════════════════════╝

  TEACHING CADENCE:
  ═════════════════

  Complexity
     ▲
     │                                    ┌─── Full game
     │                               ┌────┘
     │                          ┌────┘  Burnout
     │                     ┌────┘  Dodging
     │                ┌────┘  2 shapes
     │           ┌────┘  1 shape
     │      ┌────┘
     │ ┌────┘
     ┼─┘──────────────────────────────────────► Run #
       1     2      3       4       5+

  Each run adds ONE concept. Never two at once.
  Player builds skill incrementally.
```

---

## 4. FEEDBACK & JUICE SPECIFICATION

Every player action triggers a multi-sensory response.
"Juice" = visual + audio + haptic combined for maximum satisfaction.

```
  ╔═══════════════════════════════════════════════════════╗
  ║                                                       ║
  ║    JUICE PHILOSOPHY:                                  ║
  ║                                                       ║
  ║    Every input needs a response within 1 frame.       ║
  ║    The response should feel BIGGER than the input.    ║
  ║    Small action → disproportionately satisfying.      ║
  ║                                                       ║
  ║    If the player taps a button and NOTHING happens    ║
  ║    for even 50ms, the game feels BROKEN.              ║
  ║                                                       ║
  ╚═══════════════════════════════════════════════════════╝
```

---

### 4a. SHAPE SHIFT (Player taps shape button)

```
  TRIGGER: Player taps [ ● ], [ ■ ], or [ ▲ ] button
  DURATION: 0.15 seconds total

  ┌──────────────┬──────────────────────────────────────────┐
  │  CHANNEL     │  RESPONSE                                │
  ├──────────────┼──────────────────────────────────────────┤
  │  VISUAL      │  1. Button POPS (scale 1.0→1.2→1.0)     │
  │              │     duration: 0.15s, ease-out-bounce      │
  │              │                                          │
  │              │  2. Player avatar MORPHS:                │
  │              │     old shape dissolves into particles   │
  │              │     new shape assembles from particles   │
  │              │     duration: 0.12s                       │
  │              │                                          │
  │              │  3. Color flash on player:               │
  │              │     ● = cyan pulse                       │
  │              │     ■ = magenta pulse                    │
  │              │     ▲ = yellow pulse                     │
  │              │                                          │
  │              │  4. Previous button dims (opacity → 60%) │
  │              │     New button brightens (glow halo)     │
  │              │                                          │
  │  AUDIO       │  "pop" SFX — pitch varies per shape:    │
  │              │     ● = mid tone (440 Hz base)           │
  │              │     ■ = low tone (330 Hz base)           │
  │              │     ▲ = high tone (550 Hz base)          │
  │              │                                          │
  │  HAPTIC      │  Light tap (UIImpactFeedbackGenerator    │
  │              │  .light equivalent, ~10ms)               │
  └──────────────┴──────────────────────────────────────────┘

  MORPH ANIMATION SEQUENCE (6 frames at 60fps):
  ──────────────────────────────────────────────

  Frame 0       Frame 1       Frame 2       Frame 3
  (current)     (dissolve)    (particles)   (reform)

    ■             ▪▫            · ·           /\
   ┌──┐          ▫ ▪▫          ·  ·          /  \
   │  │           ▪ ▫          · · ·        /    \
   └──┘            ▫           ·  ·        /______\
  (square)      (breaking)   (cloud)     (triangle)

  Frame 4       Frame 5
  (settle)      (done)

    /\            ▲
   /  \          /  \       ← solid, with brief
  /____\        /____\        color pulse glow
  (forming)    (FINAL)
```

---

### 4b. BURNOUT BANK (Player clears obstacle with burnout multiplier) — ARCHIVED — issue #239

> ⚠️ **ARCHIVED.** The burnout multiplier system (×1.0 / ×1.5 / ×2.0 / ×3.0
> / ×4.0 / ×5.0 with "Nice / GREAT / CLUTCH / INSANE / LEGENDARY" popups)
> was removed from the game design. Current scoring is the timing-grade
> × chain model (Perfect / Good / Ok / Bad) defined in
> `design-docs/rhythm-spec.md` §6 and `design-docs/rhythm-design.md`.
> The popup, audio, haptic, and visual feedback table below is retained
> only as historical context and should NOT be implemented.

```
  TRIGGER: Obstacle cleared at any burnout multiplier
  RESPONSE SCALES WITH MULTIPLIER TIER:

  ╔═══════════╦══════════════════════════════════════════════╗
  ║ MULTIPLIER║  FEEDBACK RESPONSE                          ║
  ╠═══════════╬══════════════════════════════════════════════╣
  ║           ║                                              ║
  ║  ×1.0     ║  VISUAL:  Tiny "+200" text, white, fades    ║
  ║  (none)   ║  AUDIO:   Soft "ding"                       ║
  ║           ║  HAPTIC:  None                               ║
  ║           ║  POPUP:   (nothing)                          ║
  ║           ║                                              ║
  ║           ║    +200                                      ║
  ║           ║    (small, white, fades in 0.5s)             ║
  ║           ║                                              ║
  ╠═══════════╬══════════════════════════════════════════════╣
  ║           ║                                              ║
  ║  ×1.5     ║  VISUAL:  "+300" text, light green           ║
  ║  "Nice"   ║  AUDIO:   Bright "ding" + rising tone       ║
  ║           ║  HAPTIC:  Light tap                          ║
  ║           ║  POPUP:   "Nice" in small text               ║
  ║           ║                                              ║
  ║           ║    Nice                                      ║
  ║           ║     +300                                     ║
  ║           ║    (green, fades in 0.8s)                    ║
  ║           ║                                              ║
  ╠═══════════╬══════════════════════════════════════════════╣
  ║           ║                                              ║
  ║  ×2.0     ║  VISUAL:  "+400" text, YELLOW, medium size   ║
  ║  "GREAT!" ║           4 particles burst from obstacle    ║
  ║           ║  AUDIO:   Double-chime ascending             ║
  ║           ║  HAPTIC:  Medium tap                         ║
  ║           ║  POPUP:   "GREAT!" medium text, yellow       ║
  ║           ║                                              ║
  ║           ║     ✦ GREAT! ✦                               ║
  ║           ║       +400                                   ║
  ║           ║    (yellow, slight scale-up, 1.0s)           ║
  ║           ║                                              ║
  ╠═══════════╬══════════════════════════════════════════════╣
  ║           ║                                              ║
  ║  ×3.0     ║  VISUAL:  "+600" text, ORANGE, large         ║
  ║ "CLUTCH!" ║           8 particles, screen border flash   ║
  ║           ║           Camera micro-shake (2px, 0.1s)     ║
  ║           ║  AUDIO:   Triple ascending chime + bass hit  ║
  ║           ║  HAPTIC:  Medium impact                      ║
  ║           ║  POPUP:   "CLUTCH!" large text, orange       ║
  ║           ║                                              ║
  ║           ║   ✦  CLUTCH!  ✦                              ║
  ║           ║       +600                                   ║
  ║           ║   (orange, shake, border glow, 1.2s)         ║
  ║           ║                                              ║
  ╠═══════════╬══════════════════════════════════════════════╣
  ║           ║                                              ║
  ║  ×4.0     ║  VISUAL:  "+800" text, RED, huge             ║
  ║ "INSANE!" ║           16 particles, full screen flash    ║
  ║           ║           Camera shake (4px, 0.15s)          ║
  ║           ║           Speed lines appear at edges        ║
  ║           ║  AUDIO:   Power chord + crowd "OHH!"        ║
  ║           ║  HAPTIC:  Heavy impact                       ║
  ║           ║  POPUP:   "INSANE!!" huge, red, screen flash ║
  ║           ║                                              ║
  ║           ║  ✦    INSANE!!    ✦                          ║
  ║           ║  ✦      +800      ✦                          ║
  ║           ║  (red, screen flash, heavy shake, 1.5s)      ║
  ║           ║                                              ║
  ╠═══════════╬══════════════════════════════════════════════╣
  ║           ║                                              ║
  ║  ×5.0     ║  VISUAL:  "+1000" text, RAINBOW cycling      ║
  ║"LEGENDARY"║           32 particles, EXPLOSION effect     ║
  ║           ║           Full screen white flash (0.05s)    ║
  ║           ║           Camera shake (6px, 0.2s)           ║
  ║           ║           Time freeze (0.1s pause on impact) ║
  ║           ║           Screen border GOLD glow (2s fade)  ║
  ║           ║  AUDIO:   Epic brass stinger + explosion     ║
  ║           ║  HAPTIC:  Triple-pulse heavy                 ║
  ║           ║  POPUP:   "LEGENDARY!!!" massive, rainbow    ║
  ║           ║                                              ║
  ║           ║  ★✦★  LEGENDARY!!!  ★✦★                     ║
  ║           ║  ★✦★     +1000       ★✦★                    ║
  ║           ║  ★✦★                 ★✦★                    ║
  ║           ║  (rainbow cycle, explosion, time-freeze)     ║
  ║           ║                                              ║
  ╚═══════════╩══════════════════════════════════════════════╝
```

#### Popup Position & Lifecycle

```
  Popups spawn at obstacle collision point and float upward:

  TIME        POSITION          OPACITY    SCALE
  ─────────   ──────────────    ─────────  ─────
  t=0.00s     y = collision     100%       0.5 → 1.2 (spring)
  t=0.10s     y = coll - 20px   100%       1.2 → 1.0 (settle)
  t=0.50s     y = coll - 60px   100%       1.0
  t=0.80s     y = coll - 80px    80%       1.0
  t=1.20s     y = coll - 120px   0%        1.0 (faded out)

  Multiple popups can coexist (rapid obstacles).
  Newer popups push older ones higher.
```

---

### 4c. OBSTACLE CLEAR (Passing through gate / dodging block)

```
  TRIGGER: Player passes through a gate or dodges a block

  ┌──────────────┬──────────────────────────────────────────┐
  │  CHANNEL     │  RESPONSE                                │
  ├──────────────┼──────────────────────────────────────────┤
  │  VISUAL      │  Gate: opens/shatters as player passes   │
  │              │  ├─ Gate edges dissolve into 6 particles │
  │              │  ├─ Particles fly outward, fade in 0.5s  │
  │              │  └─ Brief color ring at pass-through     │
  │              │                                          │
  │              │  Block: whooshes past player              │
  │              │  ├─ Speed lines appear briefly (0.2s)    │
  │              │  └─ Block shrinks into distance          │
  │              │                                          │
  │  AUDIO       │  Gate: "whoosh" + resonant pass-through  │
  │              │  Block: quick "zip" SFX                  │
  │              │                                          │
  │  HAPTIC      │  None (save haptics for bigger moments)  │
  └──────────────┴──────────────────────────────────────────┘

  GATE PASS-THROUGH ANIMATION (4 frames):
  ────────────────────────────────────────

  Frame 0         Frame 1         Frame 2         Frame 3
  (approach)      (contact)       (through)       (shatter)

  ╔══════════╗    ╔═══  ═══╗    ╔═         ═╗     ·  ·    ·
  ║██╭──╮████║    ║██╭─●╮██║    ║█ ╭●─╮  █║       · ·  ·
  ║██│  │████║    ║██│  │██║    ║  │  │   ║     ·    ·
  ║██╰──╯████║    ║██╰──╯██║    ║█ ╰──╯  █║       · ·  ·
  ╚══════════╝    ╚════════╝    ╚═         ═╝     ·  ·    ·
  (solid gate)   (player enters) (gate splits)   (particles)
```

---

### 4d. NEAR MISS (Player clears at ×4.0 or higher) — ARCHIVED — issue #239

> ⚠️ **ARCHIVED.** The trigger condition below ("burnout multiplier ≥ ×4.0")
> depends on the removed burnout system (see §4b ARCHIVED note and the
> top-of-document supersession banner). Any near-miss / time-dilation
> feedback in the current design must be re-keyed off the timing-grade
> × chain model (`design-docs/rhythm-spec.md` §6) before implementation.
> The visual / audio / haptic spec below is retained only as historical
> context and should NOT be implemented as-is.

```
  TRIGGER: Burnout multiplier ≥ ×4.0 at moment of clear
  DURATION: 0.25 seconds

  ┌──────────────┬──────────────────────────────────────────┐
  │  CHANNEL     │  RESPONSE                                │
  ├──────────────┼──────────────────────────────────────────┤
  │  VISUAL      │  1. TIME DILATION: game slows to 50%     │
  │              │     for 3 frames, then snaps back         │
  │              │     (gives weight to the moment)          │
  │              │                                          │
  │              │  2. RADIAL BLUR: edges blur outward      │
  │              │     from player position (0.15s)          │
  │              │                                          │
  │              │  3. CHROMATIC ABERRATION: brief RGB       │
  │              │     split on screen edges (0.1s)          │
  │              │                                          │
  │              │  4. VIGNETTE PULSE: screen edges          │
  │              │     darken momentarily                    │
  │              │                                          │
  │  AUDIO       │  Low bass "THOOM" + time-stretch effect  │
  │              │                                          │
  │  HAPTIC      │  Single heavy pulse                      │
  └──────────────┴──────────────────────────────────────────┘

  NEAR-MISS VISUAL SEQUENCE:
  ──────────────────────────

  NORMAL          NEAR-MISS          RECOVERY
  ┌──────────┐    ┌──────────┐      ┌──────────┐
  │          │    │▓▓      ▓▓│      │          │
  │          │    │▓        ▓│      │          │
  │    ●     │    │  ●●●●    │      │    ●     │
  │          │    │▓  blur  ▓│      │          │
  │          │    │▓▓      ▓▓│      │          │
  └──────────┘    └──────────┘      └──────────┘
   (normal)       (radial blur       (instant
                   + vignette          recovery)
                   + time slow)
```

---

### 4e. CHAIN BUILDING (Consecutive obstacle clears)

```
  TRIGGER: Clearing 2+ obstacles without dying
  VISUAL ESCALATION BY CHAIN LENGTH:

  ╔══════════╦══════════════════════════════════════════════╗
  ║  CHAIN   ║  VISUAL FEEDBACK                            ║
  ╠══════════╬══════════════════════════════════════════════╣
  ║          ║                                              ║
  ║  1       ║  (nothing special — baseline)                ║
  ║          ║                                              ║
  ║  2       ║  Small "×2 CHAIN" text under score           ║
  ║          ║  Subtle trail appears behind player          ║
  ║          ║                                              ║
  ║          ║  SCORE: 03,200    ×2 CHAIN                   ║
  ║          ║                   ~~                          ║
  ║          ║                                              ║
  ║  3       ║  "×3 CHAIN" text, player trail brightens     ║
  ║          ║  Lane dividers pulse once                    ║
  ║          ║                                              ║
  ║          ║  SCORE: 03,500    ×3 CHAIN                   ║
  ║          ║                   ~~~                         ║
  ║          ║                                              ║
  ║  4       ║  "×4 CHAIN" text grows, trail now FIRE-like  ║
  ║          ║  Background subtle color shift (darker)      ║
  ║          ║                                              ║
  ║          ║  SCORE: 03,900   ★×4 CHAIN★                  ║
  ║          ║                  ~~~~                         ║
  ║          ║                                              ║
  ║  5+      ║  "×5 CHAIN!" + CHAIN FEVER overlay           ║
  ║          ║  Screen border glows chain color              ║
  ║          ║  Background pulses to the rhythm              ║
  ║          ║  Player has blazing trail                     ║
  ║          ║                                              ║
  ║          ║  SCORE: 04,500  ★★×5 CHAIN FEVER!★★          ║
  ║          ║                 ~~~~~                         ║
  ║          ║                                              ║
  ╚══════════╩══════════════════════════════════════════════╝

  CHAIN TRAIL PROGRESSION:
  ────────────────────────

  Chain 1       Chain 2       Chain 3       Chain 5+
  (none)        (subtle)      (bright)      (BLAZING)

     ●             ●             ●              ●
                   │            ╱│╲          ╱╲╱│╲╱╲
                   ·           ╱ · ╲        ╱╲ · ╱╲╱
                               · ·        ╱╲╱·╲╱╲╱
                                           FIRE
```

#### Chain Break

```
  TRIGGER: Player dies (chain resets to 0)

  VISUAL:  Chain counter shatters into particles
           Trail extinguishes instantly
           Brief "chain broken" grey text (0.5s)

  AUDIO:   Glass-break SFX (only if chain ≥ 3)
```

---

### 4f. GAME OVER — Death Sequence

> ⚠️ **PARTIAL ARCHIVED — issue #239.** The "Burnout meter greys out"
> step at t=0.050s and the "Best Burnout" row in the t=1.300s stats
> slide-in animation describe the removed burnout-multiplier system
> and its on-HUD meter. The death-sequence durations, camera shake,
> particle/fade timings, and overall stats-stagger pacing are still
> current; new code must just omit the burnout meter and burnout-
> derived stat rows (see §2d ARCHIVED note for the replacement
> energy-bar / rhythm-timing model).

```
  TRIGGER: Player hits obstacle with wrong shape / doesn't dodge
  TOTAL DURATION: 1.8 seconds (from crash to stats visible)

  TIMELINE:
  ═════════

  t=0.000s  ──── IMPACT ────────────────────────────────
  │
  │  • Game world FREEZES
  │  • Player shape SHATTERS into 12-16 particles
  │  • Screen FLASH (white, 0.05s)
  │  • Camera SHAKE (8px amplitude, 0.3s)
  │  • Haptic: HEAVY double-pulse
  │  • Audio: CRASH + glass shatter SFX
  │
  t=0.050s  ──── SLOW-MOTION ───────────────────────────
  │
  │  • Time scale drops to 10% (0.6s real = 0.06s game)
  │  • Particles drift slowly outward from crash point
  │  • Background begins to desaturate
  │  • Score stops counting
  │  • Burnout meter greys out  ← ARCHIVED #239 (no meter on current HUD)
  │
  t=0.650s  ──── FADE TO DARK ──────────────────────────
  │
  │  • Screen fades to 80% black (0.3s)
  │  • Particles fully faded
  │  • All HUD elements fade out except score
  │  • Audio: low drone fades in
  │
  t=0.950s  ──── GAME OVER TEXT ────────────────────────
  │
  │  • "G A M E   O V E R" fades in from above
  │  • Score ROLLS UP from 0 to final (0.6s)
  │  • Haptic: single soft pulse on each stat appear
  │
  t=1.300s  ──── STATS SLIDE IN ────────────────────────
  │
  │  • Stats rows slide in from right, 0.08s apart:
  │    Distance ─────→
  │              Time ─────→
  │                   Obstacles ─────→
  │                              Best Burnout ─────→   ← ARCHIVED #239
  │                                            ...
  │
  t=1.800s  ──── BUTTONS APPEAR ────────────────────────
  │
  │  • [PLAY AGAIN] fades in (primary, bright)
  │  • [MENU] fades in (secondary, dim)
  │  • Player can now interact
  │
  ▼ END OF DEATH SEQUENCE
```

#### Death Crash — Frame-by-Frame

```
  FRAME 0 (IMPACT):
  ╔══════════════════════════════════════╗
  ║  SCORE: 04,720                       ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║██████████║                 ║
  ║         ║███■💥████║   CRASH!!       ║
  ║         ║██████████║                 ║
  ║         ╚══════════╝                 ║
  ║                                      ║
  ║         ████ FLASH ████              ║  ← full screen white
  ║                                      ║     for 3 frames
  ╚══════════════════════════════════════╝

  FRAME 1 (SHATTER, t=0.05s):
  ╔══════════════════════════════════════╗
  ║  SCORE: 04,720                       ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║██████████║                 ║
  ║     ▪       ▫   ▪  ║                 ║  ← particles drift
  ║       ▫   ▪       ▫ ║                 ║     slowly outward
  ║         ╚══════════╝                 ║     (slow-mo active)
  ║      ▪      ▫    ▪                   ║
  ║        ▫  ▪   ▫                      ║
  ║           ▪                          ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  FRAME 2 (FADE, t=0.65s):
  ╔══════════════════════════════════════╗
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓   04,720    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ← score lingers
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ← darkening
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ╚══════════════════════════════════════╝

  FRAME 3 (GAME OVER, t=0.95s):
  → transitions to Game Over screen (section 2d)
```

---

### 4g. HIGH SCORE CELEBRATION

```
  TRIGGER: Final score > stored best score
  PLAYS DURING: Game Over screen, after score roll-up

  ┌──────────────┬──────────────────────────────────────────┐
  │  CHANNEL     │  RESPONSE                                │
  ├──────────────┼──────────────────────────────────────────┤
  │  VISUAL      │  1. Score text turns GOLD                │
  │              │  2. "◆ NEW BEST! ◆" appears below score  │
  │              │     with rainbow color cycling (2s loop)  │
  │              │  3. Sparkle particles around score (8)    │
  │              │  4. Previous best shown dimmed below:     │
  │              │     "(prev: 18,200)"                     │
  │              │  5. Screen border does gold pulse         │
  │              │                                          │
  │  AUDIO       │  Fanfare: ascending arpeggio + cymbal    │
  │              │  (plays once, 1.5s duration)              │
  │              │                                          │
  │  HAPTIC      │  Triple-tap celebration pattern           │
  │              │  (tap-pause-tap-pause-tap)                │
  └──────────────┴──────────────────────────────────────────┘

  ╔══════════════════════════════════════╗
  ║                                      ║
  ║          G A M E   O V E R           ║
  ║                                      ║
  ║     ┌────────────────────────────┐   ║
  ║     │    ✦ ✦ ✦ ✦ ✦ ✦ ✦ ✦ ✦     │   ║
  ║     │                            │   ║
  ║     │      ★ 24,850 ★           │   ║  ← GOLD text
  ║     │                            │   ║
  ║     │    ◆ NEW BEST! ◆          │   ║  ← RAINBOW cycle
  ║     │     (prev: 18,200)         │   ║
  ║     │                            │   ║
  ║     │    ✦ ✦ ✦ ✦ ✦ ✦ ✦ ✦ ✦     │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

---

### 4h. LANE SWITCH (Player swipes left/right)

```
  TRIGGER: Swipe gesture detected in gameplay area
  DURATION: 0.12 seconds

  ┌──────────────┬──────────────────────────────────────────┐
  │  CHANNEL     │  RESPONSE                                │
  ├──────────────┼──────────────────────────────────────────┤
  │  VISUAL      │  1. Player slides to new lane             │
  │              │     ease-out curve, 0.12s                  │
  │              │  2. Brief motion blur trail (3 frames)     │
  │              │  3. Dust puff at origin lane               │
  │              │                                          │
  │  AUDIO       │  Soft "swoosh" SFX                        │
  │              │                                          │
  │  HAPTIC      │  Ultra-light tap (barely perceptible)     │
  └──────────────┴──────────────────────────────────────────┘

  LANE SWITCH MOTION:
  ───────────────────

  Frame 0       Frame 1       Frame 2       Frame 3
  (start)       (moving)      (arriving)    (settled)

  │    ●    │   │   ●→   │   │         ●│   │         ●│
  │         │   │  ~~~   │   │       ~~│   │         │
  │         │   │  dust  │   │         │   │         │
  (lane 2)      (sliding)     (lane 3)      (done)
```

---

### 4i. JUMP / SLIDE (Player swipes up/down)

```
  TRIGGER: Swipe up (jump) or swipe down (slide)

  JUMP ANIMATION (0.4s total):
  ────────────────────────────

  Frame 0       Frame 2       Frame 4       Frame 6
  (ground)      (ascend)      (peak)        (land)

     ●              ●
    /|\           ─/|\─           ●           ●
    / \                          /|\         /|\  ← dust puff
  ═══════       ═══════       ═══════       ═══════
  ▓▓▓▓▓▓▓       ▓▓▓▓▓▓▓       ▓▓▓▓▓▓▓       ▓▓▓▓▓▓▓  on landing
  ═══════       ═══════       ═══════       ═══════

  Audio: "whoosh" on jump, soft "thud" on land
  Haptic: light tap on land only


  SLIDE ANIMATION (0.35s total):
  ──────────────────────────────

  Frame 0       Frame 2       Frame 4
  (upright)     (sliding)     (recover)

  ═══════       ═══════       ═══════
  ▓▓▓▓▓▓▓       ▓▓▓▓▓▓▓       ▓▓▓▓▓▓▓
  ═══════       ═══════       ═══════

     ●           ●────          ●
    /|\          ═══           /|\
    / \                        / \
  (standing)    (flat/slide)  (stand up)

  Audio: "zip" slide SFX
  Haptic: none
```

---

## 5. HUD STATE MACHINE

### Element Visibility by Game State

> ⚠️ **PARTIAL ARCHIVED — issue #239.** The `Burnout Meter` and
> `Burnout Popup` rows in the table below describe the removed burnout
> system. The current survival meter is the **Energy Bar** (see
> `design-docs/energy-bar.md` and `app/include/gameplay_hud_layout.h`)
> and the live timing cue is the **proximity ring** around shape buttons
> (`design-docs/rhythm-spec.md` §6). Treat the two `Burnout *` rows as
> historical context only; new HUD code must not add a `Burnout Meter`
> or `Burnout Popup` element. The remaining rows (Score Counter, Speed
> Bar, Pause Button, Shape Buttons, Lane Dividers, Player Avatar, Chain
> Counter, etc.) are still current.

```
  ╔═══════════════════════╦═══════╦═════════╦════════╦═══════╦════════╗
  ║  HUD ELEMENT          ║ TITLE ║ PLAYING ║ PAUSED ║ DYING ║ G.OVER ║
  ╠═══════════════════════╬═══════╬═════════╬════════╬═══════╬════════╣
  ║ Logo                  ║  ✓    ║         ║        ║       ║        ║
  ║ "Tap to Start"        ║  ✓    ║         ║        ║       ║        ║
  ║ Best Score (title)     ║  ✓    ║         ║        ║       ║        ║
  ║ Settings Gear          ║  ✓    ║         ║        ║       ║        ║
  ║ Shape Triad Anim       ║  ✓    ║         ║        ║       ║        ║
  ╠═══════════════════════╬═══════╬═════════╬════════╬═══════╬════════╣
  ║ Score Counter          ║       ║  ✓      ║  ✓ dim ║ ✓ freeze║      ║
  ║ Best Score (HUD)       ║       ║  ✓      ║  ✓ dim ║       ║        ║
  ║ Speed Bar              ║       ║  ✓      ║  ✓ dim ║       ║        ║
  ║ Pause Button (⏸)       ║       ║  ✓      ║        ║       ║        ║
  ║ Burnout Meter          ║       ║  ✓      ║  ✓ dim ║ ✓ grey ║       ║  ← ARCHIVED #239 (removed; use Energy Bar)
  ║ Shape Buttons          ║       ║  ✓      ║  ✓ dim ║       ║        ║
  ║ Lane Dividers          ║       ║  ✓      ║  ✓ dim ║ ✓ fade ║       ║
  ║ Player Avatar          ║       ║  ✓      ║  ✓ dim ║ shatter║       ║
  ║ Chain Counter          ║       ║  ✓*     ║  ✓ dim ║       ║        ║
  ║ Burnout Popup          ║       ║  ✓*     ║        ║       ║        ║  ← ARCHIVED #239 (removed)
  ╠═══════════════════════╬═══════╬═════════╬════════╬═══════╬════════╣
  ║ "PAUSED" text          ║       ║         ║  ✓     ║       ║        ║
  ║ Resume Button          ║       ║         ║  ✓     ║       ║        ║
  ║ Restart Button         ║       ║         ║  ✓     ║       ║        ║
  ║ Quit Button            ║       ║         ║  ✓     ║       ║        ║
  ║ Dim Overlay            ║       ║         ║  ✓     ║       ║        ║
  ╠═══════════════════════╬═══════╬═════════╬════════╬═══════╬════════╣
  ║ "GAME OVER" text       ║       ║         ║        ║       ║  ✓     ║
  ║ Final Score (big)      ║       ║         ║        ║       ║  ✓     ║
  ║ Stats Panel            ║       ║         ║        ║       ║  ✓     ║
  ║ New Best Badge         ║       ║         ║        ║       ║  ✓*    ║
  ║ Play Again Button      ║       ║         ║        ║       ║  ✓     ║
  ║ Menu Button            ║       ║         ║        ║       ║  ✓     ║
  ╚═══════════════════════╩═══════╩═════════╩════════╩═══════╩════════╝

  ✓* = conditional (shows only when relevant)
  ✓ dim = visible but at 40% opacity, non-interactive
  ✓ freeze = visible but not updating
  ✓ grey = visible but greyed out
  ✓ fade = fading out during death sequence
```

---

### 5a. BURNOUT METER — Animation Specification (ARCHIVED — issue #239)

> ⚠️ **ARCHIVED.** The burnout meter was removed from the game design (see
> the supersession note at the top of this document). The current survival
> meter is the **energy bar** (`design-docs/energy-bar.md`); the live
> timing cue is the **proximity ring** around shape buttons
> (`design-docs/rhythm-spec.md` §6). The animation specification below is
> retained only as historical context and should NOT be implemented.

```
  THE BURNOUT METER IS THE EMOTIONAL HEARTBEAT OF THE GAME.
  It must feel ALIVE, not like a boring progress bar.

  ┌───────────────────────────────────────────────────┐
  │  BURNOUT METER ANATOMY:                           │
  │                                                   │
  │  ┌─ track background (dark grey, rounded) ─────┐  │
  │  │                                             │  │
  │  │  ████████████████████░░░░░░░░░░░░░░░░░░░░  │  │
  │  │  ↑                                          │  │
  │  │  fill bar (color changes by zone)           │  │
  │  │                                             │  │
  │  └──────────────────┬──────────┬───────────────┘  │
  │                     │          │                   │
  │                 zone marker  zone marker           │
  │                 (30%)        (70%)                 │
  └───────────────────────────────────────────────────┘
```

#### Burnout Meter Color Zones

```
  FILL PERCENTAGE    ZONE      COLOR           BEHAVIOR
  ═════════════════  ════════  ══════════════  ═══════════════════
  0–30%              SAFE      Cyan (#00DDFF)  Smooth fill, steady
  30–60%             RISKY     Yellow (#FFDD00) Fill + gentle pulse
  60–85%             DANGER    Orange (#FF8800) Fill + fast pulse
                                               screen edge tint
  85–95%             CRITICAL  Red (#FF2200)    INTENSE pulse
                                               meter SHAKES (2px)
                                               background throb
  95–100%            DEAD ZONE Deep Red + ☠    Flash on/off
                                               everything screams
                                               "ACT NOW OR DIE"

  PULSE ANIMATION BY ZONE:
  ────────────────────────

  SAFE (no pulse):
  ████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░
  ████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░  ← steady, calm
  ████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░

  RISKY (gentle pulse, 1Hz):
  ███████████████████░░░░░░░░░░░░░░░░░░░░░  ← opacity 100%
  ██████████████████░░░░░░░░░░░░░░░░░░░░░░  ← opacity 85%
  ███████████████████░░░░░░░░░░░░░░░░░░░░░  ← opacity 100%

  DANGER (fast pulse, 3Hz + shake):
  █████████████████████████████░░░░░░░░░░░  ← bright + left
   █████████████████████████████░░░░░░░░░░  ← bright + right
  █████████████████████████████░░░░░░░░░░░  ← dim + center
   █████████████████████████████░░░░░░░░░░  ← bright + left
  (meter physically shakes horizontally)

  CRITICAL (frantic flash, 6Hz + violent shake):
  ████████████████████████████████████░░░☠  ← ON + shake
  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  ← OFF
  ████████████████████████████████████░░░☠  ← ON + shake
  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  ← OFF
  (screen edges throb red, audio heartbeat SFX)
```

#### Burnout Meter — Edge Glow Effect

```
  When burnout enters DANGER zone (60%+), the screen
  edges begin to glow the meter's color:

  SAFE:                    DANGER:                  CRITICAL:
  ╔══════════════════╗     ╔══════════════════╗     ╔══════════════════╗
  ║                  ║     ║▒                ▒║     ║█                █║
  ║                  ║     ║                  ║     ║█                █║
  ║    (gameplay)    ║     ║▒  (gameplay)   ▒║     ║█  (gameplay)   █║
  ║                  ║     ║                  ║     ║█                █║
  ║                  ║     ║▒                ▒║     ║█                █║
  ╚══════════════════╝     ╚══════════════════╝     ╚══════════════════╝
   (no edge glow)          (subtle orange glow)     (INTENSE red glow)
```

---

### 5b. SCORE COUNTER — Animation Specification

```
  THE SCORE NEVER JUMPS. IT ALWAYS ROLLS.

  When points are added, the counter rolls up digit-by-digit
  like a mechanical counter. This creates anticipation and
  makes big scores feel EARNED.

  ROLL-UP SPECIFICATION:
  ──────────────────────

  Duration: score_delta / 2000 seconds (capped at 0.6s)
  Example:  +600 points → 0.3s roll-up
  Example:  +1000 points → 0.5s roll-up

  Easing: ease-out (fast start, slow finish)

  VISUAL:
                                         ┌─ point source popup
  Current:     Rolling:      Final:      │  (+600, floating up)
  ┌────────┐   ┌────────┐   ┌────────┐  ▼
  │ 04,720 │   │ 04,9▒▒ │   │ 05,320 │  ✦+600✦
  └────────┘   └────────┘   └────────┘

  The last 2-3 digits blur/scroll while rolling.
  On completion: brief scale-up pulse (1.0→1.05→1.0, 0.1s).

  If new points arrive DURING a roll-up:
  → target value increases, roll continues seamlessly
  → never interrupts, never looks janky
```

---

### 5c. SPEED BAR — Animation Specification

```
  SPEED BAR ANATOMY:
  ──────────────────

  ┌──────────────────────────────────────────┐
  │ SPD ████████████████░░░░░░░░░░░░  ×1.8  │
  │     ↑              ↑              ↑      │
  │     label     fill (smooth)    multiplier│
  └──────────────────────────────────────────┘

  Fill color changes by speed tier:
  ×1.0 – ×1.5:  Green (#44FF44)    "Easy"
  ×1.5 – ×2.0:  Yellow (#FFFF00)   "Getting fast"
  ×2.0 – ×2.5:  Orange (#FF8800)   "Fast"
  ×2.5 – ×3.0:  Red (#FF2200)      "INSANE"

  The fill increases SMOOTHLY over time (lerp, not stepping).

  At ×2.5+:
  • Speed bar pulses
  • "FAST!" label appears briefly
  • Background environment speeds up (parallax)
```

---

## 6. TRANSITION ANIMATIONS

> ⚠️ **PARTIAL ARCHIVED — issue #239.** The frame-sequence ASCII art and
> bullet timelines in §6a (Title → Gameplay), §6c (Game Over → Retry),
> and §6d (Pause Overlay) still depict a `BURNOUT ░░░░░░░░` bar at the
> bottom of the playing HUD and a "Burnout meter slides up from bottom"
> entry-animation step. The burnout meter has been removed; the current
> survival meter is the **Energy Bar** (`design-docs/energy-bar.md`,
> `app/include/gameplay_hud_layout.h`) and the live timing cue is the
> **proximity ring** around shape buttons (`design-docs/rhythm-spec.md`
> §6). Treat any `BURNOUT …` row or "burnout meter" timeline beat in
> the diagrams below as historical context only — the surrounding
> transition timing (durations, eases, retry budget, pause grace, etc.)
> is still current.

### 6a. TITLE → GAMEPLAY

```
  TRIGGER: Player taps screen on title
  DURATION: 0.6 seconds
  FEEL: Energetic launch — "let's GO!"

  TIMELINE:
  ═════════

  t=0.00s  TAP DETECTED
  │
  │  • Tap ripple effect from touch point (0.3s)
  │  • Audio: "launch" SFX (whoosh + bass drop)
  │
  t=0.05s  ELEMENTS BEGIN EXIT
  │
  │  • Logo scales up to 1.2× then fades out (0.2s)
  │  • "Tap to Start" instantly disappears
  │  • Best score slides down off screen (0.15s)
  │  • Settings gear fades out (0.1s)
  │  • Shape triad shapes ZOOM toward player position
  │
  t=0.20s  GAMEPLAY ELEMENTS ENTER
  │
  │  • Score bar slides in from top (0.2s, ease-out)
  │  • Speed bar slides in from top (0.2s, 0.05s delay)
  │  • Lanes fade in from center outward (0.15s)
  │  • Player avatar materializes at center lane (0.15s)
  │
  t=0.40s  BOTTOM HUD ENTERS
  │
  │  • Burnout meter slides up from bottom (0.15s)  ← ARCHIVED #239 (removed; Energy Bar replaces it, see energy-bar.md)
  │  • Shape buttons pop in L→R (0.05s each, bounce)
  │
  t=0.60s  GAME BEGINS
  │
  │  • First obstacle spawns (far away)
  │  • Auto-run begins
  │  • All systems active
  │
  ▼ PLAYING STATE


  FRAME SEQUENCE:
  ═══════════════

  Frame 0 (Title):         Frame 1 (t=0.1s):
  ╔════════════════════╗    ╔════════════════════╗
  ║                    ║    ║  SCORE: 00,000     ║ ← sliding in
  ║   SHAPESHIFTER     ║    ║                    ║
  ║     (fading)       ║    ║   SHAPE            ║ ← zooming out
  ║                    ║    ║   SHIFTER           ║
  ║  ● ■ ▲ (zooming)  ║    ║         (fading)   ║
  ║                    ║    ║                    ║
  ║  ▸ TAP TO START   ║    ║      ● ■ ▲         ║ ← flying down
  ║                    ║    ║   (converging)     ║
  ║  ◆ BEST: 24,850   ║    ║                    ║
  ║              ⚙    ║    ║                    ║
  ╚════════════════════╝    ╚════════════════════╝

  Frame 2 (t=0.3s):         Frame 3 (t=0.6s):
  ╔════════════════════╗    ╔════════════════════╗
  ║  SCORE: 00,000     ║    ║  SCORE: 00,000     ║
  ║  SPD ░░░░░░░ ×1.0  ║    ║  SPD ██░░░░░ ×1.0  ║
  ║                    ║    ║                    ║
  ║  ─────┬─────┬──── ║    ║  ─────┬─────┬──── ║
  ║       │     │      ║    ║       │     │      ║
  ║       │     │      ║    ║       │     │      ║
  ║       │  ●  │      ║    ║       │  ●  │      ║
  ║       │(you)│      ║    ║       │(you)│      ║
  ║  ─────┴─────┴──── ║    ║  ─────┴─────┴──── ║
  ║                    ║    ║  BURNOUT ░░░░░░░░  ║  ← ARCHIVED #239 (now Energy Bar)
  ║  (buttons rising)  ║    ║  [ ● ] [ ■ ] [ ▲ ] ║
  ╚════════════════════╝    ╚════════════════════╝
                              ↑ GAME IS LIVE
```

---

### 6b. DEATH → GAME OVER

```
  TRIGGER: Player collides with obstacle
  DURATION: 1.8 seconds (detailed in section 4f)
  FEEL: Dramatic, weighty, but NOT frustrating

  CONDENSED SEQUENCE:
  ═══════════════════

  t=0.0s    IMPACT          ← flash + shatter + shake
  t=0.05s   SLOW-MO         ← particles drift, world fades
  t=0.65s   FADE TO DARK    ← screen goes 80% black
  t=0.95s   TEXT APPEARS    ← "GAME OVER" fades in
  t=1.10s   SCORE ROLLS     ← counter spins 0 → final
  t=1.30s   STATS ENTER     ← rows slide in from right
  t=1.80s   BUTTONS APPEAR  ← ready for input

  CRITICAL: The [PLAY AGAIN] button MUST be in the
  same screen position as the most common tap area
  during gameplay (center-bottom). This way, mashing
  "one more try" is nearly instant.
```

---

### 6c. GAME OVER → RETRY

```
  TRIGGER: Player taps [PLAY AGAIN]
  DURATION: 0.4 seconds (MUST be < 0.5s — respect player's time!)
  FEEL: Instant. Snappy. "One more try."

  TIMELINE:
  ═════════

  t=0.00s  TAP DETECTED
  │
  │  • All Game Over elements INSTANTLY fade (0.1s)
  │  • Audio: quick "restart" SFX (short upward chirp)
  │  • Haptic: single crisp tap
  │
  t=0.10s  SCREEN CLEAR
  │
  │  • Black screen for 1 frame (visual reset)
  │
  t=0.12s  GAMEPLAY SPAWNS
  │
  │  • Score resets to 00,000 (no animation)
  │  • Speed resets to ×1.0
  │  • Player appears at center lane
  │  • Lanes + HUD instantly visible
  │
  t=0.20s  BRIEF COUNTDOWN
  │
  │  • Optional: "3... 2... 1..." flash?
  │  • NO. That's too slow. Just GO.
  │  • 0.2s grace period (no obstacles yet)
  │
  t=0.40s  FIRST OBSTACLE SPAWNS
  │
  │  • Game is live
  │
  ▼ PLAYING STATE

  NOTE: We chose 0.4s because:
  ─────────────────────────────
  • Players who die want to retry IMMEDIATELY
  • Any delay > 0.5s feels like punishment
  • The fast retry IS the retention hook
  • "I almost had it, let me go again" → the magic loop
```

#### Retry Flow — Frame Sequence

```
  Frame 0 (Game Over):    Frame 1 (t=0.10s):    Frame 2 (t=0.40s):
  ╔═══════════════════╗   ╔═══════════════════╗  ╔═══════════════════╗
  ║   GAME OVER       ║   ║                   ║  ║ SCORE: 00,000     ║
  ║                   ║   ║                   ║  ║ SPD ░░░░░░ ×1.0   ║
  ║   ★ 24,850 ★      ║   ║                   ║  ║                   ║
  ║                   ║   ║    (black)         ║  ║ ─────┬─────┬────  ║
  ║   Distance 1,247m ║   ║                   ║  ║      │     │      ║
  ║   ...             ║   ║                   ║  ║      │  ●  │      ║
  ║                   ║   ║                   ║  ║      │     │      ║
  ║  [▸PLAY AGAIN]    ║   ║                   ║  ║ ─────┴─────┴────  ║
  ║  [ MENU ]         ║   ║                   ║  ║ BURNOUT ░░░░░░░░  ║  ← ARCHIVED #239 (now Energy Bar)
  ║                   ║   ║                   ║  ║ [ ● ] [ ■ ] [ ▲ ] ║
  ╚═══════════════════╝   ╚═══════════════════╝  ╚═══════════════════╝
       [TAP!]               (0.1s black)           GAME IS LIVE
                                                   (0.4s total!)
```

---

### 6d. PAUSE OVERLAY — Enter / Exit

```
  ENTER (tap ⏸):
  ═══════════════

  DURATION: 0.25 seconds

  t=0.00s  • Game world FREEZES (all physics paused)
           • ⏸ button becomes non-interactive
  t=0.05s  • Dark overlay fades in from 0% → 60% opacity
  t=0.10s  • "PAUSED" text fades in (from above, 0.15s)
  t=0.15s  • Resume button slides in from left (0.10s)
  t=0.18s  • Restart button slides in from left (0.10s)
  t=0.21s  • Quit button slides in from left (0.10s)
  t=0.25s  • All elements visible, input enabled

  EXIT (tap RESUME or tap outside):
  ══════════════════════════════════

  DURATION: 0.15 seconds (faster exit than enter!)

  t=0.00s  • Buttons fade out instantly
  t=0.05s  • Overlay fades 60% → 0%
  t=0.10s  • "PAUSED" text fades out
  t=0.15s  • Game world UNFREEZES
           • ⏸ button re-enabled
           • 0.3s grace period (no new obstacles spawn)

  FRAME SEQUENCE:
  ═══════════════

  PLAYING:              PAUSE ENTER (t=0.15s):   PAUSED:
  ╔═══════════════════╗  ╔═══════════════════╗  ╔═══════════════════╗
  ║ SCORE: 04,720 ⏸  ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║ SPD ██████ ×1.8   ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║                   ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓  ║ PAUSED ║  ▓▓║
  ║ ─────┬─────┬────  ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║      │     │      ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓ [▸ RESUME]   ▓▓║
  ║      │  ●  │      ║  ║▓▓▓(dimming)▓▓▓▓▓▓║  ║▓▓ [↺ RESTART]  ▓▓║
  ║      │     │      ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓ [✕ QUIT]     ▓▓║
  ║ ─────┴─────┴────  ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║ BURNOUT ░░░░░░░░  ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ← ARCHIVED #239 (now Energy Bar)
  ║ [ ● ] [ ■ ] [ ▲ ] ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ╚═══════════════════╝  ╚═══════════════════╝  ╚═══════════════════╝
```

---

### 6e. GAME OVER → MENU (Title)

```
  TRIGGER: Player taps [MENU] on Game Over screen
  DURATION: 0.5 seconds
  FEEL: Calm return, winding down

  t=0.00s  • Game Over elements fade out (0.2s)
  t=0.20s  • Brief black (0.1s)
  t=0.30s  • Title elements fade in:
           │  Logo from above (0.2s)
           │  Shape triad materializes (0.2s)
           │  "Tap to Start" fades in (0.2s)
           │  Best score updates (may be new!)
  t=0.50s  • Title screen fully interactive
```

---

## APPENDIX A: COMPLETE AUDIO MAP

> ⚠️ **PARTIAL ARCHIVED — issue #239.** The seven `Burnout ×N.N` rows
> and the `Burnout DANGER zone` heartbeat row below describe audio cues
> for the removed burnout-multiplier system. New audio code must not
> implement these cues; the current scoring/feedback model is the
> energy bar + proximity-ring timing (`design-docs/energy-bar.md`,
> `design-docs/rhythm-spec.md` §6). All other rows (shape shifts, lane
> switch, jump/land, gates, chains, near miss, death, UI, etc.) remain
> current.

```
  ╔═════════════════════════╦══════════════════════════════════════╗
  ║  EVENT                  ║  SOUND DESCRIPTION                   ║
  ╠═════════════════════════╬══════════════════════════════════════╣
  ║  App launch             ║  (silence — let title breathe)       ║
  ║  Tap to Start           ║  "whoosh" launch                     ║
  ║  Shape shift (●)        ║  Pop - mid pitch (440Hz)             ║
  ║  Shape shift (■)        ║  Pop - low pitch (330Hz)             ║
  ║  Shape shift (▲)        ║  Pop - high pitch (550Hz)            ║
  ║  Lane switch            ║  Soft swoosh                         ║
  ║  Jump                   ║  Ascending whoosh                    ║
  ║  Land                   ║  Soft thud                           ║
  ║  Slide                  ║  Quick zip                           ║
  ║  Gate pass-through      ║  Resonant whoosh                     ║
  ║  Block dodge            ║  Quick zip                           ║
  ║  Burnout ×1.0           ║  Soft ding                           ║  ← ARCHIVED #239
  ║  Burnout ×1.5           ║  Bright ding + rise                  ║  ← ARCHIVED #239
  ║  Burnout ×2.0           ║  Double chime                        ║  ← ARCHIVED #239
  ║  Burnout ×3.0           ║  Triple chime + bass                 ║  ← ARCHIVED #239
  ║  Burnout ×4.0           ║  Power chord + crowd                 ║  ← ARCHIVED #239
  ║  Burnout ×5.0           ║  Epic brass + explosion              ║  ← ARCHIVED #239
  ║  Burnout DANGER zone    ║  Heartbeat loop (BPM = fill%)        ║  ← ARCHIVED #239
  ║  Chain ×2               ║  Subtle chime                        ║
  ║  Chain ×3               ║  Rising chime                        ║
  ║  Chain ×5+              ║  Triumphant sting                    ║
  ║  Chain break            ║  Glass shatter (if chain ≥ 3)        ║
  ║  Near miss              ║  Low bass "THOOM"                    ║
  ║  Crash / Death          ║  Impact + shatter                    ║
  ║  Game Over              ║  Low drone fade-in                   ║
  ║  Score roll-up          ║  Ticking counter                     ║
  ║  New High Score         ║  Fanfare arpeggio + cymbal            ║
  ║  Retry                  ║  Short upward chirp                  ║
  ║  Pause enter            ║  Soft "pause" tone                   ║
  ║  Pause exit             ║  Reverse of pause tone               ║
  ║  Button hover/tap (UI)  ║  Soft click                          ║
  ╚═════════════════════════╩══════════════════════════════════════╝
```

---

## APPENDIX B: COMPLETE HAPTIC MAP

> ⚠️ **PARTIAL ARCHIVED — issue #239.** The five `Burnout ×N.N` rows
> below describe haptic patterns for the removed burnout-multiplier
> system. New haptic code must not implement these patterns; haptic
> feedback for clears/near-misses/deaths is now driven by the energy
> bar + proximity-ring timing model (`design-docs/energy-bar.md`,
> `design-docs/rhythm-spec.md` §6). All other rows (shape shift, lane
> switch, jump, near miss, death, new high score, retry, UI) remain
> current.

```
  ╔═════════════════════════╦═══════════════════════╦══════════════╗
  ║  EVENT                  ║  HAPTIC TYPE           ║  INTENSITY   ║
  ╠═════════════════════════╬═══════════════════════╬══════════════╣
  ║  Shape shift            ║  Single tap            ║  Light       ║
  ║  Lane switch            ║  Single tap            ║  Ultra-light ║
  ║  Jump (land)            ║  Single tap            ║  Light       ║
  ║  Burnout ×1.5           ║  Single tap            ║  Light       ║  ← ARCHIVED #239
  ║  Burnout ×2.0           ║  Single tap            ║  Medium      ║  ← ARCHIVED #239
  ║  Burnout ×3.0           ║  Single tap            ║  Medium      ║  ← ARCHIVED #239
  ║  Burnout ×4.0           ║  Single impact         ║  Heavy       ║  ← ARCHIVED #239
  ║  Burnout ×5.0           ║  Triple pulse          ║  Heavy       ║  ← ARCHIVED #239
  ║  Near miss              ║  Single pulse          ║  Heavy       ║
  ║  Death crash            ║  Double pulse          ║  Heavy       ║
  ║  New High Score         ║  Triple tap pattern    ║  Medium      ║
  ║  Retry tap              ║  Single tap            ║  Crisp       ║
  ║  UI button tap          ║  Single tap            ║  Ultra-light ║
  ╚═════════════════════════╩═══════════════════════╩══════════════╝

  NOTE: All haptics are opt-out (on by default).
  Players with haptics OFF get 0ms vibration (no fallback).
  raylib haptic: platform-specific vibration for simple patterns.
```

---

## APPENDIX C: TIMING CONSTANTS (for C++ implementation)

```cpp
  // ═══════════════════════════════════════════════
  // game_flow_constants.h
  // All timing values referenced in this document
  // ═══════════════════════════════════════════════

  namespace flow {

      // Transitions (seconds)
      constexpr float TITLE_TO_GAMEPLAY       = 0.60f;
      constexpr float DEATH_IMPACT_FLASH      = 0.05f;
      constexpr float DEATH_SLOW_MO           = 0.60f;
      constexpr float DEATH_FADE_TO_DARK      = 0.30f;
      constexpr float DEATH_TEXT_APPEAR        = 0.25f;
      constexpr float DEATH_STATS_STAGGER     = 0.08f;  // per row
      constexpr float DEATH_TOTAL             = 1.80f;
      constexpr float RETRY_TOTAL             = 0.40f;  // MUST be < 0.5!
      constexpr float RETRY_GRACE_PERIOD      = 0.20f;
      constexpr float PAUSE_ENTER             = 0.25f;
      constexpr float PAUSE_EXIT              = 0.15f;
      constexpr float PAUSE_RESUME_GRACE      = 0.30f;
      constexpr float GAMEOVER_TO_TITLE       = 0.50f;

      // Animations (seconds)
      constexpr float SHAPE_MORPH             = 0.12f;
      constexpr float BUTTON_POP             = 0.15f;
      constexpr float LANE_SWITCH             = 0.12f;
      constexpr float JUMP_DURATION           = 0.40f;
      constexpr float SLIDE_DURATION          = 0.35f;
      constexpr float GATE_SHATTER            = 0.20f;
      constexpr float POPUP_LIFETIME          = 1.20f;
      constexpr float NEAR_MISS_SLOWMO        = 0.15f;  // 3 frames at 50%
      constexpr float SCORE_ROLLUP_MAX        = 0.60f;
      constexpr float TAP_TO_START_PULSE      = 2.00f;  // full cycle

      // Burnout meter — ARCHIVED #239 (removed; do NOT add these
      // constants to new code. Use the Energy Bar model in
      // design-docs/energy-bar.md and the proximity-ring timing
      // constants in design-docs/rhythm-spec.md §6 instead.)
      constexpr float BURNOUT_PULSE_RISKY_HZ  = 1.0f;
      constexpr float BURNOUT_PULSE_DANGER_HZ = 3.0f;
      constexpr float BURNOUT_PULSE_CRIT_HZ   = 6.0f;
      constexpr float BURNOUT_SHAKE_PX        = 2.0f;

      // Camera
      constexpr float SHAKE_CLUTCH_PX         = 2.0f;   // ×3
      constexpr float SHAKE_INSANE_PX         = 4.0f;   // ×4
      constexpr float SHAKE_LEGENDARY_PX      = 6.0f;   // ×5
      constexpr float SHAKE_DEATH_PX          = 8.0f;

  } // namespace flow
```

---

## APPENDIX D: IMPLEMENTATION PRIORITY

```
  Build juice in this order. Each layer adds satisfaction:

  PRIORITY   SYSTEM                  IMPACT    EFFORT
  ════════   ══════════════════════  ════════  ════════
  P0         Screen flow + states    CRITICAL  Medium
  P0         Shape shift (instant)   CRITICAL  Low
  P0         Burnout meter (basic)   CRITICAL  Medium  ← ARCHIVED #239
  P0         Score counter (basic)   CRITICAL  Low
  P0         Retry (< 0.5s)         CRITICAL  Low
  ─────────────────────────────────────────────────────
  P1         Death sequence          HIGH      Medium
  P1         Burnout popups          HIGH      Low     ← ARCHIVED #239
  P1         Button pop animation    HIGH      Low
  P1         Gate pass-through FX    HIGH      Medium
  P1         Lane switch animation   HIGH      Low
  ─────────────────────────────────────────────────────
  P2         Burnout meter zones     MEDIUM    Medium  ← ARCHIVED #239
  P2         Chain counter + trail   MEDIUM    Medium
  P2         Near-miss effects       MEDIUM    Medium
  P2         Score roll-up           MEDIUM    Low
  P2         Speed bar colors        MEDIUM    Low
  ─────────────────────────────────────────────────────
  P3         FTUE tutorial runs      MEDIUM    High
  P3         Particle systems        LOW       Medium
  P3         Screen edge glow        LOW       Medium
  P3         Haptic feedback         LOW       Low
  P3         High score celebration  LOW       Low
  P3         Settings panel          LOW       Low

  RULE: Ship P0 first. P0 alone makes a playable game.
        P1 makes it FEEL good. P2 makes it ADDICTIVE.
        P3 is polish — do it when everything else works.
```

---

```
  ╔═══════════════════════════════════════════════════════╗
  ║                                                       ║
  ║   This document is the SINGLE SOURCE OF TRUTH for     ║
  ║   what the player SEES, HEARS, and FEELS.             ║
  ║                                                       ║
  ║   If it's not in here, it doesn't happen.             ║
  ║   If it IS in here, it MUST happen exactly as spec'd. ║
  ║                                                       ║
  ║   — Sr Game Designer                                  ║
  ║                                                       ║
  ╚═══════════════════════════════════════════════════════╝
```

---

*End of Game Flow & UX Specification. Cross-reference with `game.md` for rules and `rhythm-design.md` / `rhythm-spec.md` for scoring and beat semantics. (The legacy `prototype.md` ASCII scenarios have been archived to `archive/prototype.md` and no longer reflect current design — see issue #393.)*
