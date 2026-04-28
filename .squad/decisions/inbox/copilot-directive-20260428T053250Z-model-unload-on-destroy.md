### 2026-04-28T05:32:50Z: User directive
**By:** yashasg (via Copilot)
**What:** Correction to Model lifetime: raylib `Model` values owned by ECS entities should be unloaded when the owning entity is destroyed, not deferred to session/resource-pool shutdown.
**Why:** User revised the earlier lifetime decision — captured so Model component implementation uses entity destruction cleanup and avoids stale resource-pool guidance.
