# Rabin — Onset-Aligned Beatmap Playability Review

Date: 2026-05-09T13:14:28-07:00
Branch: `beatmap/onset-obstacle-spike` @ `e80211a`
Scope: chart/playability review of the three shipped beatmaps after the
onset spike. **No code or shipped beatmap edits.**

## Sources inspected

- `content/beatmaps/{1_stomper,2_drama,3_mental_corruption}_beatmap.json`
- `tools/diagnostics/onset_spike/<song>/snap_diagnostics_summary.json`

Quick recompute commands (kept here so the next loop can re-run them):

```bash
# Per-difficulty shape & lane distribution
for f in 1_stomper 2_drama 3_mental_corruption; do
  for d in easy medium hard; do
    jq -r --arg d "$d" '
      .difficulties[$d].beats as $b
      | "\($d): n=\($b|length)  shapes=\($b|map(.shape)|group_by(.)|map({k:.[0],n:length}))  lanes=\($b|map(.lane)|group_by(.)|map({k:.[0],n:length}))"
    ' content/beatmaps/${f}_beatmap.json
  done
done

# Long same-shape runs at hard
jq -r '
  .difficulties.hard.beats as $b
  | reduce range(0; $b|length) as $i ({prev:null,cur:0,max:0,runs:[]};
      ($b[$i].shape) as $s
      | if $s == .prev then .cur += 1
        else (if .cur >= 5 then .runs += [.cur] else . end) | .prev = $s | .cur = 1 end
      | .max = ([.max,.cur] | max))
  | "max=\(.max) runs>=5=\(.runs)"' content/beatmaps/1_stomper_beatmap.json
```

## Top-line metrics (from diagnostics + recomputed)

| Song / diff | obstacles | onset src % | median IOI ms | p90 IOI ms | longest same-shape run | runs ≥ 3 | shape mix (c/sq/tri) | lane mix |
|---|---|---|---|---|---|---|---|---|
| stomper / easy   | 178 | 4%  |  755 | 1879 | **40** | 110 | 32 / 115 / 31 | 0:32 / 1:115 / 2:31 |
| stomper / medium | 154 | 6%  | 1125 | 1801 | **30** | 114 | 30 / 83 / 41  | matches shapes |
| stomper / hard   | 196 | 5%  | 1074 | 1502 | **42** | 164 | 11 / 135 / 50 | 11 / 135 / 50 |
| drama / easy     | 143 | 6%  | 1379 | 2772 | 11 | 71  | 25 / 92 / 26  | matches shapes |
| drama / medium   | 145 | 7%  | 1357 | 3229 | 13 | 95  | 27 / 78 / 40  | matches shapes |
| drama / hard     | 208 | 8%  |  929 | 1843 | **21** | 150 | 50 / 103 / 55 | matches shapes |
| mental / easy    | 110 | 14% |  806 | 4797 | 12 | 56  | 43 / 65 / **2**   | matches shapes |
| mental / medium  | 191 | 19% |  806 | 1995 | 10 | 109 | 38 / 105 / 48 | matches shapes |
| mental / hard    | 211 | 28% |  806 | 1597 | **43** | 182 | 64 / 93 / 54  | matches shapes |

`subdivision_label` = `downbeat` for **100% of obstacles in every chart**
(see strict gate failure on label coverage). The eighth/triplet structure
*is* present in source events (e.g. mental eighth_grid: 240 db / 226
eighth) but is not propagated to per-obstacle labels.

## Findings

### F1 — Shape ↔ lane is 1:1 (chart is unreadable as a 3-lane chart)

Every event in every chart sets `lane == shape_index` (circle→0, square→1,
triangle→2). The "longest same-shape run" metric therefore equals
"longest same-lane run." Combined with shape skew toward `square`
(stomper hard: 135/196 = **69 %** of obstacles in lane 1), the player is
parked in one lane for long stretches.

Worst offenders measured directly off the JSON:

- **stomper / hard**: same-shape runs `[38, 5, 42, 15, 13, 14, 27]`.
  A 42-event monochrome run @ ~1 s IOI = ~42 s of identical lane.
- **mental / hard**: longest run = 43.
- **stomper / easy** also has a run of 40 (FTUE killer).

This is the single biggest playability regression in the spike.

### F2 — Subdivision label coverage stuck at 100 % `downbeat`

