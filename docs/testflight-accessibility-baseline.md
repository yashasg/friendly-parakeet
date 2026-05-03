# SHAPESHIFTER — TestFlight Accessibility Baseline

**Owner:** Redfoot (UI/UX)  
**Issue:** #75  
**Scope:** TestFlight builds (closed alpha/beta/open beta)  
**Use:** Source-of-truth checklist for implementation + QA sign-off

---

## 1) Pass Criteria Summary (must all pass)

| ID | Requirement | Pass Threshold | Verification |
| --- | --- | --- | --- |
| A11Y-01 | Shape readability is never color-only | Circle/Square/Triangle are always distinguishable by silhouette in gameplay and HUD | Visual inspection + screenshot review |
| A11Y-02 | Shape prompts include text labels | FTUE + controls/help surfaces show shape names (`Circle`, `Square`, `Triangle`) next to symbols | Manual checklist |
| A11Y-03 | Critical HUD text contrast | HUD text and numeric readouts meet at least 4.5:1 contrast against immediate background | Contrast sampling on screenshots |
| A11Y-04 | Touch target minimum | Primary interactive controls are at least 48×48 pt effective target | Layout measurement + tap test |
| A11Y-05 | Audio-off playability | Full song is playable with device muted/audio volume 0 using only visual timing cues | 3-song manual run, no audio |
| A11Y-06 | Haptics opt-out correctness | With `haptics_enabled = false`, no haptic events fire (0 vibrations) while gameplay remains functional | Settings toggle test |
| A11Y-07 | Haptics graceful fallback | If OS-level haptics are disabled/unavailable, game remains stable and fully playable (no crash/no stalls) | Device settings test |
| A11Y-08 | Reduced motion mode | With `reduce_motion = true`, non-essential pulses/flashes are suppressed while gameplay timing readability remains intact | A/B video compare + checklist |
| A11Y-09 | Accessibility settings persistence | `haptics_enabled` + `reduce_motion` persist across app restart | Toggle, relaunch, verify state |

---

## 2) Detailed Baseline Rules

### 2.1 Colorblind-safe shape/label rules

1. **Do not encode gameplay-critical information by hue alone.**  
   Shape identity must always be readable via silhouette (●, ■, ▲) independent of color perception.
2. **Shape labels must be present in onboarding/help surfaces.**  
   First-time players must see text mapping (`Circle`, `Square`, `Triangle`) alongside the symbols.
3. **Selected-state feedback must include non-color cues.**  
   Active/selected controls must also differ by outline thickness, fill, scale, or icon treatment.

### 2.2 Audio-off and haptics rules

1. Muted play (hardware mute + app volume effectively silent) must preserve core timing readability through visual cues.
2. Haptics are strictly optional; disabling haptics must not degrade control response or scoring behavior.
3. Haptic backend failures/unavailability are non-fatal and must never block frame progression.

### 2.3 Reduced motion + readability rules

1. Reduced motion mode keeps informational UI but suppresses decorative motion loops and repeated flashes.
2. Critical feedback (hit/miss/result state) stays visible as static or minimally animated alternatives.
3. All critical labels and score/energy text remain readable at gameplay pace (no transient-only communication).

---

## 3) TestFlight UAT Checklist (execute per candidate build)

| # | Scenario | Steps | Expected |
| --- | --- | --- | --- |
| AX-01 | Shape-only discrimination | Play 1 full song while intentionally ignoring color cues | Player can still correctly match shapes |
| AX-02 | FTUE label presence | Fresh run (FTUE path), inspect shape teaching step | Text labels for all 3 shapes are present |
| AX-03 | HUD contrast check | Capture screenshots in bright + dark gameplay moments | All critical text passes 4.5:1 |
| AX-04 | Audio-off run | Mute device and complete one song | No forced failure due to absent audio cues |
| AX-05 | Haptics off | Toggle haptics OFF, play 2 minutes | No vibration events; controls unaffected |
| AX-06 | OS haptics unavailable | Disable system haptics (or unsupported device), play 2 minutes | No crash, no hitching, playable |
| AX-07 | Reduced motion ON | Toggle reduced motion ON and play 2 minutes | Decorative motion reduced; gameplay clarity intact |
| AX-08 | Persistence | Set haptics OFF + reduced motion ON, restart app | Toggles restore previous values |
| AX-09 | Regression gate | Run AX-01..08 on each required iPhone class | 100% pass before promotion |

---

## 4) Release Gate Policy

- The App Store promotion gate in `docs/testflight-readiness.md` includes this
  baseline as a hard requirement.
- Any failed A11Y item blocks promotion until fixed or explicitly deferred by
  PM + QA + Design sign-off.
- Accessibility bugs that block core playability are treated as Sev-1.

---

## 5) Implementation Notes (current codebase hooks)

- Existing settings fields already support baseline toggles:
  - `SettingsState::haptics_enabled`
  - `SettingsState::reduce_motion`
- Settings UI path:
  - `app/ui/screen_controllers/settings_screen_controller.cpp`
- Persisted settings path:
  - `app/util/settings_persistence.cpp`

These hooks are the minimum integration points for engineering to satisfy
A11Y-06, A11Y-08, and A11Y-09.
