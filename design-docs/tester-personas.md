# Tester Personas — Design Specification
## Sr Game Designer Deliverable · v1.0

> Automated test players that simulate human skill levels to validate
> level feel, timing windows, and scoring balance in rhythm mode.
>
> **Tech Spec**: See `tester-personas-tech-spec.md` for implementation details.

```
  ┌─────────────────────────────────────────────────────────┐
  │  PERSONA 1: PRO Player .......................... p.1   │
  │  PERSONA 2: GOOD Player ......................... p.2   │
  │  PERSONA 3: BAD Player .......................... p.3   │
  │  ACTIVATION & OUTPUT ........................... p.4    │
  └─────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# DESIGN PILLARS
# ═══════════════════════════════════════════════════

```
  1. HUMAN-LIKE    — limited by reaction time, not omniscient
  2. OBSERVABLE    — every decision logged alongside game events
  3. SKILL-GATED   — same brain, different reflexes
  4. NON-INVASIVE  — uses existing input pipeline, no game code changes
```

**Core Question These Personas Answer:**
> "Is this level fun and clearable at each intended skill tier?"

---
---
---

# ═══════════════════════════════════════════════════
# PERSONA 1 — PRO PLAYER
# ═══════════════════════════════════════════════════

## Identity

```
  ╔═══════════════════════════════════════════╗
  ║  🎯  PRO PLAYER                          ║
  ║  "The rhythm game veteran"               ║
  ║                                           ║
  ║  Reaction:  300 – 500 ms                 ║
  ║  Vision:    800 px  (sees early)         ║
  ║  Strategy:  Aim for Perfect timing       ║
  ║  Goal:      Maximize score               ║
  ╚═══════════════════════════════════════════╝
```

## Behavior Profile

- **Sees obstacles ~1.5s before collision** (at 120 BPM)
- **Always knows the correct action** — never picks wrong shape/lane
- **Deliberately delays reaction** to land in the Perfect timing window
- Calculates `ideal_press_time` and waits for it
- Has enough margin to absorb RNG variance in reaction time
- Expected outcome: **clears all songs, high scores, mostly Perfects**

## Timing Strategy

```
  Obstacle enters vision at T=0
  │
  │  ┌── 1.14s margin ──────────────────────────────┐
  │  │                                               │
  │  │     idle...    idle...    ★ PRESS (Perfect)   │ collision
  │  │                          ▲                    │ ▲
  │  │                          │                    │ │
  │  │                     ideal_press               │ │
  │  └───────────────────────────────────────────────┘ │
  ▼                                                    ▼
  T=0                                              T≈1.54s

  ideal_press = arrival_time − morph_duration − half_window
  delay = ideal_press − see_time
  if delay > reaction_min → wait (aim for Perfect)
  if delay ≤ reaction_min → react ASAP (still likely Good)
```

## What Pro Validates

- [x] Perfect timing window is achievable with fast reflexes
- [x] Score ceiling feels rewarding (high multipliers)
- [x] No obstacles are physically impossible (always solvable)
- [x] Timing windows reward skilled on-beat play


---
---
---

# ═══════════════════════════════════════════════════
# PERSONA 2 — GOOD PLAYER
# ═══════════════════════════════════════════════════

## Identity

```
  ╔═══════════════════════════════════════════╗
  ║  👍  GOOD PLAYER                         ║
  ║  "The casual-but-experienced gamer"      ║
  ║                                           ║
  ║  Reaction:  500 – 800 ms                 ║
  ║  Vision:    600 px  (medium range)       ║
  ║  Strategy:  React as fast as possible    ║
  ║  Goal:      Clear the level              ║
  ╚═══════════════════════════════════════════╝
```

## Behavior Profile

- **Sees obstacles ~1.15s before collision** (at 120 BPM)
- **Always knows the correct action** — never picks wrong shape/lane
- **Reacts immediately** when reaction timer fires — no deliberate delay
- Timing grade lands wherever RNG reaction puts it (mostly Good/Ok)
- **Tight margins** — survives but rarely has time to spare
- Expected outcome: **clears easy/medium songs, struggles on hard**

## Timing Budget

```
  Obstacle enters vision at T=0
  │
  │  ┌── 0.50s margin ────────┐
  │  │                         │
  │  │  react...  ★ PRESS     │ collision
  │  │            ▲            │ ▲
  │  │            │            │ │
  │  │         500-800ms       │ │
  │  └─────────────────────────┘ │
  ▼                              ▼
  T=0                         T≈1.15s

  No time to aim — timing grade is a natural consequence.
  Typical: 40% Good, 35% Ok, 15% Bad, 10% Perfect (lucky)
