### 2026-04-28T05:36:50Z: User directive
**By:** yashasg (via Copilot)
**What:** Do not use `LoadModelFromMesh` for obstacle Models. Obstacles are a combination of three meshes, so their raylib `Model` should be built with a populated `meshes` array instead.
**Why:** User correction — captured to prevent one-mesh Model construction from driving the obstacle Model migration.
