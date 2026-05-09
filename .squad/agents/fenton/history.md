# Fenton — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Audio Expert
- **Joined:** 2026-05-01T00:00:00Z

## Learnings

- Pending first assignment.

- 2026-05-09T00:16:15.582-07:00 — Timing-drift experiment on `2_drama` medium/pro confirmed collision drift is primarily fixed-frame quantization: fresh combined runs mean `actual - expected` = 8.94 ms over 159 collisions versus 8.33 ms half-frame at 60 Hz; provided full log matched at 8.59 ms over 151 collisions. Artifacts: `/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/timing-drift-experiment-2026-05-09/fenton-timing-drift-report.md`, `timing_drift_analysis.json`, `collision_drifts.csv`, and fresh run logs `run2_session_pro_rt0000000364_n0001.log` / `run3_session_pro_rt0000000322_n0001.log`.

- 2026-05-09T00:29:44.825-07:00 — Compared Reddit OP's STFT/onset/tempogram/subdivision pipeline against our 2_drama path. Key learning: our current pipeline already uses STFT flux plus tempogram-derived tempo, but still uses `librosa.beat.beat_track` for authoritative beat timestamps; naive PLP is not a drop-in replacement on 2_drama and should be evaluated as an augmenting candidate grid. Artifact: `/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/beat-grid-tempogram-review-2026-05-09/fenton-op-pipeline-comparison.md`.

- 2026-05-09T00:41:48.960-07:00 — Produced Loop 1 DSP guardrails for Fenster to avoid overfitting subdivision snapping on `2_drama`. Confirmed analysis JSON has no explicit confidence/PLP fields; usable confidence proxies are event flux percentile, pass count, residual, class margin, and local repetition. Recommended quarter <=20 ms, eighth <=25 ms, and triplet <=25 ms only with strong repeated evidence. Artifacts: `/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/loop1-dsp-guardrails-2026-05-09/fenton-loop1-dsp-guardrails.md`, `fenton_subdivision_guardrail_probe.py`, and `2_drama_guardrail_metrics.json`.
