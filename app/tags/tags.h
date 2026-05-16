#pragma once

// Canonical home for ECS marker tags.
//
// Per the ECS folder allowlist (`.squad/decisions.md`), `app/tags/` contains
// exactly one header — this file. Every empty marker struct used as an EnTT
// tag belongs here. Do not add new per-tag headers; extend this file instead.
//
// Tags are pure existence markers: presence on an entity is the entire signal.
// They carry no data and have no methods. Systems query for them via
// `registry.view<Tag>()` (or `entt::exclude<Tag>` for absence).

// ── Player ───────────────────────────────────────────────────
struct PlayerTag {};

// ── Player current shape (per-tag table; exactly one per player entity) ──
// Replaces the former `PlayerShape::current` Shape-typed field (issue
// #1202/#1204). Per Fabian's existential processing, each former enum value
// becomes its own zero-column table on the player entity. Shape changes are
// table operations (remove old tag, emplace new tag) — no `switch` on a
// discriminator, no `if (x == Shape::Bar)` branches.
//
// Maintenance invariant: exactly one `Shape*Tag` is present on any player
// entity. Use `set_player_shape_tag()` in `app/util/shape_tag.h` to keep
// the invariant; it removes all four tags before emplacing the matching one.
struct ShapeCircleTag   {};
struct ShapeSquareTag   {};
struct ShapeTriangleTag {};
struct ShapeHexagonTag  {};

// ── Shape Window target shape (per-tag table; exactly one when window open) ──
// Replaces the former `ShapeWindow::target_shape` Shape-typed field (issue
// #1202/#1204). The "which shape does the player intend to morph into?" data
// is presence of one of these tags on the player entity during a shape
// window. When the window closes (MorphOut completes → all `ShapeWindow*Tag`
// removed), the target tag is reset to Hexagon to match the legacy
// `target_shape = Shape::Hexagon` reset.
struct TargetShapeCircleTag   {};
struct TargetShapeSquareTag   {};
struct TargetShapeTriangleTag {};
struct TargetShapeHexagonTag  {};

// ── Shape Window phase (per-tag table; absence of all three = Idle) ──
// Replaces the former WindowPhase enum (issue #1202/#1204).
// The shape-window state machine on the player advances through
// MorphIn → Active → MorphOut → Idle. Per Fabian's existential processing,
// each non-Idle phase is its own zero-column table; the Idle state is the
// absence of all three tags. shape_window_system runs one transform per
// tag instead of switching on a discriminator.
struct ShapeWindowMorphInTag  {};
struct ShapeWindowActiveTag   {};
struct ShapeWindowMorphOutTag {};

// ── Obstacles ────────────────────────────────────────────────
struct ObstacleTag {};

// ── Obstacle required shape (per-tag table; ShapeGate/SplitPath obstacles) ──
// Replaces the former `RequiredShape::shape` Shape-typed field (issue
// #1202/#1204). The whole `RequiredShape` struct was a single-column table;
// per Fabian's relational-database mechanic the column collapses into the
// tag set itself. Each ShapeGate / SplitPath obstacle carries exactly one
// `RequiredShape*Tag`; obstacle factories emplace it via
// `set_required_shape_tag()` in `app/util/shape_tag.h`.
struct RequiredShapeCircleTag   {};
struct RequiredShapeSquareTag   {};
struct RequiredShapeTriangleTag {};
struct RequiredShapeHexagonTag  {};

// ── Obstacle kind (per-tag table; exactly one per obstacle entity) ──
// Replaces the former ObstacleKind enum (issue #1202/#1204). Each
// former enum value is its own per-kind table:
//   - ShapeGateTag / SplitPathTag  → zero-column tag; the per-instance
//     data (`RequiredShape*Tag` for shape, raw `int8_t` for lane index)
//     lives in its own component table per the schema-per-kind mechanic
//     in #1204. The lane wrappers `ShapeGateLane` / `RequiredLane` were
//     unwrapped to raw `int8_t` per #1198; see `app/components/obstacle.h`
//     for the slot-reservation note.
//   - OnsetMarkerTag → zero-column tag (no per-instance data).
// Spawn helpers in `entities/obstacle_entity.h` emplace exactly one
// kind tag; renderer / logger / test-player systems dispatch via
// tag-presence views (no `switch` on a discriminator).
struct ShapeGateTag {};
struct SplitPathTag {};

