### 2026-04-28T05:30:22Z: User directive
**By:** yashasg (via Copilot)
**What:** Entity-owned raylib `Model` values should use session/resource-pool lifetime for GPU/resource ownership; Models are unloaded at level/session shutdown rather than per entity destruction.
**Why:** User decision — captured before implementing Model components to avoid accidental double-unload or per-entity GPU churn.

### 2026-04-28T05:30:22Z: User directive
**By:** yashasg (via Copilot)
**What:** Render-pass membership should use empty tag components (for example world/HUD/effects tags) rather than a compact runtime `RenderPass { uint8_t pass; }` component.
**Why:** User decision — captured to guide the rendering component split away from `rendering.h` as a component bucket.
