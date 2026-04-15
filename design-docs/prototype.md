# SHAPESHIFTER — ASCII ART PROTOTYPE
## Paper Prototype v1.0

> "The longer you wait, the more you score — but wait too long and you're dead."

---

## TABLE OF CONTENTS
1. Screen Layout & Controls
2. Core Visual Language
3. The Burnout Scoring Mechanic (Frame-by-Frame)
4. Obstacle Types Reference
5. Scenario A: Shape Gate (Pure Shape-Shift)
6. Scenario B: Lane Dodge (Pure Swipe)
7. Scenario C: Combo Obstacle (Shape + Swipe)
8. Scenario D: Rapid Sequence (Difficulty Ramp)
9. Failure States
10. Difficulty Progression
11. Scoring & UI Details

---

## 1. SCREEN LAYOUT & CONTROLS

The player holds the phone in PORTRAIT mode.
The screen is divided into two zones:

```
╔══════════════════════════════════╗
║          SWIPE ZONE              ║
║   (swipe anywhere in this        ║
║    area to dodge)                ║
║                                  ║
║   ← LEFT    RIGHT →             ║
║   ↑ UP (jump)                   ║
║   ↓ DOWN (slide)                ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║──────────────────────────────────║
║         BUTTON ZONE              ║
║                                  ║
║      [  ●  ] [  ■  ] [  ▲  ]    ║
║      Circle  Square  Triangle    ║
║                                  ║
╚══════════════════════════════════╝
```

### Full Screen Layout with UI:

```
╔══════════════════════════════════╗
║  SCORE: 04,720    ◆ BEST: 12,300║
║  ══════════════════              ║
║  SPEED ████████░░░░  x1.8       ║
║                                  ║
║  ┌─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─┐  ║
║  │                            │  ║
║  │     GAMEPLAY AREA          │  ║
║  │                            │  ║
║  │  Lane 1  │ Lane 2 │ Lane 3│  ║
║  │  (left)  │  (mid)  │(right)│  ║
║  │          │         │       │  ║
║  │          │         │       │  ║
║  │          │   ●     │       │  ║
║  │          │ (you)   │       │  ║
║  └─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─┘  ║
║┌────────────────────────────────┐║
║│       BURNOUT METER            │║
║│  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │║
║│           (empty = safe)       │║
║├────────────────────────────────┤║
║│                                │║
║│    [ ● ]    [ ■ ]    [ ▲ ]     │║
║│   circle   square   triangle   │║
║│                                │║
║└────────────────────────────────┘║
╚══════════════════════════════════╝
```

### Button Detail:

```
  Currently selected shape is HIGHLIGHTED:

  Normal:            Selected:
  ┌─────┐            ╔═════╗
  │     │            ║     ║
  │  ●  │            ║  ●  ║  ← glowing / pulsing
  │     │            ║     ║
  └─────┘            ╚═════╝
   gray              bright + particle effect
```

---

## 2. CORE VISUAL LANGUAGE

### The 3 Player Shapes:

```
   ●              ■              ▲
  ╭──╮           ┌──┐           /\
  │  │           │  │          /  \
  ╰──╯           └──┘         /____\
 CIRCLE          SQUARE       TRIANGLE

 ● passes        ■ passes     ▲ passes
 through         through      through
 round holes     square gaps  triangle gaps
```

### Obstacle Gate Shapes (what the player sees approaching):

```
  ╔══════════╗    ╔══════════╗    ╔══════════╗
  ║██████████║    ║██████████║    ║██████████║
  ║███╭──╮███║    ║███┌──┐███║    ║████/\████║
  ║███│  │███║    ║███│  │███║    ║███/  \███║
  ║███╰──╯███║    ║███└──┘███║    ║██/____\██║
  ║██████████║    ║██████████║    ║██████████║
  ╚══════════╝    ╚══════════╝    ╚══════════╝
   CIRCLE GATE     SQUARE GATE    TRIANGLE GATE
   need: ●         need: ■        need: ▲
```

### Lane Obstacles (dodge with swipe):

```
  BARRIER (swipe left/right to avoid):

  Lane 1    Lane 2    Lane 3
    │         │         │
    │       ╔═══╗       │
    │       ║ X ║       │       ← swipe left or right
    │       ╚═══╝       │
    │         │         │

  LOW BAR (swipe up to jump):

  ═══════════════════════════
  ████████████████████████████      ← swipe UP to jump over
  ═══════════════════════════

  HIGH BAR (swipe down to slide):

  ═══════════════════════════
  ████████████████████████████      ← swipe DOWN to slide under
  ═══════════════════════════
         (floating overhead)
```

---

## 3. THE BURNOUT SCORING MECHANIC (Frame-by-Frame)

This is the CORE of the game. Here's how it works:

