# Session Log: Archetype Candidate Audit

**Date:** 2026-04-27T23:41:31Z  
**Requested by:** yashasg  
**Scope:** Read-only audit of ECS systems and test helpers to identify archetype extraction candidates and reduce duplicate entity construction.

## Summary

Two agents completed independent audits:

**Keyser (ECS/archetype boundaries):** Identified five candidates ranked by priority.
- **P1:** Obstacle pre-bundle (C1, under #344) and player entity factory (C2, new issue)
- **P2:** Score popup (C3) and shape button orphan (C4)
- **P3:** gameobjects/shape_obstacle relocation (C5)
- Explicit non-candidates: state transitions, JSON UI, entity mutations, phase-gating tags

**Keaton (duplicate construction patterns):** Identified test helper drift as P0 blocker.
- `tests/test_helpers.h` has six obstacle helpers with color contract divergences (HighBar, LanePush, shape_gate)
- Recommended P0 migration to canonical `apply_obstacle_archetype` path before P1/P2
- Ranked P1a: `create_obstacle_base`, P1b: `create_player_entity`, P2: internal refactor of `ui_button_spawner.h`

## Next Steps

Coordinator will route findings to implementation team:
- P0 test helper fix (Keaton)
- P1a obstacle base extraction (per C1, under #344)
- P1b player archetype factory (new issue)
- P2/P3 backlog or deferred per priority

## Decisions Added to Inbox

- `keyser-archetype-candidate-audit.md` (four candidates ranked + routing)
- `keaton-archetype-duplicate-audit.md` (test drift + P0-P2 recommendations)

Status: AUDIT CLOSED. Coordinator synthesis pending.
