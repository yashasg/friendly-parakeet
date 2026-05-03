# Keaton r18 — double-emplace fix in bench_systems.cpp

## Pre tail-5 (verbatim)

```
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/3_mental_corruption_beatmap.json (189 beats, difficulty=hard)
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/1_stomper_beatmap.json (190 beats, difficulty=medium)
All tests passed (2235 assertions in 785 test cases)
```

## Option chosen: A — bench_systems.cpp:58-59 double-emplace bug

**Rationale:** Keyser-r17 explicitly flagged this as a pre-existing correctness issue. Options B/C require perf investigation or grep audits that may yield nothing actionable. Option D is a Scribe correction, not mine to ship. Option A is surgical and unambiguous.

## Investigation evidence

`benchmarks/bench_systems.cpp:55-67` — `spawn_obstacles()` creates an obstacle entity `obs`, then:

```cpp
// line 58
reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[i % 3], y}});
// line 59 — DUPLICATE, identical arguments
reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[i % 3], y}});
```

Both calls are byte-for-byte identical. In EnTT v3.x, emplacing a component that already exists on an entity is undefined behaviour (debug builds assert, release builds silently overwrite or crash depending on pool state). Non-crashing here only because:
1. EnTT's default storage uses a dense set; the second emplace resolves to `replace` in practice on this build.
2. The values are identical so the observable result is unchanged.

Still a latent bug: a future EnTT upgrade or pool configuration change could make it assert/abort.

## Diff summary

`benchmarks/bench_systems.cpp` — removed line 59 (exact duplicate of line 58):

```diff
-        reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[i % 3], y}});
-        reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[i % 3], y}});
+        reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[i % 3], y}});
```

One line deleted. No logic change.

## Bench numbers (motion_system, post-fix)

```
10 entities      26.3817 ns   (low 26.3035 ns, high 26.4745 ns)
100 entities    178.708 ns   (low 175.582 ns, high 181.985 ns)
```

Within noise of r17 baseline (26.6 ns for 10 ents). Duplicate emplace had no measurable effect — consistent with the "non-crashing" characterisation.

## Post tail-5 (verbatim)

```
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/3_mental_corruption_beatmap.json (157 beats, difficulty=medium)
INFO: Loaded beatmap: /Users/yashasgujjar/dev/bullethell/build/content/beatmaps/3_mental_corruption_beatmap.json (189 beats, difficulty=hard)
All tests passed (2235 assertions in 785 test cases)
```

Test count: 2235 assertions / 785 test cases — unchanged. ✅  
Build: zero warnings (Werror). ✅

## Diminishing returns observation

Honest assessment: this is busy work. Removing one duplicate line in a benchmark helper has zero gameplay impact, zero perf impact, and zero test coverage gained. It is a correctness hygiene fix — the codebase is marginally cleaner. The loop is genuinely at diminishing returns; r18's value is "we didn't leave a known bug documented but unfixed." That's maintenance discipline, not forward progress. If r19 has no higher-leverage target from Keyser's audit, the loop should conclude.