// Runtime cue authored from beatmap onset metadata. It is visible but
// intentionally non-scorable and non-blocking.
struct OnsetMarkerTag {};

// Presence means scoring has consumed the obstacle's final hit/miss result.
struct ResolvedObstacleTag {};

// Presence means the obstacle has been cleared and awaits scoring.
struct ScoredTag {};

// Obstacle does not participate in the scoring ladder
// (no score popup, no chain contribution).
struct NonScorableTag {};

// Scored obstacle was failed/missed and should not award points.
struct MissTag {};

// ── Particles ────────────────────────────────────────────────
struct ParticleTag {};

// ── HUD / Energy bar ─────────────────────────────────────────
struct EnergyBarTag {};

// ── Test player (deterministic AI) ───────────────────────────
struct TestPlayerPlannedTag {};

// ── Render-pass membership; one per entity ───────────────────
struct TagWorldPass   {};  // drawn in BeginMode3D (3D world geometry)
struct TagEffectsPass {};  // particles, post-process overlays
struct TagHUDPass     {};  // screen-space UI elements

// ── Mesh kind (per-tag table; pair with ModelTransform/MeshChild) ──
// Replaces the former MeshType enum (issue #1202/#1204). Each kind drives
// its own renderer transform — no `switch` on a discriminator. The Shape
// kind carries an extra column (mesh_index) so it lives as a real struct
// in app/components/render_mesh.h; the other two are zero-column tables.
struct MeshKindSlab {};
struct MeshKindQuad {};

// ── Timing tier (per-tag table; one tier tag per graded obstacle) ──
// Replaces the former TimingTier enum (issue #1202/#1204). Each former enum
// value becomes its own zero-column table; the per-row "precision" scalar
// lives in TimingGrade. collision_system emplaces exactly one tier tag at
// grading-time; scoring_system, popup_feedback_system, and the session
// logger dispatch via tag presence (no switch on a discriminator).
//
// Score popup entities carry the same tier tag so renderers can identify
// their tier without re-reading ScorePopup data.
struct TimingPerfectTag {};
struct TimingGoodTag    {};
struct TimingOkTag      {};
struct TimingBadTag     {};

// ── Singletons ───────────────────────────────────────────────
struct BeatMapTag {};
struct SettingsTag {};

// ── Persistence dirty flags (existence = needs save) ─────────
// Replaces the former `bool dirty` columns on `SettingsPersistence` and
// `HighScorePersistence` per Fabian relational normalization (issue #1203
// "Apply relational normalization to god-class component bundles"). A
// "dirty" state-machine flag is a NULL-column in disguise: meaningful only
// while a save is pending. Per Fabian's existential processing, that
// pending-state is its own zero-column table; presence on the owning
// entity / ctx is the entire signal.
//
// `SettingsDirtyTag` lives on the singleton SettingsTag entity (matches
// the entity-level SettingsPersistence component).
// `HighScoreDirtyTag` lives on `registry.ctx()` (matches the ctx-level
// HighScorePersistence singleton).
//
// Writers: `settings::mark_dirty_and_save()`, `update_and_persist_high_score()`.
// Test helpers: emplace / remove the tag directly via the registry / ctx.
struct SettingsDirtyTag  {};
struct HighScoreDirtyTag {};

// ── Game phase (per-tag ctx tables; exactly one present at any time) ──
// Per Fabian's existential processing (issue #1202/#1204), each former
// GamePhase enum value gets its own zero-column table on `registry.ctx()`.
// Exactly one of these tags is present at any time; `enter_phase<...>()`
// (see `systems/game_phase_transition.h`) is the sole writer.
struct GamePhaseTitleTag        {};
struct GamePhaseLevelSelectTag  {};
struct GamePhasePlayingTag      {};
struct GamePhasePausedTag       {};
struct GamePhaseGameOverTag     {};
struct GamePhaseSongCompleteTag {};
struct GamePhaseSettingsTag     {};
struct GamePhaseTutorialTag     {};

