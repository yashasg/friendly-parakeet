# Acceptance Criteria Traceability

This file is the release-facing map for unchecked acceptance/checklist items in
design docs. An unchecked box in a design doc is not enough to block a release by
itself; each item must be traced to a test, an implementation issue, or an
explicitly deferred/cut status here.

## Release gate

Before a TestFlight or App Store go/no-go review, verify that every unchecked
design-doc criterion is covered by one of:

- a passing automated test or CI/build gate,
- an open issue that owns the remaining implementation work,
- a documented `Deferred` or `Cut` decision with rationale.

The promotability gate that enforces this map lives in
`docs/testflight-readiness.md` section 3.5.

## Design-doc criteria map

| Source | Criteria covered | Trace | Status |
| --- | --- | --- | --- |
| `design-docs/feature-specs.md` Spec 1 Input System | Touch zone routing, swipe thresholds/directions, discarded invalid swipes, active-shape no-op, same-frame swipe+tap, consume-once semantics, virtual coordinate conversion | Automated coverage in `tests/test_input_pipeline_behavior.cpp`, `tests/test_player_systems.cpp`, `tests/test_player_action_rhythm.cpp`, `tests/test_pr43_regression.cpp`, `tests/test_ndc_viewport.cpp`, and `tests/test_web_input_policy.cpp`; remaining multi-touch/debounce details are owned by the input backlog when touch hardware coverage expands. | Partially covered; open implementation/test gaps stay traceable through the input backlog rather than silent unchecked boxes. |
| `design-docs/feature-specs.md` Spec 2 Burnout Scoring | Burnout meter, zones, multiplier banking, burnout game-over, burnout chain rules | Superseded by issue #239 and the rhythm-first scoring docs (`design-docs/rhythm-design.md`, `design-docs/rhythm-spec.md`, `design-docs/energy-bar.md`). | Cut. Do not implement or gate release on these historical criteria. |
| `design-docs/feature-specs.md` Spec 3 Obstacle Spawning & Difficulty | Distance-ahead timer spawning, piecewise speed ramp, cumulative obstacle unlocks, weighted-random lane assignment, combo frequency, burnout range shrink | Superseded by song-driven beatmaps and known content decisions in issues #328, #420, #441, and #446. Current beatmap/content gates live in `tools/validate_loop2_content_gates.py`, `tools/check_shape_change_gap.py`, and rhythm docs. | Deferred or superseded. Release gating uses shipped beatmap validators, not these historical random-spawner criteria. |
| `design-docs/normalized-coordinates.md` | Normalized gameplay constants, collision/ring/test-player distances, beatmap positions, aspect-ratio enforcement | Display and virtual coordinate behavior is covered by `tests/test_ndc_viewport.cpp`, `tests/test_safe_area_layout.cpp`, and `tests/test_pr43_regression.cpp`; full gameplay-normalization migration remains parked by the document status. | Deferred. Not a TestFlight blocker until the feature is pulled from parked status. |
| `design-docs/rguilayout-portable-c-integration.md` Validation Checklist | `.rgl`/generated header presence, generated-file discipline, controller wiring, raygui CMake integration, button dispatch, warning-free builds | Build and zero-warning policy are enforced by CMake/CI; controller behavior is covered by screen/controller tests such as `tests/test_components.cpp`, `tests/test_world_systems.cpp`, `tests/test_level_select_controller.cpp`, `tests/test_input_pipeline_behavior.cpp`, and generated layout compilation. | Active gate through build/tests; visual regression remains future scope as stated in that doc. |
| `design-docs/beatmap-editor.md` section 6.1 task-file template | Placeholder checklist items `Item 1` through `Item 3` | These are template placeholders for sub-agent task files, not project acceptance criteria. | Not applicable. Do not count as release criteria. |
| `docs/testflight-readiness.md` unchecked items | MetricKit implementation, privacy/ATT verification, audio pipeline follow-up, tester group/public link | Each line already names its owning issue or operational owner in the readiness plan. | Active TestFlight checklist items; gate through section 3.5 and the linked issues. |

