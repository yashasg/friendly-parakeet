# keaton-r14: obstacle_despawn ↔ popup_feedback ordering is incidental

## Pre tail-5 (verbatim)

```
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (190 beats, difficulty=medium)
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (190 beats, difficulty=medium)
All tests passed (2255 assertions in 786 test cases)
```

## Investigation: coupling analysis

Read all relevant systems end-to-end. Verdict: **the two systems are commutative**.

### Data surfaces

| System | Reads | Writes |
|---|---|---|
| `obstacle_despawn_system` | `ObstacleTag+ObstacleScrollZ` (ctx: `GameCamera`) → destroys entities past boundary; `ObstacleTag+Position` → same | entity destruction only |
| `popup_feedback_system` | `ScorePopupRequestQueue` (ctx variable) | spawns `ScorePopup` entities, clears queue, pushes `AudioQueue` |

These surfaces are **fully disjoint**. No component type appears in both read/write sets.

### Why there is no observable ordering dependency

`scoring_system` runs **inside** `tick_playing_systems` (line 17, `fixed_tick_runner.cpp`). By the time control returns from `tick_playing_systems`:

1. All `ScoredTag` entities have had `ScoredTag` + `Obstacle` + `TimingGrade` removed — `scoring_system.cpp:111–113,206–207`.
2. `ScorePopupRequestQueue` is fully populated — `scoring_system.cpp:202`.

So when `obstacle_despawn_system` (line 20) runs, there are no `ScoredTag` entities remaining. Despawn only destroys entities that have scrolled past the boundary (`oz.z > camera_despawn_z` or `pos.y > DESTROY_Y`) — regardless of any scoring state.

When `popup_feedback_system` (line 27) runs, it reads only `ScorePopupRequestQueue` — a ctx variable already fully populated. It creates popup entities. It never touches `ObstacleTag`, `ObstacleScrollZ`, `Position`, or anything that despawn wrote.

**Swapping lines 20 and 27 in `fixed_tick_runner.cpp` produces zero observable state difference.**

The existing test comment at `test_phase_runner.cpp:73–78` (before r14) had already acknowledged this:
> "because popup_feedback reads from a pre-populated queue (not live obstacle entities), wrong ordering does not produce a visible crash"

### Line cites

- `fixed_tick_runner.cpp:17,20,27` — call order under analysis
- `scoring_system.cpp:55–60` — `popup_queue_for` helper, populates queue **inside** `tick_playing_systems`
- `scoring_system.cpp:202` — `popup_queue.requests.push_back(...)` — queue fully populated before despawn/feedback run
- `scoring_system.cpp:111–113,206–207` — `ScoredTag` removed before despawn/feedback run
- `obstacle_despawn_system.cpp:44–50` — reads `ObstacleTag+ObstacleScrollZ`, destroys by threshold; no queue touch
- `obstacle_despawn_system.cpp:53–59` — reads `ObstacleTag+Position`, same
- `popup_feedback_system.cpp:10–18` — reads only `ScorePopupRequestQueue`; no obstacle component touch

## Decision: no ordering regression test warranted

A test that fails when the two lines are swapped cannot be written because swapping them produces no observable state change. Writing a fake dependency to force failure would be dishonest and misleading.

The correct action is to:
1. Update the comment in `fixed_tick_runner.cpp` from "semantic invariant" language to "cache-locality preference" language.
2. Update the test comment in `test_phase_runner.cpp` to reflect the actual invariant (wiring placement, not relative call order).

## Changes made

### `app/systems/fixed_tick_runner.cpp` (lines 18–26)

Old comment claimed a "post-despawn world state invariant" and told reviewers not to move systems due to ordering. New comment correctly describes:
- The order between `obstacle_despawn` and `popup_feedback` is a cache-locality preference, NOT a semantic invariant.
- These systems are commutative (disjoint data surfaces).
- The real invariant that must not be violated: popup_feedback/energy must stay OUTSIDE `tick_playing_systems` (so scoring_system runs first and populates the queue).

### `tests/test_phase_runner.cpp` (test comment block)

Updated to accurately describe:
- What the test actually guards (wiring, not call sequence)
- Why relative ordering between despawn and popup_feedback is not a semantic invariant (Keaton-r14 finding)
- The real placement invariant (outside tick_playing_systems)

## Fail-then-fix protocol: N/A

No ordering regression test was written because no ordering dependency exists. The Fail-then-fix protocol only applies when a real dependency is confirmed and a test is created. Fabricating a false dependency to produce a test would violate the protocol's intent ("Do NOT invent a fake dependency just to make a test fail").

## vel_view path (#349): not shipped this round

Investigated scope. The `Velocity` component is still used in:
- `obstacle_entity.cpp:11` — `spawn_obstacle` creates with `Velocity`
- `scroll_system.cpp:39` — `ObstacleTag+ObstacleScrollZ+Velocity` view
- `test_helpers.h:175,192,209,228,246,264` — 6 obstacle helper functions emit `Velocity`
- `test_obstacle_archetypes.cpp:54,234,248,270,289,304,318,332` — 8 archetype assertions include `Velocity`
- Plus scattered test files

This is not a small surgical change. Deferring to a dedicated round with an explicit migration plan.

## Bench summary

No system changes made this round (comment-only edits). No benchmark delta expected or observed. Full bench run completed; all within normal variance.

## Post tail-5 (verbatim)

```
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (137 beats, difficulty=easy)
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (190 beats, difficulty=medium)
All tests passed (2255 assertions in 786 test cases)
```

**786 tests / 2255 assertions. Zero warnings. No new tests (none warranted).**
