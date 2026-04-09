# SHAPESHIFTER — Game Flow & UX Specification
## v1.0 — The Definitive Player Experience Blueprint

> This document defines **every screen, every transition, every piece of feedback**
> the player experiences from first launch to 100th death.
> If it's on screen, it's in here. If the player feels it, it's in here.

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

```
                          ┌─────────────────┐
                          │   APP LAUNCH     │
                          │  (splash/load)   │
                          └────────┬────────┘
                                   │
                          ┌────────▼────────┐
                     ┌───▶│  TITLE SCREEN   │◄──────────────────┐
                     │    │                 │                    │
                     │    │  ● logo         │                    │
                     │    │  ● tap to start │                    │
                     │    │  ● best score   │                    │
                     │    │  ● settings ⚙   │                    │
                     │    └────────┬────────┘                    │
                     │             │                             │
                     │        [TAP SCREEN]                       │
                     │             │                             │
                     │    ┌────────▼────────┐                    │
                     │    │  FTUE CHECK     │                    │
                     │    │  (first time?)  │                    │
                     │    └───┬─────────┬───┘                    │
                     │        │  YES    │ NO                     │
                     │        ▼         ▼                        │
                     │  ┌──────────┐ ┌──────────┐               │
                     │  │ TUTORIAL │ │ GAMEPLAY │               │
                     │  │ RUN 1-5  │ │  SCREEN  │◄──┐           │
                     │  │ (guided) │ │ (full)   │   │           │
                     │  └────┬─────┘ └────┬─────┘   │           │
                     │       │            │          │           │
                     │       │  on death  │  on death│           │
                     │       ▼            ▼          │           │
                     │  ┌─────────────────────┐      │           │
                     │  │                     │      │           │
                     │  │    GAME OVER        │      │           │
                     │  │    SCREEN           │      │           │
                     │  │                     │      │           │
                     │  │  ● final score      │      │           │
                     │  │  ● stats breakdown  │      │           │
                     │  │  ● [RETRY] [MENU]   │      │           │
                     │  │                     │      │           │
                     │  └───┬────────────┬────┘      │           │
                     │      │            │           │           │
                     │ [MENU btn]   [RETRY btn]      │           │
                     │      │            │           │           │
                     └──────┘            └───────────┘           │
                                                                 │
                     ┌──────────────────────────────┐            │
                     │         PAUSE SCREEN         │            │
                     │  (overlay during gameplay)    │            │
                     │                              │            │
                     │    [RESUME] → back to game   │            │
                     │    [RESTART] → new run ───────────────────┤
                     │    [QUIT]   → title ──────────────────────┘
                     └──────────────────────────────┘
```

### State Enumeration (for ECS implementation)

```
  enum class GameState {
      SPLASH,        // app loading (< 1 second, preload assets)
      TITLE,         // main menu
      TUTORIAL,      // guided runs 1-5
      PLAYING,       // core gameplay
      PAUSED,        // overlay on gameplay
      DYING,         // crash animation (0.8s)
      GAME_OVER,     // results screen
      TRANSITIONING  // between any two states
  };
```

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
  ║│   Sound      [■■■■■■■░░░] 70%     │║
  ║│                                    │║
  ║│   Music      [■■■■░░░░░░] 40%     │║
  ║│                                    │║
  ║│   Haptics    [ ON  ●│ off ]        │║
  ║│                                    │║
  ║│   ──────────────────────────       │║
  ║│                                    │║
  ║│   Reset Tutorial  [ RESET ]        │║
  ║│                                    │║
  ║│          [ ✕ CLOSE ]               │║
  ║│                                    │║
  ║└────────────────────────────────────┘║
  ╚══════════════════════════════════════╝
```

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
  ║ │ BURNOUT ░░░░░░▓▓▓▓▓████████░░░  │ ║    BURNOUT
  ║ │         SAFE    RISKY  DANGER    │ ║    METER
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
  ║  Burnout meter        ║  x:0.05W y:0.79H ║  0.90W × 0.05H║
  ║  ├─ Track background  ║  full width      ║  rounded rect  ║
  ║  ├─ Fill bar          ║  left→right      ║  variable width║
  ║  └─ Zone markers      ║  at 30%, 60%, 90%║  tick marks    ║
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
  ║     │  Best Burnout   ×4.0       │   ║
  ║     │  Longest Chain  5          │   ║
  ║     │  Avg Burnout    ×2.3       │   ║
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
  ║  Best Burnout        ║  highest single mult ║ "×4.0"  ║
  ║  Longest Chain       ║  max consecutive     ║ "5"     ║
  ║  Avg Burnout         ║  mean of all mults   ║ "×2.3"  ║
  ╚══════════════════════╩══════════════════════╩═════════╝

  HIGHLIGHT RULES:
  ─────────────────
  • If Best Burnout ≥ ×4.0  → show in GOLD with ★ icon
  • If Longest Chain ≥ 5    → show in GOLD with ★ icon
  • If new personal best    → "◆ NEW BEST! ◆" rainbow text
  • If first time > 120s    → show "🔥 SURVIVOR" badge
```

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
  • Burnout meter hidden
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
  • Burnout meter still hidden
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

