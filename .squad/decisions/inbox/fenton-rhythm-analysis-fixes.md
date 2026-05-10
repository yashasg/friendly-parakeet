# Fenton rhythm analysis fixes — 2026-05-10

## Decision
Segment-focus onset generation applies difficulty-specific IOI gating during selection, not as a post-generation cleanup pass. Protected cross-layer same-beat pairs remain distinct when they are different broad layers and within 50 ms.

## Rationale
Strict onset-spike diagnostics require easy/medium outputs to avoid dense sub-floor IOI clusters, but the project directive requires percussive, harmonic, and full-spectrum same-beat onsets to survive. The selector now thins ranked events for readability while exempting protected cross-layer pairs and can promote a sparse subdivision candidate so medium/hard diagnostics retain at least two beat/subdivision labels.

## Validation
- `python3 tools/test_validate_onset_spike_artifacts.py`
- `python3 tools/test_level_designer_onset_spike.py`
- Strict validation passed for `tools/diagnostics/onset_spike_smoke` and all `tools/diagnostics/*_loop1` onset-spike artifacts.