// ── Pending phase transition (per-tag ctx tables) ──────────────────────
// Per Fabian existential processing (issue #1202/#1204), the per-frame
// "transition request" target is expressed as ctx-tag presence. UI / input
// systems call `request_phase_transition<NextPhase*Tag>()` instead of
// writing an enum field; `game_state_system` reads tag presence at the top
// of its dispatch block, performs the per-tag swap, and drains the mirror
// via `clear_next_phase_tags()`. Outside that block exactly zero
// `NextPhase*Tag` ctx slots are present; inside it, exactly one.
struct NextPhaseTitleTag        {};
struct NextPhaseLevelSelectTag  {};
struct NextPhasePlayingTag      {};
struct NextPhasePausedTag       {};
struct NextPhaseGameOverTag     {};
struct NextPhaseSongCompleteTag {};
struct NextPhaseSettingsTag     {};
struct NextPhaseTutorialTag     {};

// ── End-screen menu choice (per-choice ctx tables) ───────────
// Per Fabian's existential processing (issue #1202/#1204), each former
// EndScreenChoice value is its own zero-column table on `registry.ctx()`.
// Presence of an EndChoice* tag signals the player's selection on a Game
// Over or Song Complete screen; absence of all three is the "no choice
// yet" state (what EndScreenChoice::None represented). One transform per
// tag in `game_state_end_screen_system` consumes the choice when the
// per-phase input delay has elapsed; until then the tag persists.
struct EndChoiceRestart {};
struct EndChoiceLevelSelect {};
struct EndChoiceMainMenu {};

// ── Level-select confirmation latch (singleton ctx tag) ──────
// Per Fabian's existential processing / relational normalization
// (issues #1194, #1203): the former `LevelSelectState::confirmed` bool was a
// one-shot latch with a different lifecycle than the selection bundle
// (selection persists across phases; confirmation is a per-frame intent set
// by UI on START and drained by game_state_system after the entry debounce).
// Existence of this ctx tag is the data; absence is "not confirmed yet".
// Matches the precedent set for `EndChoiceRestart` / `EndChoiceLevelSelect` /
// `EndChoiceMainMenu` (player-choice latches on the end screens).
struct LevelSelectConfirmedTag {};

// ── Game-over cause (per-cause ctx tables) ──────────────────
// Per Fabian's existential processing, each former DeathCause value is its
// own zero-column table on `registry.ctx()`. Presence of a *Death tag is the
// data; absence of all *Death tags means "no terminal cause recorded" (what
// the former DeathCause::None sentinel represented).
//
// The system that triggers the end-of-run condition emplaces the matching
// tag; the Game Over screen controller reads tag presence to surface a
// one-line, platform-neutral, colorblind-safe reason.
struct EnergyDepletedDeath {};

// ── UI screens (per-screen tag tables) ───────────────────────
// Per #1193 (rguilayout codegen → entity-spawner functions). Each `.rgl`
// layout in `content/ui/screens/` produces a `spawn_<screen>_screen()` that
// emplaces the matching per-screen tag on every control entity it creates,
// and a `despawn_<screen>_screen()` that destroys all entities carrying the
// tag. The UI render system queries `view<UiLabelTag, ScreenTag>()` /
// `view<UiButtonTag, ScreenTag>()` to render only the active screen.
// Per Fabian's existential processing, screen-membership is presence of one
// of these tags — there is no `current_screen` discriminator anywhere.
struct TitleScreenTag        {};
struct LevelSelectScreenTag  {};
struct TutorialScreenTag     {};
struct GameplayHudTag        {};
struct PausedScreenTag       {};
struct GameOverScreenTag     {};
struct SongCompleteScreenTag {};
struct SettingsScreenTag     {};

// ── UI control kinds (per-kind tag tables) ───────────────────
// Per #1193 — `.rgl` control type codes map to existential per-kind tags
// instead of a `UiKind` enum discriminator. Render and input dispatch use
// tag-presence views (no `switch` on a discriminator).
//   * `UiLabelTag`    — type code 4  (static text)
//   * `UiButtonTag`   — type code 5  (pressable)
//   * `UiDummyRecTag` — type code 24 (visual placeholder / icon slot)
struct UiLabelTag    {};
struct UiButtonTag   {};
struct UiDummyRecTag {};