**Goal:** Teach lane swiping. Lane blocks appear. All 3 shape buttons now visible.

```
  WHAT'S DIFFERENT:
  ─────────────────
  • Shape gates (●, ■, ▲) + LANE BLOCKS appear
  • All 3 shape buttons visible
  • Swipe hint arrow appears on first lane block
  • Speed: ×0.8
  • Burnout meter still hidden

  WHAT PLAYER LEARNS:
  ───────────────────
  "I can swipe to dodge obstacles in my lane"
  "I need both shape matching AND dodging"
```

#### Run 3 — Frame 1: First lane block ever

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║           ╔═══╗                      ║
  ║           ║ X ║  ← BLOCK!           ║
  ║           ╚═══╝    (in your lane)    ║
  ║  ─────────┬─────────┬─────────      ║
  ║           │    :    │                ║
  ║           │    :    │                ║
  ║           │    :    │                ║
  ║           │    ●    │                ║
  ║           │  (you)  │                ║
  ║  ─────────┴─────────┴─────────      ║
  ║                                      ║
  ║           ◄──── swipe! ────►         ║  ← animated arrow
  ║                                      ║     slides L/R
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │  ●   │ │  ■   │ │  ▲   │        ║
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

#### Run 3 — Frame 2: Player swipes right — dodged!

```
  ╔══════════════════════════════════════╗
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║  ─────────┬─────────┬─────────      ║
  ║           │  ╔═══╗  │               ║
  ║           │  ║ X ║  │    ✨          ║
  ║           │  ╚═══╝  │     ●         ║
  ║           │         │   (safe!)     ║
  ║  ─────────┴─────────┴─────────      ║
  ║                                      ║
  ║            ✓ NICE!                   ║
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │  ●   │ │  ■   │ │  ▲   │        ║
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  Run continues with mix of gates + blocks.
  After 8 obstacles, auto-end.
```

---

### TUTORIAL RUN 4 — "Risk & Reward"

**Goal:** Introduce the burnout meter. Show waiting = more points.

```
  WHAT'S DIFFERENT:
  ─────────────────
  • Burnout meter NOW VISIBLE for first time
  • Score counter visible
  • Speed: ×0.8 (still gentle)
  • First obstacle has a HUGE burnout window (3× normal)
  • Visual cue: burnout meter glows brighter as it fills
  • One-time hint text: "WAIT for more points!" (6 words, our limit)

  WHAT PLAYER LEARNS:
  ───────────────────
  "The meter fills up as obstacles get closer"
  "Waiting longer = more points"
  "But wait too long = death"
```

#### Run 4 — Frame 1: Burnout meter appears for the first time

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
  ║              :                       ║
  ║              ■  ← you               ║
  ║                                      ║
  ║ ┌──── NEW! ──────────────────────┐   ║
  ║ │ BURNOUT ░░░░░░░░░░░░░░░░░░░░░ │   ║  ← glowing border
  ║ │     wait for more points!      │   ║     to draw attention
  ║ └────────────────────────────────┘   ║
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │  ●   │ │ ■■■  │ │  ▲   │        ║
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

#### Run 4 — Frame 2: Burnout building, visual reward preview

```
  ╔══════════════════════════════════════╗
  ║  SCORE: 00,000         ×2.0 ✦       ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║         ╔══════════╗                 ║
  ║         ║███╭──╮███║  ← closer      ║
  ║         ╚══════════╝                 ║
  ║              :                       ║
  ║              :                       ║
  ║              ■  ← still waiting...   ║
  ║                                      ║
  ║                                      ║
  ║ ┌────────────────────────────────┐   ║
  ║ │ BURNOUT ░░░░░▓▓▓▓▓▓░░░░░░░░░ │   ║
  ║ │              ↑                 │   ║
  ║ │        ×2 = 400 pts!           │   ║  ← shows point
  ║ └────────────────────────────────┘   ║     preview!
  ║                                      ║
  ║   ┌──────┐ ┌──────┐ ┌──────┐        ║
  ║   │  ●   │ │ ■■■  │ │  ▲   │        ║
  ║   └──────┘ └──────┘ └──────┘        ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  The burnout meter shows a POINT PREVIEW during
  tutorial run 4 ONLY. This makes the relationship
  between waiting and scoring visually explicit.

  After this run, the point preview disappears.
  The player now instinctively understands.
```

#### Run 4 — Frame 3: Player banks at ×3 — big reward!

```
  ╔══════════════════════════════════════╗
  ║  SCORE: 00,600  ★+600★   ×3.0       ║
  ║                                      ║
  ║                                      ║
  ║        ✨ ★ CLUTCH! ★ ✨              ║
  ║             ×3.0                      ║
  ║          600 points!                  ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ║              ●  ← switched!          ║
  ║                                      ║
  ║ ┌────────────────────────────────┐   ║
  ║ │ BURNOUT ░░░░░░░░░░░░░░░░░░░░░ │   ║
  ║ │          ^reset                │   ║
  ║ └────────────────────────────────┘   ║
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
  • Full HUD: score, speed, burnout, all buttons
  • Normal difficulty ramp
  • No more tutorial hints
  • This IS the real game now

  WHAT PLAYER HAS LEARNED:
  ────────────────────────
  Run 1: Shape matching (tap button to change)
  Run 2: Shape switching (read what's coming, react)
  Run 3: Lane dodging (swipe to avoid)
  Run 4: Burnout scoring (wait = more points)
  Run 5: Put it all together → PLAY
```

