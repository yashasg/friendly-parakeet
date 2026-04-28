# TestFlight Readiness Plan

**Owner:** Edie (PM)
**Milestone:** `test-flight`
**Status:** v1.0 — initial decisions for issues #185, #188, #201
**Audience:** Engineering, Design, QA, Release

This document is the source of truth for the TestFlight beta program: what we
collect from real devices, what we ship, and how we decide whether a build is
good enough to promote to App Store submission. It supersedes ad-hoc decisions
and is intended to be amended as we learn from each cohort.

---

## 1. Crash Reporting & Telemetry (issue #185)

### 1.1 Decision — Apple-first, zero third-party SDK

For the TestFlight phase we ship **no third-party crash or analytics SDK**. We
rely entirely on first-party Apple tooling:

| Source                              | What it gives us                                     |
| ----------------------------------- | ---------------------------------------------------- |
| **TestFlight crash logs**           | Symbolicated stack traces per build, per tester      |
| **TestFlight feedback**             | Tester-submitted screenshots + comments              |
| **MetricKit** (`MXMetricManager`)   | Daily aggregated crash, hang, launch-time, energy    |
| **MetricKit diagnostics**           | `MXCrashDiagnostic`, `MXHangDiagnostic` payloads     |
| **App Store Connect → Analytics**   | Sessions, crashes, retention (post-release only)     |

**Rationale**
- Zero new third-party dependency, zero ATT prompt requirement, no new
  privacy-label categories beyond what Apple already discloses.
- Aligns with the existing `vcpkg.json` (no Sentry/Firebase/PostHog) and avoids
  the SDK-evaluation cost during a beta where the cohort is small enough that
  TestFlight + MetricKit produce sufficient signal.
- We can add a third-party SDK *later* if MetricKit volume proves too low or
  too lagged; the decision is reversible.

We will **revisit** this decision before App Store submission if any of the
following are true: (a) crash-free session rate cannot be measured to
±0.5%, (b) MetricKit reports lag prevents debugging a Sev-1, or
(c) we need real-time funnel analytics for a content/monetization decision.

### 1.2 Minimum viable telemetry event set

