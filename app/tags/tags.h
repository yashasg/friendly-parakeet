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

// ── Obstacle kind (per-tag table; exactly one per obstacle entity) ──
// Replaces the former ObstacleKind enum (issue #1202/#1204). Each
// former enum value is its own per-kind table:
//   - ShapeGateTag / SplitPathTag  → zero-column tag; the per-instance
//     data (`RequiredShape`, `RequiredLane`, `ShapeGateLane`) lives in
//     its own component table per the schema-per-kind mechanic in #1204.
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
