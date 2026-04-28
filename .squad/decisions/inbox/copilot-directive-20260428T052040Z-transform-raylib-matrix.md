### 2026-04-28T05:20:40Z: User directive
**By:** yashasg (via Copilot)
**What:** Transform should use raylib's `Matrix` type and raylib-provided matrix/helper functions. Do not interpret "Matrix" as a custom matrix class or introduce a parallel Transform math abstraction unless raylib helpers are insufficient.
**Why:** User clarification — captured to correct the earlier Transform helper directive before implementation starts.

### 2026-04-28T05:20:40Z: User directive
**By:** yashasg (via Copilot)
**What:** Obstacles should be treated as object entities whose meshes travel together at the same direction and speed. Collision should use the known primary mesh, and meshes should be represented as separate typed mesh components on the obstacle entity rather than a logical root with child mesh entities.
**Why:** User clarification — captured to guide the entity/archetype migration and remove unnecessary root/child obstacle complexity.
