# Kobayashi — History

## 2026-05-03 | Steamworks Integration Path Assessment
- **Question:** Is SDL2 materially easier for Steamworks than raylib?
- **Finding:** No. Steamworks SDK is orthogonal to graphics/input library choice.
- **Why:** Overlay hooks use OS window handles (HWND/NSWindow); achievements/stats use async callbacks; both raylib and SDL2 can satisfy these. Switching engines for Steamworks is scope creep.
- **Recommendation:** Keep raylib, create isolated `SteamworksSystem` ECS wrapper. Real work is in platform/release pipeline (SteamPipe, AppID, depot setup), not engine choice.
- **Risk (switch to SDL2):** MEDIUM-HIGH — 6-12 week rewrite, breaks iOS/web builds, gameplay regression.
- **Risk (raylib + Steamworks):** LOW — 2-4 weeks, zero impact on existing systems.
- **Output:** Decision doc at `.squad/decisions/inbox/kobayashi-steam-integration-decision.md`

