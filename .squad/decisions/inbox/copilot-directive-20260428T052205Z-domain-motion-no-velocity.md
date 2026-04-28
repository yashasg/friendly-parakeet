### 2026-04-28T05:22:05Z: User directive
**By:** yashasg (via Copilot)
**What:** Once `Transform.matrix` owns position, moving entities should use domain-specific motion data instead of a generic `Velocity` component. Obstacle speed, lane/player state, and other domain systems should drive Transform updates directly through raylib Matrix helpers.
**Why:** User decision — captured before the Position/Velocity migration starts.
