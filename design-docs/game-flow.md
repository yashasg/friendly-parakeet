# SHAPESHIFTER — Game Flow & UX Specification
## v1.0 — The Definitive Player Experience Blueprint

> This document defines **every screen, every transition, every piece of feedback**
> the player experiences from first launch to 100th death.
> If it's on screen, it's in here. If the player feels it, it's in here.

> **Current model:** the removed Burnout risk/reward mechanic is archived
> history only. The HUD uses the **proximity ring** around shape buttons
> (`rhythm-spec.md` §6) as the live timing cue and the **energy bar**
> (`energy-bar.md`) as the survival meter. Scoring is on-beat timing grades
> (Perfect/Good/Ok/Bad) × chain.

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

> **Source of truth:** the active phase is carried by per-phase ctx tags
> (`GamePhaseTitleTag` / `GamePhaseLevelSelectTag` / `GamePhasePlayingTag` /
> `GamePhasePausedTag` / `GamePhaseGameOverTag` / `GamePhaseSongCompleteTag` /
> `GamePhaseSettingsTag` / `GamePhaseTutorialTag` per `app/tags/tags.h`);
> the active phase is the lone present `GamePhase*Tag` on the registry
> ctx. Transitions live in `app/systems/game_state_routing.cpp` (per-event
> handlers that call `request_phase_transition<NextPhase*Tag>`),
> `app/systems/game_phase_transition.{h,cpp}` (the swap mechanic), and
> `app/systems/screen_lifecycle_system.{h,cpp}` (per-tag spawn/despawn
> dispatch into `app/systems/generated/screen_spawners.h`).
> The diagram below mirrors current shipped routing.

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
                     │   │ on energy=0   │ song  │ [RESUME]    │   │
                     │   │               │ ends  │ [MAIN MENU]─┼───┤
                     │   ▼               ▼       └─────────────┘   │
                     │ ┌──────────┐ ┌──────────┐                   │
                     │ │ GAMEOVER │ │   SONG   │
                     │ │  SCREEN  │ │ COMPLETE │
                     │ │          │ │  SCREEN  │
                      │ │ ● score  │ │          │
                      │ │ ● reason │ │ ● score  │
                      │ │[RESTART] │ │ ● stats  │
                      │ │[LEVELS]  │ │[RESTART] │
                      │ │[MAIN]    │ │[LEVELS]  │
                      │ │          │ │[MAIN]    │
                     │ └────┬─────┘ └────┬─────┘
                     │      │            │
                     └──────┴────────────┘
                          (back to Title)
```

> **Tutorial phase.** `GamePhaseTutorialTag` is reached from Level Select
> when `SettingsState::ftue_run_count == 0`. Pressing START on the
> tutorial marks FTUE complete, persists settings, and starts gameplay.
> Completed FTUE profiles skip Tutorial and go directly to Playing.

### State Enumeration (for ECS implementation)

> **Source of truth:** `app/tags/tags.h` § "Game phase mirrors"
> (`GamePhase*Tag` set). If this list drifts from the header, the
> header wins.

```cpp
struct GamePhaseTitleTag        {};   // main menu
struct GamePhaseLevelSelectTag  {};   // song/difficulty selection
struct GamePhasePlayingTag      {};   // core gameplay
struct GamePhasePausedTag       {};   // overlay on gameplay
struct GamePhaseGameOverTag     {};   // results screen
struct GamePhaseSongCompleteTag {};   // song finished successfully
struct GamePhaseSettingsTag     {};   // settings screen (entered from Title gear)
struct GamePhaseTutorialTag     {};   // FTUE/tutorial run before first gameplay
```

The active phase is the lone `GamePhase*Tag` present on `reg.ctx()` —
exactly one is present at any time. A parallel `NextPhase*Tag` set (same
names with `NextPhase` prefix) holds the pending-transition target; see
`app/systems/game_phase_transition.h` for the `enter_phase<...>()` /
`request_phase_transition<...>()` / `is_phase_transition_pending()` API.
```

