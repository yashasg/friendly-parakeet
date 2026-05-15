# Keyser — History (Condensed)

## Core Context

- **Project:** C++20 raylib/EnTT rhythm bullet hell game
- **Role:** Lead Architect  
- **Joined:** 2026-04-26T02:07:46.542Z

## Expertise

**Audit & Architecture Review:** 21 rounds of SOLID/module-health/system-wiring audits; identified and escalated production bugs (lane_push_response_system wiring, order regressions); audited Position migration, shape_vertices cleanup, raylib vs. rlgl tradeoffs, EnTT lifecycle opportunities.

**Key Learnings:** Dispatcher callbacks bypass runner guards; parallel edits require explicit git paths; synthetic benchmarks need re-validation after refactors; gitignored inbox files don't indicate process failure; Public headers are architecture boundaries (resolved #405/#406 via system header split).

## Spawn: ECS Architecture Canon Codification (2026-05-15T06:15:00Z)

**Mission:** Codify ECS architecture canon from yashas's session directives into authoritative team-brain entry.

**Deliverable:** `.squad/decisions/inbox/keyser-ecs-architecture-canon.md` (8,525 bytes, 159 lines)

**Output:** Six-section canon grounded in verbatim user directives:
1. Core ECS Definitions (Component/Entity/System/Registry)
2. The Flatten Rule (no OOP god-structs)
3. The Raylib-First Rule (no abstraction except platform/)
4. The Util Test (real util has .h-only + inline; systems-in-costume have .cpp)
5. Folder Layout Standard (clean/impure/costumed verdicts)
6. EnTT Type-Collision Is Downstream (wrapper→raw swaps free; audit per-archetype)

**Status:** SUCCESS. Canon written; merged to decisions.md by Scribe. Ready for team enforcement.

**Related Issues:** #1193–#1200 (active work queue)

---

## Recent Status (2026-05-11)

**Module Health:** 11 🟢 (green) / 1 🟡 (motion_system pending bridge audit)

**Recent Audits (R19–R21):**
- R19: Architecture boundary issues (#405/#406/#407/#432/#433) — fixed public header split and docs
- R20: Residual abstraction seams in components
- R21: UI/WASM global state audit

**Current Work:** High-level system design documentation — approved 9-block layered architecture with fixed-step deterministic core and semantic intent flow (2026-05-11).

### 2026-05-12T05:13:01Z: High-level system design diagram (coordination session)

- Approved 9-block layered architecture visualization spec from Keyser (systems layers, boundary annotations, camera/audio notes for Coordinator).
- Approved Excalidraw layout specification from Redfoot (5 bands, color palette, node inventory, typography, acceptance criteria).
- Both specs merged into decisions.md; orchestration logs written; Coordinator owns canvas drawing.

## 2026-05-12T05:42:23Z — EnTT System Design Documentation (spawn session)

**Session:** Parallel Keyser + Keaton analysis synthesized by Coordinator
**Deliverable:** `.squad/log/2026-05-12T05-42-23Z-entt-system-design.md`

**Task:** Produce comprehensive system design of EnTT architecture, entity/component/system boundaries, util patterns.

**Keyser output:** High-level architecture mapping
- Registry ownership & lifecycle
- Entity/component/system boundaries  
- Event/data flow patterns
- Safe refactoring candidates (7 HC + 4 guardrails)
- Integration points

**Status:** Complete. Document ready for team review.

## Learnings

- 2026-05-15T06:10:56Z: ECS Architecture Canon codified in `.squad/decisions/inbox/keyser-ecs-architecture-canon.md` — six-section authoritative standard derived from yashas's 2026-05-14 directives covering core definitions, flatten rule, raylib-first rule, util test, folder layout, and EnTT type-collision policy.

- The biggest component leak is directory gravity: once a type becomes a plain struct, event payloads, scratch buffers, and session state all drift into `app/components/` even when no entity owns them.
- The decisive smell is lifecycle, not syntax. If a type only appears in `ctx().get<>`, `dispatcher.enqueue<>`, or reserve/clear helpers, it belongs beside its owning system.
- The raylib-first rule makes the leak more obvious: once the god-structs split, many leaf wrappers stop being defensible at all.
- 2026-05-14T22:27:50.210-07:00: Keaton ran the parallel audit lens on `app/components/*.h`; both passes converged that raw raylib/std substitutions should stay selective wherever EnTT type identity would otherwise collide.