### Concept:
```
  OBSTACLE APPROACHING...

  ║                           ║
  ║    ╔═══╗                  ║    The gate is far away.
  ║    ║ ● ║  ← circle gate  ║    Player is currently ■ (square).
  ║    ╚═══╝                  ║    They KNOW they need to switch to ●.
  ║                           ║
  ║         ...               ║    But WAITING = MORE POINTS.
  ║         ...               ║
  ║         ...               ║
  ║          ■  ← player      ║    The burnout meter fills up.
  ║                           ║    Switch at the LAST MOMENT = MAX SCORE.
  ║                           ║
```

### The Burnout Meter:

```
  ┌─ BURNOUT METER ──────────────────────────┐
  │                                           │
  │  SAFE          RISKY        DANGER  DEAD  │
  │  ░░░░░░░░░░░▓▓▓▓▓▓▓▓▓██████████████ ☠☠  │
  │  x1          x2       x3    x5      💀   │
  │                                           │
  │  ───────────────────────────────────►      │
  │  meter fills as obstacle approaches       │
  │  switch/swipe anytime to bank points      │
  │  hit the skull zone = CRASH               │
  └───────────────────────────────────────────┘
```

### Frame-by-Frame Burnout Sequence:

**FRAME 1 — Obstacle Detected (far away)**
```
╔══════════════════════════════════╗
║  SCORE: 04,720         x1.0     ║
║                                  ║
║         ╔══════════╗             ║
║         ║███╭──╮███║  ← ● gate  ║
║         ╚══════════╝             ║
║              :                   ║
║              :                   ║
║              :                   ║
║              :                   ║
║              :                   ║
║              :                   ║
║              :                   ║
║              ■  ← you (square)   ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║          ^safe                   ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  💭 "I see a circle gate... I should switch..."
  💭 "...but not yet. I'll WAIT for more points."
```

**FRAME 2 — Getting Closer (x2 multiplier)**
```
╔══════════════════════════════════╗
║  SCORE: 04,720         x2.0     ║
║                                  ║
║                                  ║
║                                  ║
║         ╔══════════╗             ║
║         ║███╭──╮███║  ← closer! ║
║         ╚══════════╝             ║
║              :                   ║
║              :                   ║
║              :                   ║
║              :                   ║
║              ■  ← still square   ║
║                                  ║
║  BURNOUT ░░░░░░░▓▓▓▓▓░░░░░░░░░  ║
║                 ^risky  +200pts  ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  💭 "Getting closer... x2 multiplier building..."
  💭 "Hold... hold..."
```

**FRAME 3 — Danger Zone! (x3 multiplier)**
```
╔══════════════════════════════════╗
║  SCORE: 04,720         x3.0     ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║         ╔══════════╗             ║
║         ║███╭──╮███║  ← CLOSE!! ║
║         ╚══════════╝             ║
║              :                   ║
║              :                   ║
║              ■  ← still square   ║
║                                  ║
║  BURNOUT ░░░░░░░▓▓▓▓▓████████░  ║
║                       ^DANGER!   ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  💭 "NOW! SWITCH NOW!"
  ⚡ Player taps [ ● ] button!
```

**FRAME 4a — PERFECT SWITCH! (Player tapped in time)**
```
╔══════════════════════════════════╗
║  SCORE: 05,320  ✦+600✦  x3.0   ║
║                                  ║
║             ✨ ✨ ✨               ║
║          ╔══════════╗            ║
║          ║███    ███║            ║
║          ║███ ●→ ███║  THROUGH! ║
║          ║███    ███║            ║
║          ╚══════════╝            ║
║              :                   ║
║              :                   ║
║              :                   ║
║              ●  ← now circle!   ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║          ^reset     "NICE! x3!" ║
║    [●●●]    [ ■ ]    [ ▲ ]      ║
╚══════════════════════════════════╝
  ⭐ +600 points! (200 base × 3.0 burnout multiplier)
  🎆 Screen flash! Particle burst!
```