### Tutorial Flow Summary

```
  ╔══════════════════════════════════════════════════╗
  ║  RUN  ║ CONCEPT        ║ MECHANICS ACTIVE        ║
  ╠═══════╬════════════════╬═════════════════════════╣
  ║   1   ║ Match shape    ║ ■ gates, 1 button       ║
  ║   2   ║ Switch shapes  ║ ●■ gates, 2 buttons     ║
  ║   3   ║ Dodge + switch ║ ●■▲ gates + lane blocks ║
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

### 4b. BURNOUT BANK (Player clears obstacle with burnout multiplier)

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

### 4d. NEAR MISS (Player clears at ×4.0 or higher)

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
  │  • Burnout meter greys out
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
  │                              Best Burnout ─────→
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
  ║ Burnout Meter          ║       ║  ✓      ║  ✓ dim ║ ✓ grey ║       ║
  ║ Shape Buttons          ║       ║  ✓      ║  ✓ dim ║       ║        ║
  ║ Lane Dividers          ║       ║  ✓      ║  ✓ dim ║ ✓ fade ║       ║
  ║ Player Avatar          ║       ║  ✓      ║  ✓ dim ║ shatter║       ║
  ║ Chain Counter          ║       ║  ✓*     ║  ✓ dim ║       ║        ║
  ║ Burnout Popup          ║       ║  ✓*     ║        ║       ║        ║
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

### 5a. BURNOUT METER — Animation Specification

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
  │  • Burnout meter slides up from bottom (0.15s)
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
  ║                    ║    ║  BURNOUT ░░░░░░░░  ║
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
  ║  [ MENU ]         ║   ║                   ║  ║ BURNOUT ░░░░░░░░  ║
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
  ║ BURNOUT ░░░░░░░░  ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
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
  ║  Burnout ×1.0           ║  Soft ding                           ║
  ║  Burnout ×1.5           ║  Bright ding + rise                  ║
  ║  Burnout ×2.0           ║  Double chime                        ║
  ║  Burnout ×3.0           ║  Triple chime + bass                 ║
  ║  Burnout ×4.0           ║  Power chord + crowd                 ║
  ║  Burnout ×5.0           ║  Epic brass + explosion              ║
  ║  Burnout DANGER zone    ║  Heartbeat loop (BPM = fill%)        ║
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

```
  ╔═════════════════════════╦═══════════════════════╦══════════════╗
  ║  EVENT                  ║  HAPTIC TYPE           ║  INTENSITY   ║
  ╠═════════════════════════╬═══════════════════════╬══════════════╣
  ║  Shape shift            ║  Single tap            ║  Light       ║
  ║  Lane switch            ║  Single tap            ║  Ultra-light ║
  ║  Jump (land)            ║  Single tap            ║  Light       ║
  ║  Burnout ×1.5           ║  Single tap            ║  Light       ║
  ║  Burnout ×2.0           ║  Single tap            ║  Medium      ║
  ║  Burnout ×3.0           ║  Single tap            ║  Medium      ║
  ║  Burnout ×4.0           ║  Single impact         ║  Heavy       ║
  ║  Burnout ×5.0           ║  Triple pulse          ║  Heavy       ║
  ║  Near miss              ║  Single pulse          ║  Heavy       ║
  ║  Death crash            ║  Double pulse          ║  Heavy       ║
  ║  New High Score         ║  Triple tap pattern    ║  Medium      ║
  ║  Retry tap              ║  Single tap            ║  Crisp       ║
  ║  UI button tap          ║  Single tap            ║  Ultra-light ║
  ╚═════════════════════════╩═══════════════════════╩══════════════╝

  NOTE: All haptics are opt-out (on by default).
  Players with haptics OFF get 0ms vibration (no fallback).
  SDL2 haptic: SDL_HapticRumblePlay() for simple patterns.
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

      // Burnout meter
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
  P0         Burnout meter (basic)   CRITICAL  Medium
  P0         Score counter (basic)   CRITICAL  Low
  P0         Retry (< 0.5s)         CRITICAL  Low
  ─────────────────────────────────────────────────────
  P1         Death sequence          HIGH      Medium
  P1         Burnout popups          HIGH      Low
  P1         Button pop animation    HIGH      Low
  P1         Gate pass-through FX    HIGH      Medium
  P1         Lane switch animation   HIGH      Low
  ─────────────────────────────────────────────────────
  P2         Burnout meter zones     MEDIUM    Medium
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

*End of Game Flow & UX Specification. Cross-reference with `game.md` for rules and `prototype.md` for gameplay scenarios.*