Strict onset gate failure on medium/hard label coverage is correct: the
authoring pass labels every obstacle as `downbeat` even though the
underlying onset/beat detector clearly resolves eighths and triplets in
the analysis (see `subdivision_histogram.eighth_grid` and
`triplet_grid` in each `snap_diagnostics_summary.json`). The label
plumbing from candidate → authored event drops the subdivision class.

This blocks any future system that wants to react to subdivision
(e.g. accent SFX, score multiplier on offbeat hits, shape rules
parameterised by label).

### F3 — Difficulty curve inversion on stomper

Median IOI: easy **755 ms** < medium 1125 ms ≈ hard 1074 ms. Easy is
denser than medium/hard. Hard adds count (196 vs 154) but not pace, and
its density comes from the long mono-lane runs in F1 rather than from
shorter IOIs. Hard p90 IOI (1502 ms) is *shorter* than medium's (1801 ms)
which papers over the issue but doesn't fix the median.

### F4 — Mental Corruption has flat IOI across all difficulties

Median IOI is exactly **806 ms on easy / medium / hard**. The chart
gains events by *filling more of the song* on harder difficulties
rather than by *playing faster*, so "hard" reads as "longer easy chart
with more wall clusters" rather than a tempo step-up.

### F5 — Mental easy is effectively 2-lane

Triangle count = **2 obstacles in 110**. Lane 2 is unused. Easy players
will not learn the third gate before medium throws 48 triangles at them.

### F6 — Onset source share is mostly nominal on stomper / drama

`timing_source = onset` shares: stomper 4–6 %, drama 6–8 %, mental
14 / 19 / 28 %. The "onset spike" reads as a *Mental Corruption* feature
right now; on stomper and drama it is essentially still beat-snap with a
handful of micro-shifts (max |residual| ≤ 15 ms anywhere). Worth
deciding if that's intended or if onset selection thresholds should be
loosened on the steadier-tempo tracks.

### F7 — Stomper hard short-IOI population (info, not red)

24 IOIs in 369–378 ms (just clearing the 380 ms cluster threshold), all
isolated (no adjacent indices). No physical burst — but every one of
them lands inside an existing same-shape run, so the player feels a
quick "tap-tap" with no lane change. Cluster gate is technically green;
playability is not.

## Top 3 tuning priorities for the next pass

1. **Break the shape↔lane lock and cap mono-lane runs.**
   Decouple `lane` from `shape` (e.g. round-robin or onset-energy-driven
   lane assignment, with shape chosen by accent/section). Add a
   per-difficulty mono-lane-run ceiling:

   | difficulty | max consecutive same-lane | max consecutive same-shape | per-lane share floor |
   |---|---|---|---|
   | easy   | ≤ 4 | ≤ 4 | ≥ 20 % |
   | medium | ≤ 5 | ≤ 5 | ≥ 18 % |
   | hard   | ≤ 6 | ≤ 6 | ≥ 15 % |

   Today's hard charts blow the same-lane ceiling by 6–7×.

2. **Propagate subdivision labels end-to-end and gate label coverage.**
   `subdivision_label` should reflect the candidate's grid class
   (`downbeat / eighth / triplet / syncop`) and not collapse to
   `downbeat`. Suggested coverage gates:

   | difficulty | non-downbeat label share |
   |---|---|
   | easy   | ≥ 5 %  |
   | medium | ≥ 15 % |
   | hard   | ≥ 25 % |

   Diagnostics already prove the source material can support this —
   mental hard alone has 226 eighth-grid candidates available.

3. **Fix difficulty pacing per song.**
   - **Stomper**: lift easy median IOI to ≥ 900 ms (cut ~25 events) and
     pull hard median to ≤ 850 ms so the curve is monotonic. Hard's
     "harder" should come from shorter IOIs, not longer mono-lane walls.
   - **Mental**: hard median IOI target ≤ 650 ms (vs today 806). Easy
     should keep ≥ 900 ms median **and** add ≥ 10 triangles so all
     three lanes are taught before medium.
   - **Drama** is the closest to right; main work is F1 (longest_run
     21 on hard) and F2 (labels).

## Out of scope but worth flagging

- Onset utilisation on stomper/drama is so low that the spike's
  "onset-aligned" claim is mostly a Mental Corruption claim. If we
  intend onset to drive feel on all three songs, the candidate-selection
  thresholds need a separate pass before more chart work happens.
- Dense-cluster gate uses fixed ms thresholds (700/380/300) but never
  fires on any chart — likely because the authoring layer pre-filters
  clusters out, not because the source lacks them. Worth confirming the
  gate is still meaningful before relying on its green status.
