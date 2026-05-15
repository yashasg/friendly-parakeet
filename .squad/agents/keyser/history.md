# Keyser — History (Condensed)

## Core Context

- **Project:** C++20 raylib/EnTT rhythm bullet hell game
- **Role:** Lead Architect  
- **Joined:** 2026-04-26T02:07:46.542Z

## Expertise

**Audit & Architecture Review:** 21 rounds of SOLID/module-health/system-wiring audits; identified and escalated production bugs (lane_push_response_system wiring, order regressions); audited Position migration, shape_vertices cleanup, raylib vs. rlgl tradeoffs, EnTT lifecycle opportunities.

**Key Learnings:** Dispatcher callbacks bypass runner guards; parallel edits require explicit git paths; synthetic benchmarks need re-validation after refactors; gitignored inbox files don't indicate process failure; Public headers are architecture boundaries (resolved #405/#406 via system header split).

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

- The biggest component leak is directory gravity: once a type becomes a plain struct, event payloads, scratch buffers, and session state all drift into `app/components/` even when no entity owns them.
- The decisive smell is lifecycle, not syntax. If a type only appears in `ctx().get<>`, `dispatcher.enqueue<>`, or reserve/clear helpers, it belongs beside its owning system.
- The raylib-first rule makes the leak more obvious: once the god-structs split, many leaf wrappers stop being defensible at all.
- 2026-05-14T22:27:50.210-07:00: Keaton ran the parallel audit lens on `app/components/*.h`; both passes converged that raw raylib/std substitutions should stay selective wherever EnTT type identity would otherwise collide.
