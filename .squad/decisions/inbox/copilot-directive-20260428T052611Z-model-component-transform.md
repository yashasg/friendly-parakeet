### 2026-04-28T05:26:11Z: User directive
**By:** yashasg (via Copilot)
**What:** Player and obstacle entities should use raylib's `Model` as the ECS component when appropriate, because `Model` already contains `transform`, meshes, materials, and related render data. For current generated meshes, entities should own their `Model` by value.
**Why:** User clarification — captured to supersede the earlier typed mesh-component direction before the entity/render migration starts.

### 2026-04-28T05:26:11Z: User directive
**By:** yashasg (via Copilot)
**What:** For visible entities that own a raylib `Model`, `Model.transform` is the authoritative transform. Do not keep a separate gameplay `Transform` component that must be copied/synced into `Model.transform` for those entities.
**Why:** User decision — captured to prevent duplicate transform sources during the Matrix/Model migration.