**FRAME 4b — TOO LATE! (Player didn't switch in time)**
```
╔══════════════════════════════════╗
║  SCORE: 04,720                   ║
║                                  ║
║                                  ║
║                                  ║
║          ╔══════════╗            ║
║          ║██████████║            ║
║          ║███■💥████║  CRASH!!   ║
║          ║██████████║            ║
║          ╚══════════╝            ║
║                                  ║
║             💀                   ║
║                                  ║
║                                  ║
║  ┌──────────────────────────┐    ║
║  │     GAME OVER            │    ║
║  │  Final Score: 04,720     │    ║
║  │  Best Burnout: x3.0     │    ║
║  │                          │    ║
║  │     [ RETRY ]            │    ║
║  └──────────────────────────┘    ║
╚══════════════════════════════════╝
  💀 Wrong shape. Should have been ● but was ■.
```

---

## 4. OBSTACLE TYPES REFERENCE

```
╔═══════════════════════════════════════════════════════╗
║                  OBSTACLE CATALOG                     ║
╠═══════════════╦═══════════════╦═══════════════════════╣
║  TYPE         ║  ACTION       ║  BURNOUT APPLIES?     ║
╠═══════════════╬═══════════════╬═══════════════════════╣
║  Shape Gate   ║  Tap shape    ║  YES — delay switch   ║
║  Lane Push    ║  None (auto)  ║  NO — passive         ║
║  Low Bar      ║  Swipe UP     ║  YES — delay jump     ║
║  High Bar     ║  Swipe DOWN   ║  YES — delay slide    ║
║  Combo Gate   ║  Shape + L/R  ║  YES — both timers!   ║
║  Split Path   ║  Shape + L/R  ║  YES — pick correct   ║
╚═══════════════╩═══════════════╩═══════════════════════╝
```

### Visual Reference for Each:

```
SHAPE GATE — Wall with cutout, must match shape
══════════╗    ╔══════════
██████████║    ║██████████
███╭──╮███║    ║██████████      ← only matching shape
███│  │███║    ║██████████        fits through the hole
███╰──╯███║    ║██████████
██████████║    ║██████████
══════════╝    ╚══════════

LANE PUSH — Auto-pushes player one lane on beat arrival
    │         │         │
    │       ╔═╧═╗       │
    │       ║ ▶ ║       │       ← pushes player right (lane_push_right)
    │       ╚═╤═╝       │
    │         │         │
  (Replaces legacy Lane Block. Player takes no action — passive.
   ▲ = push left, ▼ = push right. Edge pushes are no-ops.)

LOW BAR — Ankle-height barrier across all lanes
    │         │         │
  ══╪═════════╪═════════╪══
  ▓▓│▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓▓│▓▓     ← jump over (swipe UP)
  ══╪═════════╪═════════╪══
    │         │         │

HIGH BAR — Head-height barrier across all lanes
  ══╪═════════╪═════════╪══
  ▓▓│▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓▓│▓▓     ← slide under (swipe DOWN)
  ══╪═════════╪═════════╪══
    │         │         │
    │         │         │

COMBO GATE — Shape gate in only one lane
    │         │         │
    │    ╔════╧════╗    │
    │    ║██╭──╮██ ║    │       ← must be ● AND in lane 2
    │    ║██│  │██ ║    │
    │    ║██╰──╯██ ║    │
    │    ╚════╤════╝    │
    │         │         │

SPLIT PATH — Two gates in different lanes, pick one
    │         │         │
  ╔═╧══╗   ╔═╧══╗      │
  ║╭──╮║   ║┌──┐║      │       ← lane 1 needs ●
  ║│  │║   ║│  │║      │         lane 2 needs ■
  ║╰──╯║   ║└──┘║      │         lane 3 is blocked!
  ╚═╤══╝   ╚═╤══╝      │
    │         │     ████│
```

---

## 5. SCENARIO A — Shape Gate (Pure Shape-Shift)

> Player is ■, approaching a ▲ gate. Must switch to triangle.

**A1: Running, no obstacle yet**
```
╔══════════════════════════════════╗
║  SCORE: 01,200         ×1.0     ║
║  SPEED ████░░░░░░░░  x1.2       ║
║                                  ║
║  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ║
║  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ║
║  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ║
║  ─────────┬─────────┬─────────  ║
║           │         │            ║
║           │         │            ║
║           │         │            ║
║           │    ■    │            ║
║           │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  📋 Just running. No threat. Chill.
```

**A2: Triangle gate appears on the horizon**
```
╔══════════════════════════════════╗
║  SCORE: 01,240         ×1.0     ║
║  SPEED ████░░░░░░░░  x1.2       ║
║                                  ║
║         ╔══════════╗             ║
║         ║████/\████║   ← ▲ gate ║
║         ╚══════════╝             ║
║  ─────────┬─────────┬─────────  ║
║           │    :    │            ║
║           │    :    │            ║
║           │    :    │            ║
║           │    ■    │            ║
║           │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  👁️ Gate spotted! Need ▲. Currently ■.
  💭 "I'll wait..."
```

**A3: Gate closer — burnout building ×2**
```
╔══════════════════════════════════╗
║  SCORE: 01,240      ×2.0 ✦     ║
║  SPEED ████░░░░░░░░  x1.2       ║
║                                  ║
║                                  ║
║                                  ║
║         ╔══════════╗             ║
║  ───────╫────/\────╫──────────  ║
║         ║███/  \███║             ║
║         ╚══/════\══╝             ║
║           │    :    │            ║
║           │    ■    │            ║
║           │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░▓▓▓▓▓▓░░░░░░░░░  ║
║               ↑×2                ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  ⏱️ Burnout building... ×2 multiplier active
  💭 "Hold... hold..."
```

**A4: LAST MOMENT — ×4 — player taps ▲**
```
╔══════════════════════════════════╗
║  SCORE: 01,240      ×4.0 ✦✦   ║
║  SPEED ████░░░░░░░░  x1.2       ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║         ╔══════════╗             ║
║  ───────╫────/\────╫──────────  ║
║         ║██/    \██║             ║
║           │    ■    │            ║
║           │ !!!!!! │   ← 😱     ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░▓▓▓▓▓█████████☠  ║
║                          ↑×4    ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  ⚡⚡⚡ LAST MOMENT! Player smashes [ ▲ ] !!!
```

**A5: CLEARED! ×4 burnout banked!**
```
╔══════════════════════════════════╗
║  SCORE: 02,040  ★+800★  ×4.0   ║
║  SPEED ████░░░░░░░░  x1.2       ║
║                                  ║
║        ✨  ★ CLUTCH! ★  ✨       ║
║               ×4.0               ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║  ─────────┬─────────┬─────────  ║
║           │         │            ║
║           │    ▲    │            ║
║           │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║          ↑reset                  ║
║    [ ● ]    [ ■ ]    [▲▲▲]      ║
╚══════════════════════════════════╝
  🎆 +800 points! (200 base × 4.0 burnout!)
  🎆 "CLUTCH!" text pops on screen
```

---

## 6. SCENARIO B — Lane Dodge (Pure Swipe)

> A pillar blocks the center lane. Player must swipe left or right.

**B1: Pillar appears ahead in center lane**
```
╔══════════════════════════════════╗
║  SCORE: 03,400         ×1.0     ║
║  SPEED ██████░░░░░░  x1.5       ║
║                                  ║
║           ╔═══╗                  ║
║           ║ X ║  ← pillar!      ║
║           ╚═══╝                  ║
║  ─────────┬─────────┬─────────  ║
║           │    :    │            ║
║           │    :    │            ║
║           │    :    │            ║
║           │    ●    │            ║
║           │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [●●●]    [ ■ ]    [ ▲ ]      ║
╚══════════════════════════════════╝
  👁️ Pillar in center lane! Swipe left or right!
```

**B2: Burnout building — ×2**
```
╔══════════════════════════════════╗
║  SCORE: 03,400      ×2.0 ✦     ║
║  SPEED ██████░░░░░░  x1.5       ║
║                                  ║
║                                  ║
║           ╔═══╗                  ║
║           ║ X ║  ← closer!      ║
║  ─────────╫───╫─────┬─────────  ║
║           ║   ║     │            ║
║           ╚═══╝     │            ║
║           │    :    │            ║
║           │    ●    │            ║
║           │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░▓▓▓▓▓▓░░░░░░░░░  ║
║    [●●●]    [ ■ ]    [ ▲ ]      ║
╚══════════════════════════════════╝
  ⏱️ Still in center lane... ×2 building...
```

**B3: Player swipes RIGHT at ×3**
```
╔══════════════════════════════════╗
║  SCORE: 03,700  ✦+300✦  ×3.0   ║
║  SPEED ██████░░░░░░  x1.5       ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║  ─────────┬─────────┬─────────  ║
║           │  ╔═══╗  │            ║
║           │  ║ X ║  │            ║
║           │  ╚═══╝  │   ✨       ║
║           │         │    ●      ║
║           │         │  (you)    ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [●●●]    [ ■ ]    [ ▲ ]      ║
╚══════════════════════════════════╝
  👉 Swiped right! Dodged at ×3! +300 points!
     Player is now in lane 3 (right).
```

---

## 7. SCENARIO C — Combo Obstacle (Shape + Swipe)

> A circle gate appears, but only in lane 1 (left).
> Player is ■ in lane 2 (center).
> Must: switch to ● AND swipe left.

**C1: Combo obstacle appears**
```
╔══════════════════════════════════╗
║  SCORE: 06,100         ×1.0     ║
║  SPEED ████████░░░░  x1.8       ║
║                                  ║
║  ╔══════╗                        ║
║  ║╭──╮██║████████████████████   ║
║  ║│  │██║████████████████████   ║
║  ║╰──╯██║████████████████████   ║
║  ╚══════╝                        ║
║  ─────────┬─────────┬─────────  ║
║     :     │         │            ║
║     :     │    ■    │            ║
║     :     │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  😰 Circle gate in LEFT lane only!
     Need to: 1) switch to ● AND 2) swipe LEFT
     TWO actions required!
```

**C2: Burnout — one meter per required action**
```
╔══════════════════════════════════╗
║  SCORE: 06,100      ×2.5 ✦     ║
║  SPEED ████████░░░░  x1.8       ║
║                                  ║
║                                  ║
║  ╔══════╗                        ║
║  ║╭──╮██║████████████████████   ║
║  ║│  │██║████████████████████   ║
║  ║╰──╯██║████████████████████   ║
║  ╚══════╝                        ║
║  ─────────┬─────────┬─────────  ║
║     :     │    ■    │            ║
║     :     │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║  SHAPE  ░░░░░▓▓▓▓▓▓████░░░░░░  ║
║  DODGE  ░░░░░▓▓▓▓▓▓████░░░░░░  ║
║    [ ● ]    [■■■]    [ ▲ ]      ║
╚══════════════════════════════════╝
  ⏱️ Both burnout meters ticking!
     Combo obstacles = both timers run!
     Multiplier is the AVERAGE of both.
```

**C3: Player taps ● first (shape burnout banked!)**
```
╔══════════════════════════════════╗
║  SCORE: 06,100      ×2.5 ✦     ║
║  SPEED ████████░░░░  x1.8       ║
║                                  ║
║                                  ║
║                                  ║
║  ╔══════╗                        ║
║  ║╭──╮██║████████████████████   ║
║  ║│  │██║████████████████████   ║
║  ║╰──╯██║████████████████████   ║
║  ╚══════╝                        ║
║  ─────────┬─────────┬─────────  ║
║     :     │    ●    │  ← now ●  ║
║     :     │  (you)  │            ║
║  ─────────┴─────────┴─────────  ║
║  SHAPE  ░░░ BANKED ×2.5 ░░░ ✓  ║
║  DODGE  ░░░░░░▓▓▓▓▓█████████☠  ║
║    [●●●]    [ ■ ]    [ ▲ ]      ║
╚══════════════════════════════════╝
  ✓ Shape switched! But still in WRONG LANE!
  ⚡ SWIPE LEFT NOW!!!
```

**C4: Player swipes left — BOTH cleared!**
```
╔══════════════════════════════════╗
║  SCORE: 07,350  ★+1250★ COMBO! ║
║  SPEED ████████░░░░  x1.8       ║
║                                  ║
║     ✨ ✨ ★ COMBO CLUTCH! ★ ✨ ✨  ║
║           ×2.5 + ×4.0            ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║  ─────────┬─────────┬─────────  ║
║     ●     │         │            ║
║   (you)   │         │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [●●●]    [ ■ ]    [ ▲ ]      ║
╚══════════════════════════════════╝
  🎆🎆 COMBO CLUTCH! Shape ×2.5 + Dodge ×4.0!
     Base 200 × average(2.5, 4.0) = 200 × 3.25 = 650
     COMBO BONUS: ×2 for doing both = 1300 points!
```

---

## 8. SCENARIO D — Rapid Sequence (Difficulty Ramp)

> Late game. Speed is HIGH. Three obstacles in quick succession.
> Tests reaction speed AND burnout greed management.

**D1: First obstacle — ▲ gate (at high speed)**
```
╔══════════════════════════════════╗
║  SCORE: 18,400         ×1.0     ║
║  SPEED ██████████░░  x2.5 FAST! ║
║                                  ║
║         ╔══════════╗             ║
║         ║████/\████║   ← ▲ gate ║
║         ╚══════════╝             ║
║  ─────────┬────╔═══╗┬─────────  ║
║           │    ║ X ║│  ← pillar ║
║       ╔══════╗ ╚═══╝│    after! ║
║  ═════╬══════╬═══════╬════════  ║
║  ▓▓▓▓▓║low  ▓║▓▓▓▓▓▓│▓▓▓▓▓▓▓  ║
║  ═════╬══bar═╬═══════╬═════     ║
║       ╚══════╝  ●    │  ← low  ║
║           │   (you)  │    bar   ║
║  ─────────┴─────────┴───after   ║
║                          that!   ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [●●●]    [ ■ ]    [ ▲ ]      ║
╚══════════════════════════════════╝
  😱 THREE obstacles stacked!!
     1. ▲ gate → need triangle
     2. Pillar in lane 2 → swipe right
     3. Low bar → swipe up to jump
     At x2.5 speed, barely time to react!
```

**D2: Player switches to ▲, burns through gate**
```
╔══════════════════════════════════╗
║  SCORE: 18,600  ✦+200✦ ×1.0    ║
║  SPEED ██████████░░  x2.5 FAST! ║
║                                  ║
║                                  ║
║                                  ║
║         ╔══════════╗             ║
║  ───────╫────/\────╫──────────  ║
║         ║██/  ▲\██ ║ THROUGH!   ║
║         ╚══/════\══╝             ║
║           │ ╔═══╗  │  ← NEXT!  ║
║           │ ║ X ║  │            ║
║  ═════════╬═╚═══╝══╬═════════  ║
║  ▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓  ║
║  ═════════╬═════════╬═════════  ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [ ● ]    [ ■ ]    [▲▲▲]      ║
╚══════════════════════════════════╝
  ✓ Gate 1 cleared! Only ×1.0 — no time to burn!
  ⚡ PILLAR INCOMING — swipe RIGHT!
```

**D3: Swiped right past pillar — low bar next!**
```
╔══════════════════════════════════╗
║  SCORE: 18,800  ✦+200✦ ×1.0    ║
║  SPEED ██████████░░  x2.5 FAST! ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║  ─────────┬─────────┬─────────  ║
║  ═════════╬═════════╬═════════  ║
║  ▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓  ║
║  ═════════╬═════════╬═════════  ║
║           │         │    ▲      ║
║           │         │  (you)    ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [ ● ]    [ ■ ]    [▲▲▲]      ║
╚══════════════════════════════════╝
  ✓ Pillar dodged! Now in lane 3!
  ⚡ LOW BAR — swipe UP to JUMP!
```

**D4: Jumped! Survived the gauntlet!**
```
╔══════════════════════════════════╗
║  SCORE: 19,200  ★+400★ CHAIN!  ║
║  SPEED ██████████░░  x2.5 FAST! ║
║                                  ║
║   ★ ★ ★  TRIPLE CHAIN!  ★ ★ ★  ║
║         3 obstacles cleared!     ║
║                                  ║
║                                  ║
║           ▲                      ║
║          /|\  ← jumping!         ║
║  ─────────┬─────────┬─────────  ║
║  ═════════╬═════════╬═════════  ║
║  ▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓▓│▓▓▓▓▓▓▓▓  ║
║  ═════════╬═════════╬═════════  ║
║           │         │            ║
║  ─────────┴─────────┴─────────  ║
║                                  ║
║  BURNOUT ░░░░░░░░░░░░░░░░░░░░░  ║
║    [ ● ]    [ ■ ]    [▲▲▲]      ║
╚══════════════════════════════════╝
  🎆 TRIPLE CHAIN BONUS!
     At high speed, no time to burnout —
     but chains give a FLAT BONUS instead.
     +400 = 100 per obstacle + 100 chain bonus
```

---

## 9. FAILURE STATES

### Failure: Wrong Shape
```
╔══════════════════════════════════╗
║  SCORE: 04,720                   ║
║                                  ║
║         ╔══════════╗             ║
║         ║██████████║             ║
║         ║██ ■ 💥 ██║  CRASH!!   ║
║         ║██████████║  (needed●) ║
║         ╚══════════╝             ║
║                                  ║
║        ■ shatters into           ║
║         fragments:               ║
║       ▪ ▫ ▪                      ║
║      ▫ ▪ ▫ ▪                     ║
║       ▪ ▫ ▪                      ║
║                                  ║
║  ┌──────────────────────────┐    ║
║  │       GAME OVER          │    ║
║  │  Score: 04,720           │    ║
║  │  Distance: 847m          │    ║
║  │  Best Burnout: ×4.0      │    ║
║  │  Shapes Shifted: 23      │    ║
║  │                          │    ║
║  │  [ RETRY ]  [ MENU ]    │    ║
║  └──────────────────────────┘    ║
╚══════════════════════════════════╝
```

### Failure: Didn't Dodge
```
╔══════════════════════════════════╗
║  SCORE: 03,100                   ║
║                                  ║
║                                  ║
║           ╔═══╗                  ║
║           ║ X ║                  ║
║           ║💥●║  SMASHED!       ║
║           ║   ║                  ║
║           ╚═══╝                  ║
║                                  ║
║         ● bounces off:           ║
║            ○                     ║
║           ○ ○                    ║
║            ○                     ║
║                                  ║
║  ┌──────────────────────────┐    ║
║  │       GAME OVER          │    ║
║  │  Score: 03,100           │    ║
║  │  [ RETRY ]  [ MENU ]    │    ║
║  └──────────────────────────┘    ║
╚══════════════════════════════════╝
```

### Failure: Didn't Jump
```
╔══════════════════════════════════╗
║  SCORE: 07,850                   ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║  ═══════════════════════════════ ║
║  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ║
║  ═══════════╬═══════════════════ ║
║             │                    ║
║          💥 ●  TRIPPED!         ║
║           ○○○                    ║
║                                  ║
║  ┌──────────────────────────┐    ║
║  │       GAME OVER          │    ║
║  │  Score: 07,850           │    ║
║  │  [ RETRY ]  [ MENU ]    │    ║
║  └──────────────────────────┘    ║
╚══════════════════════════════════╝
```

---

## 10. DIFFICULTY PROGRESSION

```
DIFFICULTY CURVE
================

Speed
 ▲
 │                                        ╭────── INSANE
 │                                   ╭────╯
 │                              ╭────╯
 │                         ╭────╯  ← obstacles start
 │                    ╭────╯         stacking/comboing
 │               ╭────╯
 │          ╭────╯    ← new obstacle types unlock
 │     ╭────╯
 │╭────╯
 │╯  ← gentle start, single obstacles
 ┼────────────────────────────────────────────────► Time
 0s    30s    60s    90s   120s   150s   180s+


WHAT CHANGES OVER TIME:
═══════════════════════

 TIME        SPEED    OBSTACLES           BURNOUT WINDOW
 ──────────  ───────  ──────────────────  ──────────────
 0-30s       ×1.0     Single shape gates  Very generous
                      spread far apart     (easy to burn)

 30-60s      ×1.3     + Lane pushes        Generous
                      + Low bars

 60-90s      ×1.6     + High bars          Moderate
                      + Closer spacing

 90-120s     ×2.0     + Combo obstacles    Tighter
                      + 2-obstacle chains

 120-150s    ×2.3     + Split paths        Tight
                      + 3-obstacle chains

 150-180s    ×2.6     + Rapid sequences    Very tight
                      everything mixed

 180s+       ×3.0     Full chaos            Razor thin
              max     pure skill zone       "frame-perfect"


OBSTACLE INTRODUCTION TIMELINE:
═══════════════════════════════

  0s         30s        60s        90s        120s
  │          │          │          │          │
  ├──●■▲─────┤          │          │          │
  │  shape   ├──pillar──┤          │          │
  │  gates   │  low bar ├──hi bar──┤          │
  │  only    │          │          ├──combos──┤
  │          │          │          │  splits  ├──rapid──→
  │          │          │          │          │  chains
```

### Side-by-Side: Early vs Late Game

```
EARLY GAME (×1.0)                 LATE GAME (×2.5)
One obstacle, lots of space       Obstacles stacked tight

╔════════════════════╗            ╔════════════════════╗
║                    ║            ║   ╔════╗           ║
║   ╔══════════╗     ║            ║   ║/\██║███████████║
║   ║███╭──╮███║     ║            ║   ╚════╝           ║
║   ║███│  │███║     ║            ║      ╔═══╗         ║
║   ║███╰──╯███║     ║            ║      ║ X ║         ║
║   ╚══════════╝     ║            ║      ╚═══╝         ║
║        :           ║            ║   ═══════════════  ║
║        :           ║            ║   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓  ║
║        :           ║            ║   ═══════════════  ║
║        :           ║            ║        :           ║
║        :           ║            ║        :           ║
║        ■           ║            ║        ●           ║
║                    ║            ║                    ║
╚════════════════════╝            ╚════════════════════╝
 "I can chill and                  "OHGODOHGODOHGOD"
  build burnout"                    no time to burn!
                                    survive = the goal
```

---

## 11. SCORING & UI DETAILS

### Score Breakdown:

```
╔══════════════════════════════════════════════╗
║              SCORING SYSTEM                  ║
╠══════════════════════════════════════════════╣
║                                              ║
║  BASE POINTS PER OBSTACLE:                   ║
║  ─────────────────────────                   ║
║  Shape Gate .......... 200 pts               ║
║  Lane Dodge .......... 100 pts               ║
║  Jump / Slide ........ 100 pts               ║
║  Combo Gate .......... 200 pts               ║
║  Split Path .......... 300 pts               ║
║                                              ║
║  BURNOUT MULTIPLIER:                         ║
║  ──────────────────                          ║
║  Switch immediately .. ×1.0 (no bonus)       ║
║  Wait a bit ......... ×1.5                   ║
║  Wait longer ........ ×2.0                   ║
║  Danger zone ........ ×3.0                   ║
║  Last possible frame . ×5.0 (MAX)            ║
║  Too late ........... 💀 DEAD                ║
║                                              ║
║  CHAIN BONUS:                                ║
║  ────────────                                ║
║  2 obstacles in a row: +50 flat              ║
║  3 obstacles in a row: +100 flat             ║
║  4 obstacles in a row: +200 flat             ║
║  5+ obstacles .......: +100 per extra        ║
║                                              ║
║  COMBO BONUS:                                ║
║  ────────────                                ║
║  When obstacle needs 2 actions:              ║
║  Final score = base × avg(burnout) × 2       ║
║                                              ║
║  DISTANCE BONUS:                             ║
║  ──────────────                              ║
║  +10 pts per second survived                 ║
║  (passive income, encourages survival)       ║
║                                              ║
╚══════════════════════════════════════════════╝
```

### HUD Layout:

```
╔══════════════════════════════════╗
║ ┌────────────────────────────┐  ║
║ │ 🏆 04,720     ◆ BEST 12.3K│  ║  ← top bar
║ └────────────────────────────┘  ║
║ ┌────────────────────────────┐  ║
║ │ SPEED ████████░░░░ ×1.8   │  ║  ← speed indicator
║ └────────────────────────────┘  ║
║                                  ║
║                                  ║
║       ~~ gameplay area ~~        ║
║                                  ║
║                                  ║
║                                  ║
║                                  ║
║ ┌────────────────────────────┐  ║
║ │ BURNOUT ░░░░░▓▓▓███░░░░░  │  ║  ← burnout meter
║ ├────────────────────────────┤  ║
║ │                            │  ║
║ │  [ ● ]    [■■■]    [ ▲ ]  │  ║  ← shape buttons
║ │  circle   ACTIVE   triangle│  ║     (■ is selected)
║ │                            │  ║
║ └────────────────────────────┘  ║
╚══════════════════════════════════╝
```

### Burnout Popup Feedback:

```
  When player banks burnout points, a popup appears:

  ×1.0  →  (nothing, too easy)
  ×1.5  →  "Nice"          (small text, subtle)
  ×2.0  →  "GREAT!"        (medium text, yellow)
  ×3.0  →  "CLUTCH!"       (large text, orange, shake)
  ×4.0  →  "INSANE!!"      (huge text, red, screen flash)
  ×5.0  →  "LEGENDARY!!!"  (massive, rainbow, explosion)

  Visual:

     ×2.0             ×4.0             ×5.0
                    ✦         ✦
    GREAT!        ✦ INSANE!! ✦     ★✦ LEGENDARY!!! ✦★
                    ✦         ✦      ✦★  ★✦  ★✦  ✦★
     (yellow)       (red+shake)      (rainbow+explosion)
```

---

## APPENDIX: FULL GAME FLOW

```
  ┌─────────┐
  │  TITLE  │
  │  SCREEN │
  └────┬────┘
       │
       ▼
  ┌─────────┐     ┌───────────┐
  │  TAP TO  │────▶│  RUNNING  │◄──────────────────┐
  │  START   │     │  (auto)   │                    │
  └─────────┘     └─────┬─────┘                    │
                        │                           │
                        ▼                           │
                  ┌───────────┐                     │
                  │ OBSTACLE  │                     │
                  │ DETECTED  │                     │
                  └─────┬─────┘                     │
                        │                           │
                   ┌────┴────┐                      │
                   ▼         ▼                      │
             ┌──────────┐ ┌──────────┐              │
             │ BURNOUT  │ │  PLAYER  │              │
             │  METER   │ │  ACTS    │              │
             │  FILLS   │ │ (tap/    │              │
             │    ...   │ │  swipe)  │              │
             └────┬─────┘ └────┬─────┘              │
                  │            │                     │
             ┌────┴────┐  ┌───┴────┐                │
             ▼         │  ▼        │                │
        ┌────────┐     │ ┌───────┐ │                │
        │  💀    │     │ │POINTS │ │                │
        │ DEAD!  │     │ │SCORED │ │                │
        │ meter  │     │ │(base× │ │                │
        │ maxed  │     │ │ burn) │ │                │
        └───┬────┘     │ └──┬────┘ │                │
            │          │    │      │                │
            ▼          │    └──────┼───────────────▶┘
       ┌─────────┐     │           │
       │  GAME   │◄────┘           │
       │  OVER   │                 │
       └────┬────┘                 │
            │                      │
       ┌────┴────┐                 │
       ▼         ▼                 │
  ┌────────┐ ┌────────┐           │
  │ RETRY  │ │  MENU  │           │
  │(restart│ │(title) │           │
  └────────┘ └────────┘           │
```

---

## DESIGN NOTES

```
  ┌──────────────────────────────────────────────────┐
  │  KEY PLAYER EMOTIONS BY PHASE:                   │
  │                                                  │
  │  EARLY GAME    →  "I'm learning, this is fun"    │
  │                    confidence building            │
  │                                                  │
  │  MID GAME      →  "I'm GREEDY for burnout pts"   │
  │                    risk/reward dopamine loop      │
  │                                                  │
  │  LATE GAME     →  "JUST SURVIVE"                 │
  │                    pure adrenaline, flow state    │
  │                                                  │
  │  DEATH         →  "ONE MORE TRY"                 │
  │                    instant restart, no friction   │
  │                                                  │
  │  The burnout mechanic creates a SKILL CEILING:   │
  │  • Beginners switch early (safe, low score)      │
  │  • Good players push the burnout (higher score)  │
  │  • Masters dance on the edge (legendary scores)  │
  │                                                  │
  │  This means two players at the same distance     │
  │  can have WILDLY different scores. That's the    │
  │  leaderboard hook.                               │
  └──────────────────────────────────────────────────┘
```

---

*End of prototype. This document is the source of truth for implementation.*