Telemetry is **not** sent over the network in the TestFlight build. It is
captured to the existing local `session_log` (#112) for tester-submitted
diagnostic bundles and for MetricKit signpost correlation. This keeps us inside
Apple's first-party envelope while still letting us measure the funnel.

Event set (v1):

| Event              | Properties (no PII)                                | Purpose                                |
| ------------------ | -------------------------------------------------- | -------------------------------------- |
| `app_launch`       | build, os_version, device_model, locale            | Cohort baseline                        |
| `ftue_started`     | run_index                                          | Onboarding entry                       |
| `ftue_completed`   | run_index, duration_ms                             | Onboarding completion rate             |
| `ftue_abandoned`   | run_index, last_step                               | Where new players drop                 |
| `song_started`     | song_id, difficulty                                | Content selection                      |
| `song_completed`   | song_id, difficulty, score, accuracy_pct           | Win-rate per song/difficulty           |
| `song_failed`      | song_id, difficulty, hp_zero_at_song_pct           | Failure point analysis                 |
| `pause_used`       | song_id, song_pct                                  | Pause behaviour                        |
| `audio_interrupt`  | song_id, song_pct, source (call/siri/route)        | iOS audio-session correctness (#180)   |
| `app_background`   | song_id?, song_pct?                                | Lifecycle correctness (#182)           |

Properties **explicitly excluded**: IDFA, IDFV, advertising identifiers, raw
device serial, user name, email, IP address, precise location, contacts,
photos, any free-form text. `device_model` is the public Apple model
identifier (e.g. `iPhone15,2`), which is not PII.

### 1.3 Schema, retention, access

- **Schema:** versioned JSON envelope `{schema_version, build, events: [...]}`
  appended to `session_log`. Each event has `ts_ms_since_launch` (relative,
  not wall-clock) to avoid timezone fingerprinting.
- **Retention (device):** the local `session_log` rolls over at 1 MB or on
  app uninstall. Testers may attach it manually via TestFlight feedback if
  asked; we never auto-upload.
- **Retention (TestFlight / App Store Connect):** governed by Apple's defaults
  (TestFlight crashes ~90 days, MetricKit 365-day rolling window). We do not
  re-host or re-export.
- **Access:** App Store Connect → repo owner + invited internal team only.
  No external sharing.

### 1.4 Privacy / ATT / App Privacy label posture

- **ATT:** **No** `App Tracking Transparency` prompt required. We do not track
  users across apps or websites owned by other companies, and we collect no
  IDFA. This will be re-evaluated in #194.
- **App Privacy label answers (preliminary, see #189 for final):**
  - *Data Used to Track You:* **None**.
  - *Data Linked to You:* **None**.
  - *Data Not Linked to You:* **Diagnostics — Crash Data, Performance Data**
    (the MetricKit/TestFlight default; "Not Linked" because no user identifier
    is attached).
- **PII:** **None collected.** Reviewed against the event list above; if any
  future event needs to add a property, the PR must update this section and
  ping #189 for label re-review.
- **Opt-out:** Apple already provides a system-level opt-out for sharing
  diagnostics with developers (Settings → Privacy & Security → Analytics &
  Improvements → "Share With App Developers"). We respect this implicitly
  because we do not run a parallel pipeline. No in-app toggle required for v1.

### 1.5 Acceptance for #185
- [x] Decision recorded (MetricKit + TestFlight, no SDK).
- [x] Event set defined.
- [x] Schema, retention, access documented.
- [x] Privacy / ATT / no-PII posture documented.
- [ ] Engineering: wire MetricKit subscription on iOS launch (tracked as
      follow-up implementation task in the iOS lifecycle epic).
- [ ] Verify against #189 (privacy labels) and #194 (ATT) before first TF
      upload.

---

## 2. iOS Audio Asset Format & Size Budget (issue #188)

### 2.1 App-size budget (v1)

| Metric                        | Budget           | Rationale                                      |
| ----------------------------- | ---------------- | ---------------------------------------------- |
| **Compressed IPA download**   | **≤ 100 MB**     | Half of the 200 MB cellular cap; headroom for icons, on-device DB, future songs |
| **Audio bundle (3 songs)**    | **≤ 35 MB**      | ~12 MB/song target; current FLAC totals ~92 MB |
| **Cold-start budget**         | ≤ 2.0 s on iPhone 12-class | First-frame after launch; large lossless decode dominates today |

Today's `content/audio/*.flac` totals **~88 MB** (28.4 + 32.5 + 28.6 MB), which
alone exceeds half the cellular cap before any binary, assets, or icons. This
is the primary driver for re-encoding.

### 2.2 Shipping format — AAC `.m4a` @ 192 kbps CBR

**Decision:** ship audio as **AAC-LC in `.m4a` containers, 192 kbps CBR,
44.1 kHz stereo**.

| Option                        | Verdict   | Reason                                                |
| ----------------------------- | --------- | ----------------------------------------------------- |
| FLAC (current)                | Reject    | ~30 MB/song — blows IPA budget, FLAC support on raylib is platform-dependent |
| AAC 128 kbps                  | Reject    | Audible artefacts on percussive transients in #1 stomper; rhythm game leans on attacks |
| **AAC 192 kbps**              | **Ship**  | ~3 MB/min, hardware-decoded on iOS, native AVFoundation + raylib OGG/M4A path, perceptually transparent on our content in informal tests |
| AAC 256 kbps                  | Hold      | Use as fallback if 192 fails the A/B in §2.4         |
| OGG Vorbis 192                | Reject    | Worse iOS decoder support, no hardware path           |
| MP3 192                       | Reject    | Patent/compat concerns vs AAC, no quality advantage   |

**Estimated post-encode size:** ~10–12 MB per song → ~30–36 MB for the bundle,
inside the 35 MB budget.

> This task does **not** transcode audio. Transcoding lands in a separate
> implementation issue once this decision is signed off. The pipeline change
> belongs to Fenster (`tools/rhythm_pipeline.py`) and Hockney (iOS audio).

### 2.3 Beatmap regeneration / drift policy

This is a rhythm game; onset positions are sacred. Re-encoding can shift
perceived attack times by a few ms via low-pass filtering and codec delay.

**Policy:**
1. Beatmaps are **regenerated against the shipping `.m4a`** as the canonical
   input — *not* invariant-asserted from the FLAC analysis. This avoids hidden
   drift accumulating across encoder versions.
2. The Python pipeline (`tools/rhythm_pipeline.py`) gains a CLI flag to point
   at the shipping format. Documented in `docs/audio-pipeline-spec.md` (update
   pending).
3. We also keep the FLAC masters in source control / archive but **do not
   ship** them. They are the regeneration source of truth.
4. Encoder is pinned (recommended: `ffmpeg` with `-c:a aac -b:a 192k -ar 44100
   -ac 2`, single specific ffmpeg version recorded in
   `docs/audio-pipeline-spec.md`) so re-encodes are bit-stable across
   developers and CI.

### 2.4 Validation plan (no transcode in this task)

When the implementation issue lands, validation must show **all** of:

| Check                        | Method                                                | Pass criterion                          |
| ---------------------------- | ----------------------------------------------------- | --------------------------------------- |
| **Perceptual A/B**           | 3 listeners, blind A/B FLAC vs `.m4a` on AirPods Pro and iPhone speaker, all 3 songs | < 2/9 correct identifications (chance) |
| **Onset drift**              | `aubio onset` on FLAC and `.m4a`, compute per-onset Δms | p95 |Δ| ≤ 10 ms; max ≤ 20 ms          |
| **Beatmap re-gen diff**      | Run `tools/rhythm_pipeline.py` on both, diff outputs  | All beat events within 1 frame @ 60 Hz (16.6 ms) |
| **In-game perceptual sync**  | Pro AI persona run, compare hit-timing histogram      | Mean offset within ±5 ms of FLAC baseline |
| **File size**                | `ls -l content/audio/`                                | ≤ 12 MB/song, ≤ 35 MB total             |
| **Cold-start**               | iPhone 12, fresh launch to first song frame           | ≤ 2.0 s                                  |

Failing any row → bump bitrate to 256 kbps and retest, or escalate to PM.

### 2.5 Acceptance for #188
- [x] IPA size budget set (≤ 100 MB).
- [x] Shipping format chosen (AAC 192 kbps `.m4a`).
- [x] Beatmap regeneration policy stated (regenerate against shipping format).
- [x] Validation plan defined.
- [ ] `tools/rhythm_pipeline.py` and `docs/audio-pipeline-spec.md` updated by
      transcode-implementation issue (separate ticket).

---

## 3. TestFlight Beta Program (issue #201)

### 3.1 Cohort & recruitment

| Phase                | Audience                                  | Size target | Channel                |
| -------------------- | ----------------------------------------- | ----------- | ---------------------- |
| **Closed alpha**     | Internal: squad + ≤ 5 trusted friends     | 5–10        | Direct invite (email)  |
| **Closed beta**      | Friends-and-family + design contacts      | 25–50       | Direct invite (email)  |
| **Open beta**        | Public TestFlight link                    | up to 200   | Link in README/social  |

We start at closed alpha and only graduate to the next phase after the
go/no-go gate (§3.5) for the previous phase passes. We do **not** open the
public link until at least one full beta cohort has shipped a green build.

### 3.2 Tester welcome message (template)

> Thanks for testing **Shapeshifter**! It's a rhythm bullet hell — match the
> shape, hit the beat, survive the song.
>
> **Before you start**
> - You'll need iOS 16 or later.
> - Headphones strongly recommended; this is a rhythm game.
> - Play in a quiet place for your first run.
>
> **What we'd love you to try**
> 1. Complete the tutorial (FTUE) — does it teach you the controls?
> 2. Play **each of the 3 songs** at least once.
> 3. Try at least one song on **each difficulty**.
> 4. Use **pause** mid-song. Does it resume cleanly?
> 5. While playing, trigger an **interruption**: an incoming call, Siri, or
>    unplug headphones. Does the game recover?
> 6. Finish a song and read the **results screen** — does the score make sense?
> 7. Play once with the screen rotated / Low Power Mode / Do Not Disturb on.
>
> **How to send feedback**
> - Easiest: in the TestFlight app, tap "Send Beta Feedback" → screenshot +
>   note. This goes straight to us with your build version attached.
> - Crashes are sent automatically — you don't have to do anything.
> - Larger thoughts: reply to the invite email.
>
> **Please don't share** the TestFlight link or screenshots publicly yet.

This template lives here as the canonical version. The actual invite email
copy can paraphrase but must preserve sections "What we'd love you to try"
and "How to send feedback".

### 3.3 Feedback intake

- **Primary:** TestFlight in-app feedback (screenshots + comments + auto build
  version). Apple delivers these into App Store Connect.
- **Secondary:** the invite email reply-to address, for prose-heavy reports.
- **Not used in v1:** public form, Discord, Google Form. Reduces channels to
  triage; revisit at open-beta phase.

Each piece of feedback is mirrored into a GitHub issue within the SLA below
so engineering only watches one queue.

### 3.4 Triage SLA

| Severity        | Definition                                                       | First response | Fix-or-defer decision |
| --------------- | ---------------------------------------------------------------- | -------------- | --------------------- |
| **Sev-1 crash** | Repro crash, or crash on >5% of sessions in MetricKit            | 24 h           | 48 h                  |
| **Sev-1 bug**   | Game unplayable (no audio, can't pass FTUE, save corruption)     | 24 h           | 48 h                  |
| **Sev-2**       | Major feature broken on a subset of devices                      | 48 h           | 1 week                |
| **Sev-3**       | Annoyance, polish, content tuning                                | 1 week         | Next milestone        |
| **Feedback**    | Subjective ("too hard", "love this") — aggregated, not ticketed  | Weekly review  | Per-cohort summary    |

All TestFlight feedback is reviewed at least **weekly** even if no individual
item meets a threshold. Crash logs are reviewed within **48 h** of upload by
the on-call engineer (currently rotates through Hockney / McManus).

### 3.5 Go / No-Go criteria for App Store submission

A TestFlight build is promotable to App Store submission only if **all** of
the following hold across the most recent cohort of ≥ 25 testers and ≥ 7 days
of soak time:

| Gate                                  | Threshold                            | Source                       |
| ------------------------------------- | ------------------------------------ | ---------------------------- |
| Crash-free session rate               | ≥ 99.0 %                             | TestFlight + MetricKit       |
| Crash-free user rate                  | ≥ 98.0 %                             | TestFlight                   |
| FTUE completion rate                  | ≥ 70 %                               | `ftue_completed` event       |
| Open Sev-1 bugs                       | **0**                                | Issue tracker                |
| Open Sev-2 bugs                       | ≤ 3, each with documented workaround | Issue tracker                |
| Audio-interrupt recovery              | 100 % manual test pass on 3 devices  | Manual test matrix           |
| Cold-start time (iPhone 12-class)     | ≤ 2.0 s                              | MetricKit launch metric      |
| Privacy label & ATT                   | Locked (#189, #194)                  | App Store Connect            |
| Build signing & version bump          | Locked (#183, #184)                  | Release checklist            |

Failing any gate → fix, ship a new TF build, restart 7-day soak. The
go/no-go review is owned by Edie (PM) with sign-off from Kobayashi (release)
and Verbal (QA). Decision is recorded in `.squad/decisions.md`.

### 3.6 Acceptance for #201
- [x] Cohort size & recruitment phases decided.
- [x] Welcome message + "what to test" checklist drafted.
- [x] Feedback intake defined (TestFlight in-app + email).
- [x] Triage SLA defined.
- [x] Go/no-go criteria defined.
- [ ] Implementation: create the TestFlight tester group in App Store Connect
      and wire the public link (gated on first signed build, #184).

---

## 4. Cross-cutting dependencies

| This doc decides | Depends on / blocks                                       |
| ---------------- | --------------------------------------------------------- |
| §1 Telemetry     | #189 (privacy labels), #194 (ATT), #182 (lifecycle), #180 (audio session) |
| §2 Audio         | `tools/rhythm_pipeline.py`, `docs/audio-pipeline-spec.md`, raylib audio support on iOS |
| §3 Beta program  | #183 (versioning), #184 (signing), every Sev-1 issue file |

## 5. Revision log
- **v1.0 (2026-04-28)** — Initial decisions for #185, #188, #201. — Edie