Shipped transitions (input events flow through dispatcher sinks in
`app/systems/game_state_routing.cpp`, which call
`request_phase_transition<NextPhase*Tag>`; the actual swap runs in
`game_state_system` on the next tick per the deferred-transition
pattern of issue #482):

- `Title → LevelSelect` — confirm/body tap (`game_state_handle_confirm`).
- `Title → Settings` — gear-icon tap (settings entrypoint event).
- `Settings → Title` — back action.
- `LevelSelect → Tutorial` — song selection when FTUE is incomplete.
- `LevelSelect → Playing` — song selection after FTUE is complete.
- `Playing ↔ Paused` — pause button / resume (any direction unpauses).
- `Paused → Title` — quit action.
- `Playing → GameOver` — energy reaches zero (`energy_system` requests it).
- `Playing → SongComplete` — song reaches end (`game_state_terminal_phase_system`).
- `GameOver | SongComplete → Playing` — restart end-screen choice.
- `GameOver | SongComplete → LevelSelect` — level-select end-screen choice.
- `GameOver | SongComplete → Title` — main-menu end-screen choice.
- `Tutorial → Playing` — `game_state_handle_confirm` marks FTUE complete and begins the run.

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
**LevelSelect** screen (active `GamePhaseLevelSelectTag`). This screen
presents a list of available songs and difficulty options. The layout
is defined in `content/ui/screens/level_select.rgl`. Confirming a
selection transitions to `GamePhaseTutorialTag` when FTUE is incomplete;
otherwise it transitions directly to `GamePhasePlayingTag`.

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
- **Reset Tutorial [ RESET ]** — `ftue_run_count` / `mark_ftue_complete` plumbing exists in `app/entities/settings.{h,cpp}` (the `settings::` namespace), but no UI surface ships. If this is still desired, file a separate feature issue rather than implying it ships here.

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
  ║  ├─ Pause button      ║  x:0.861W y:0.008H║ 0.111W × 0.039H║
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
  ║          TAP RESUME TO CONTINUE      ║
  ║        ╔══════════════════╗          ║
  ║        ║     RESUME       ║          ║     button: 400×100 px in .rgl
  ║        ╚══════════════════╝          ║
  ║                                      ║
  ║          OR RETURN TO MAIN MENU      ║
  ║                                      ║
  ║        ┌──────────────────┐          ║
  ║        │   MAIN MENU      │          ║     button: 400×100 px in .rgl
  ║        └──────────────────┘          ║
  ║                                      ║
  ║                                      ║
  ║                                      ║
  ╚══════════════════════════════════════╝

  Trigger: tap ⏸ icon at top-center during gameplay
  Actions: RESUME returns to the paused run; MAIN MENU returns to Title.
```

---

### 2d. GAME OVER SCREEN

> **Trigger:** `Playing → GameOver` occurs only when energy reaches 0.
> A MISS drains energy and resets chain, but a miss is not itself a
> terminal transition.
>
> The shipped Game Over screen currently renders final score, high score,
> and death reason with `RESTART`, `LEVEL SELECT`, and `MAIN MENU` buttons;
> the larger stat panel below is future scope.

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
  ║     │  SHIPPED SUMMARY           │   ║
  ║     │  ─────────────────────     │   ║
  ║     │                            │   ║
  ║     │  High Score    31,420      │   ║
  ║     │  End Reason    Energy 0    │   ║
  ║     │  Detailed stats: future    │   ║
  ║     │                            │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║  ← y = 0.78H
  ║                                      ║
  ║     ╔════════════════════════════╗   ║  ← y = 0.82H
  ║     ║     ▸ RESTART             ║   ║     PRIMARY button
  ║     ╚════════════════════════════╝   ║     0.80W × 0.08H
  ║                                      ║
  ║     ┌────────────────────────────┐   ║  ← y = 0.92H
  ║     │      LEVEL SELECT         │   ║     secondary button
  ║     └────────────────────────────┘   ║
  ║     ┌────────────────────────────┐   ║
  ║     │       MAIN MENU            │   ║     secondary button
  ║     └────────────────────────────┘   ║     0.80W × 0.05H
  ║                                      ║
  ╚══════════════════════════════════════╝
```

#### Stats Breakdown — What Each Row Shows

```
  ╔══════════════════════════════════════════════════════╗
  ║  SHIPPED ROW        ║  SOURCE             ║ FORMAT  ║
  ╠══════════════════════╬══════════════════════╬═════════╣
  ║  Final Score         ║  SongResults.score   ║ "24850" ║
  ║  High Score          ║  ScoreState.best     ║ "31420" ║
  ║  Death Reason        ║  GameOverState.cause ║ text    ║
  ╚══════════════════════╩══════════════════════╩═════════╝

  HIGHLIGHT RULES:
  ─────────────────
  • If new personal best    → "◆ NEW BEST! ◆" rainbow text
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
  ║     │      CLEAR BONUS APPLIED   │   ║  ← future; not yet shipped
  ║     │                            │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ║     ┌────────────────────────────┐   ║
  ║     │  RESULTS                   │   ║
  ║     │  ─────────────────────     │   ║
  ║     │  Perfect      42           │   ║
  ║     │  Good         18           │   ║
  ║     │  Ok            7           │   ║
  ║     │  Bad           1           │   ║
  ║     │  Miss          3           │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ║     ╔════════════════════════════╗   ║
  ║     ║     ▸ RESTART             ║   ║
  ║     ╚════════════════════════════╝   ║
  ║                                      ║
  ║     ┌────────────────────────────┐   ║
  ║     │      LEVEL SELECT         │   ║
  ║     └────────────────────────────┘   ║
  ║     ┌────────────────────────────┐   ║
  ║     │       MAIN MENU            │   ║
  ║     └────────────────────────────┘   ║
  ║                                      ║
  ╚══════════════════════════════════════╝
```

Terminal split:
- `energy <= 0` before the song ends → Game Over.
- Song duration reached with energy remaining → Song Complete.

---

## 3. FIRST-TIME USER EXPERIENCE (FTUE)

> **Implementation status:** the shipped tutorial is the single-screen
> **Quick Start** flow (active `GamePhaseTutorialTag`, `content/ui/screens/tutorial.rgl`).
> The five-run adaptive FTUE design below is aspirational/archived and is not
> routed by the current controllers.

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

The shipped onboarding surface is the **Quick Start** tutorial screen
(`content/ui/screens/tutorial.rgl`). The older five-run adaptive FTUE plan
below is archived design intent only: `ftue_run_count` may exist in settings
storage, but no shipped controller routes players through Tutorial Run 1-5.

---

### TUTORIAL RUN 1 — "Match the Shape" (unimplemented archived design)

**Goal:** Teach shape matching. Only ■ gates appear. Only ■ button visible.

```
  WHAT'S DIFFERENT:
  ─────────────────
  • Only SQUARE gates spawn (no other obstacles)
  • Only [ ■ ] button visible (● and ▲ hidden)
  • Player starts as ● (wrong shape on purpose)
  • Speed is VERY slow (×0.6)
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

> ⚠️ **NOT SHIPPED — lane_push tutorial content (historical context:
> #328, #470, both closed).** The "Dodge!" run below teaches a `lane_push`
> obstacle that automatically shoves the player one lane on beat arrival.
> **No `lane_push` obstacle ships in the runtime today** — `LanePush` was
> removed from the runtime, content, and tools (#328) and no shipped
> beatmap (`content/beatmaps/`) emits one. The shipped tutorial therefore
> graduates from Run 2 ("Two Shapes") directly to Run 4 ("Stay on the
> Beat"); shape_gate is the only obstacle the tutorial currently teaches.
> The frames below are retained as forward-design context for if/when a
> committed lane-push (or successor dodge mechanic) plan lands. Do not
> treat them as a description of current player-facing tutorial content.


```
  WHAT'S DIFFERENT:
  ─────────────────
  • All 3 shape buttons visible
  • Speed: ×0.8
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
  "Pressing while the ring is tight = PERFECT"
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

  The ring is the live timing cue: tight = inside the PERFECT timing window.
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

```
  ╔══════════════════════════════════════════════════╗
  ║  RUN  ║ CONCEPT        ║ MECHANICS ACTIVE        ║
  ╠═══════╬════════════════╬═════════════════════════╣
  ║   1   ║ Match shape    ║ ■ gates, 1 button       ║
  ║   2   ║ Switch shapes  ║ ●■ gates, 2 buttons     ║
  ║   4   ║ Stay on beat   ║ proximity ring + grades  ║
  ║   5+  ║ FULL GAME      ║ Everything               ║
  ╚═══════╩════════════════╩═════════════════════════╝

  TEACHING CADENCE:
  ═════════════════

  Complexity
     ▲
     │                                    ┌─── Full game
     │                               ┌────┘
     │                          ┌────┘  Beat timing
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

### 4b. OBSTACLE CLEAR (Passing through gate / dodging block)

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

### 4c. CHAIN BUILDING (Consecutive obstacle clears)

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

### 4d. GAME OVER — Death Sequence

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

### 4e. HIGH SCORE CELEBRATION

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

### 4f. LANE SWITCH (Player swipes left/right)

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

### 4g. JUMP / SLIDE (Player swipes up/down)

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

> The shipped HUD has a vertical left-edge Energy Bar, Score/Best Score,
> Pause Button, Shape Buttons, Lane Dividers, and Player Avatar. Speed Bar
> and Chain Counter are not currently rendered.

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
  ║ Energy Bar (left edge) ║       ║  ✓      ║  ✓ dim ║ ✓ grey║        ║
  ║ Speed Bar              ║       ║         ║        ║       ║        ║  ← removed
  ║ Pause Button (⏸)       ║       ║  ✓      ║        ║       ║        ║
  ║ Shape Buttons          ║       ║  ✓      ║  ✓ dim ║       ║        ║
  ║ Lane Dividers          ║       ║  ✓      ║  ✓ dim ║ ✓ fade ║       ║
  ║ Player Avatar          ║       ║  ✓      ║  ✓ dim ║ shatter║       ║
  ║ Chain Counter          ║       ║         ║        ║       ║        ║  ← not shipped
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

### 5a. SCORE COUNTER — Animation Specification

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

### 5b. SPEED BAR — Animation Specification

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

> Transition timing is current; HUD art should use the current Energy Bar and
> proximity-ring model from `energy-bar.md` and `rhythm-spec.md` §6.

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
  ║                    ║    ║  ENERGY █████░░░░  ║
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
  ║  [ MENU ]         ║   ║                   ║  ║ ENERGY █████░░░░  ║
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
  t=0.18s  • Main Menu button slides in from left (0.10s)
  t=0.25s  • All elements visible, input enabled

  EXIT (tap RESUME):
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
  ║      │     │      ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓ [ RESUME ]   ▓▓║
  ║      │  ●  │      ║  ║▓▓▓(dimming)▓▓▓▓▓▓║  ║▓▓              ▓▓║
  ║      │     │      ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓ [MAIN MENU]  ▓▓║
  ║ ─────┴─────┴────  ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
  ║ ENERGY █████░░░░  ║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║  ║▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓║
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
  P0         Score counter (basic)   CRITICAL  Low
  P0         Retry (< 0.5s)         CRITICAL  Low
  ─────────────────────────────────────────────────────
  P1         Death sequence          HIGH      Medium
  P1         Button pop animation    HIGH      Low
  P1         Gate pass-through FX    HIGH      Medium
  P1         Lane switch animation   HIGH      Low
  ─────────────────────────────────────────────────────
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
