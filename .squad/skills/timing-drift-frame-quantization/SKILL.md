# SKILL: Timing Drift Frame-Quantization Validation

**Domain:** Rhythm timing QA / regression checks  
**Applicable to:** Cases where collision drift appears positive and may be due to fixed-tick quantization rather than audio alignment bugs.

---

## Pattern

Validate drift claims with two independent gates: **freshness** and **distribution shape**.

### 1) Freshness gate (mandatory first)

1. Confirm experiment log is newly generated (different filename and file hash from prior runs).
2. Confirm run ended cleanly (no forced termination markers).
3. Reject statistical conclusions if freshness fails.

### 2) Distribution gate (quantization hypothesis)

Given `drift = collision_song_time - beat_arrival_time` and fixed frame period `F`:

- Sample size: `N >= 100`
- Non-negativity: negative drift fraction <= 1%
- Mean target: `mean(drift)` in `[0.35F, 0.65F]`
- Tail bound: `p95 <= F + 2ms`, `max <= F + 4ms`
- Shape check: histogram over `[0, F]` with >=8 bins; no bin >45% (avoid edge collapse)

Pass all => frame-quantization likely dominant.  
Fail any => rerun (if freshness issue) or escalate to offset/librosa investigation.

---

## Why this works

A fixed-step collision resolver can only observe crossings at tick boundaries, introducing non-negative quantization delay whose expected mean is near `F/2` and bounded near one frame (plus logging precision/jitter).

---

## Notes

- If drift logging is rounded (e.g., 1ms precision), keep small tolerance margins on max/p95 checks.
- Always pair statistical checks with code-path verification of where `drift` is computed and where song time is sampled.

## Audio-analysis separation note

For audio-expert reviews, explicitly distinguish two clocks:
- `expected_time` from the chart/librosa/STFT pipeline;
- `dispatch_time` from the engine frame or fixed tick.

If `dispatch_time - expected_time` is one-frame-wide and centered near `F/2`, treat frame quantization as dominant before retuning librosa/aubio parameters. Only investigate STFT/beat-picking offsets after removing or accounting for this engine-side quantization.