```

## What Good Validates

- [x] Level is clearable with moderate reflexes
- [x] Timing windows are forgiving enough for non-experts
- [x] Scoring feels fair (not punishing for Ok/Good hits)
- [x] Difficulty ramp doesn't wall out mid-skill players


---
---
---

# ═══════════════════════════════════════════════════
# PERSONA 3 — BAD PLAYER
# ═══════════════════════════════════════════════════

## Identity

```
  ╔═══════════════════════════════════════════╗
  ║  😅  BAD PLAYER                          ║
  ║  "The first-time rhythm player"          ║
  ║                                           ║
  ║  Reaction:  800 – 1200 ms                ║
  ║  Vision:    400 px  (sees late)          ║
  ║  Strategy:  React ASAP (still late)      ║
  ║  Goal:      Survive                      ║
  ╚═══════════════════════════════════════════╝
```

## Behavior Profile

- **Sees obstacles ~0.77s before collision** (at 120 BPM)
- **Always knows the correct action** — never picks wrong shape/lane
- **Reacts as fast as possible** but reaction takes 0.8–1.2s
- **Often too slow** — reaction exceeds available time → miss → game over
- At faster BPMs or shorter lead times, fails almost immediately
- Expected outcome: **dies on medium+, barely survives easy**

## Timing Budget

```
  Obstacle enters vision at T=0
  │
  │  ┌── NEGATIVE margin! ─────────────────────┐
  │  │                                          │
  │  │  react...  react...  react... ★ TOO LATE │ collision already happened!
  │  │                                ▲         │ ▲
  │  │                                │         │ │
  │  │            800-1200ms          │         │ │
  │  └────────────────────────────────┘         │ │
  ▼                                             ▼ │
  T=0                                        T≈0.77s

  At base speed: 0.77s visible − 1.0s react = −0.23s
  Bad player CANNOT react in time on ~23% of obstacles.
  This is intentional — it's test data telling us "this section is hard."
```

## What Bad Validates

- [x] Tutorial / easy sections are survivable for beginners
- [x] Difficulty curve is appropriate (not punishing too early)
- [x] Death feels fair ("I saw it, I was just too slow")
- [x] HP system provides enough buffer for occasional misses


---
---
---

# ═══════════════════════════════════════════════════
# ACTIVATION & OUTPUT
# ═══════════════════════════════════════════════════

## CLI

```bash
./build/shapeshifter --test-player pro
./build/shapeshifter --test-player good
./build/shapeshifter --test-player bad
```

## Output

```
  Game window opens → Title screen → auto-starts (0.5s delay)
  → AI plays through beatmap → visual playback at 60fps
  → Song ends (or player dies)
  → Log file written to: session_{skill}_{timestamp}.log
```

## Expected Outcomes per Persona

```
  ┌───────────┬───────────┬──────────┬──────────┬──────────────────┐
  │ Persona   │ Easy Song │ Med Song │ Hard Song│ Primary Insight  │
  ├───────────┼───────────┼──────────┼──────────┼──────────────────┤
  │ Pro       │ CLEAR ★★★ │ CLEAR ★★ │ CLEAR ★  │ Score ceiling    │
  │ Good      │ CLEAR ★★  │ CLEAR ★  │ FAIL 60% │ Fairness check   │
  │ Bad       │ CLEAR ★   │ FAIL 40% │ FAIL 90% │ Floor / tutorial │
  └───────────┴───────────┴──────────┴──────────┴──────────────────┘

  ★ = relative score quality (not literal stars)
```

## Level Design Feedback Loop

```
  ┌──────────┐     ┌──────────────┐     ┌──────────────┐
  │ Design   │────▶│ Run 3        │────▶│ Compare Logs │
  │ beatmap  │     │ personas     │     │              │
  └──────────┘     └──────────────┘     └──────┬───────┘
       ▲                                       │
       │           ┌──────────────────────┐    │
       └───────────│ Adjust obstacles,    │◀───┘
                   │ timing, difficulty   │
                   └──────────────────────┘

  Questions the logs answer:
  ─────────────────────────────────────────────
  • "Pro clears with all Perfects → is it too easy?"
  • "Good fails at beat 24 → is that obstacle too fast?"
  • "Bad dies at beat 8 → is the intro too punishing?"
  • "Ring hits Perfect zone 0.5s before Pro presses → lead time is good"
  • "Ring hits Perfect zone 0.1s before Bad presses → lead time too short"
```
