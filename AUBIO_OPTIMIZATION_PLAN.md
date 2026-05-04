# Aubio Accuracy Optimization Plan
## Ground Truth Datasets for Beat Tracking Benchmarking

**Author:** Fenton (Audio Expert)  
**Date:** 2026-05-01  
**Scope:** Identify, evaluate, and rank human-annotated beat/BPM datasets for aubio parameter tuning in SHAPESHIFTER repo.

---

## 1. Candidate Datasets: Analysis & Comparison

| Source | Human Annotation Type | Audio Access | License | Fit for Repo | Key Caveats |
|--------|----------------------|---------------|---------|--------------|------------|
| **BALLROOM** | Manual beat/downbeat taps; BPM consensus | Direct download (50 tracks, ~4 min each) | Public domain | ✅ **Excellent** | Standard ballroom only; steady tempos; not generalizable |
| **CARNATIC** | Manually annotated beat/sam/cycle | Download via Univ. Bristol (academic) | Research use; unclear redistribution | ⚠️ **Feasible** | Indian classical; tempo highly variable; niche applicability |
| **HAINSWORTH Beat Annotation Corpus** | Manual beat/downbeat taps by trained annotators | Email request to author (David Hainsworth) | Custom licensing per request | ⚠️ **Requires negotiation** | ~465 tracks, high quality; unclear commercial/repo use terms |
| **MEDIAEVAL Beat Tracking Task (2013–2019)** | Multi-annotator beat/downbeat consensus | Registration + download (academic DB) | Non-commercial research use | ⚠️ **Conditional** | Diverse genres; small (~100 tracks); requires academic affiliation |
| **AES Multitrack Music Stems Collection** | Limited BPM/beat data; mostly production metadata | Commercial; Spotify API | Restrictive; audio not redistributable | ❌ **Not viable** | License prohibits redistribution; data sparse |
| **AIST Dance Music Database (DMD)** | Manual beat/downbeat; BPM consensus | Download via request (U. Tsukuba) | Research use; unclear redistribution terms | ⚠️ **Possible** | 460+ tracks; dance/electronic bias; audio redistribution unclear |
| **Beat Tracking Challenge (ISMIR/MIREX)** | Annotated beats + evaluation code | Registration required; sample provided | Non-commercial research | ⚠️ **Limited** | Only sample tracks publicly available; full dataset via ISMIR membership |
| **Audio Aesthetics Research Dataset (AAC)** | Production metadata; minimal beat annotations | Request from authors | Custom licensing | ❌ **Not suitable** | Weak beat/BPM coverage; focus on production tags |
| **Spotify URI metadata + Expert Tagging** | BPM via API; beats from trainable models | Spotify API key required | Spotify ToS; audio access restrictive | ❌ **Not viable** | No direct beat truth; audio streaming DRM-protected |
| **YouTube Music (via Genius/Musixmatch)** | Crowdsourced lyrics; sparse beat data | Indirect; requires scraping | ToS violation risk | ❌ **Not viable** | No structured beat truth; copyright enforcement |
| **Custom Annotation Project (Ad-hoc)** | Manual annotation via labeling tool | User-supplied | User-owned | ✅ **Control** | Time-intensive; quality depends on annotators; limited scale (10–30 tracks) |

---

## 2. Realistic Dataset Candidates for SHAPESHIFTER Repo (Immediate Use)

### Tier 1: Directly Usable (Low Legal Risk, Downloadable)

**BALLROOM Database** *(Recommended primary source)*
- **Source:** https://www.audiocontentanalysis.org/ (formerly TU Darmstadt)
- **What you get:** 50 ballroom dance tracks (waltz, foxtrot, tango, etc.); ~4–5 min each; manual beat annotations + BPM consensus
- **Format:** WAV, 44.1 kHz; annotations as plain text (beat times in seconds)
- **License:** Public domain / CC0 (clarify with host)
- **Redistribution:** ✅ Allowed
- **Why fit:** High-quality human beat taps; steady tempos ideal for baseline testing; publicly available; no licensing friction
- **Caveat:** All steady ballroom tempos; not representative of dynamic/genre-varied beatmaps

**Custom Benchmark Pack (Opt-In User Collection)**
- **Source:** Users contribute analyzed tracks they own
- **Format:** User supplies WAV/MP3 + manual beat annotations (via simple labeling tool, e.g., Sonic Visualiser or web UI)
- **License:** User-licensed; non-commercial research use default
- **Redistribution:** Per-track; disclosed in metadata
- **Why fit:** Fully controlled; avoids legal ambiguity; allows gradual growth
- **Caveat:** Requires community participation; annotation quality variance; slower to scale

---

### Tier 2: Possible with Negotiation (Academic/Research Use)

**HAINSWORTH Corpus** *(High quality, requires outreach)*
- **Contact:** David Hainsworth (Queen Mary University of London)
- **What you get:** ~465 classical/pop/rock/jazz tracks; multi-annotator consensus beats + downbeats; ground truth BPM
- **Format:** MIDI beat times + BPM; audio sourcing unclear (likely copyrighted)
- **License:** Custom; typically free for academic research, restrictions on commercial/public redistribution
- **Why pursue:** Largest human-annotated beat corpus; diverse genres; proven in MIR research
- **Risk:** Unclear repo applicability; may not allow public dataset redistribution

**MEDIAEVAL Beat Tracking Task Dataset**
- **Source:** https://multimediaeval.org/ (archive; task ended 2019)
- **What you get:** ~100 diverse tracks; annotated beats + downbeats; multi-annotator consensus
- **License:** Non-commercial research use
- **Redistribution:** Unclear; likely restricted to academic context
- **Why pursue:** Diverse genres; established evaluation protocol
- **Risk:** Small size; licensing ambiguity for open-source repo

---

### Tier 3: Not Recommended (High Risk / Impractical)

- **AES, AAC, Spotify API:** Audio redistribution prohibited or metadata too sparse
- **YouTube/Genius:** Copyright enforcement + ToS violations
- **ISMIR Full Archives:** Full dataset access restricted to members; licensing complex

---

## 3. Recommended Benchmark Pack: 15–20 Tracks

### Strategy
Start with **BALLROOM** (publicly available, low risk) + **custom user submissions** (growing library). Defer HAINSWORTH negotiation until pack proves useful.

### Initial Ballroom Subset (Tier 1, Immediate)
- **5 Waltz tracks:** Steady 3/4, ~84–90 BPM (baseline)
- **3 Foxtrot tracks:** Steady 4/4, ~120–130 BPM
- **2 Tango tracks:** Syncopated, ~120–135 BPM (onset complexity)
- **2 Quickstep tracks:** Fast 4/4, ~160–180 BPM
- **2 Slowfox tracks:** Lyrical, ~95–105 BPM

**Expected Metadata per Track:**
```
filename:           ballroom_waltz_01.wav
bpm:                86.5
meter:              3/4
style:              Waltz
duration_sec:       272.4
beats:              [timestamps in seconds, one per line]
  0.465
  0.930
  1.395
  ...
downbeats:          [subset of beat timestamps marking bar onsets]
  0.0
  1.395
  2.790
  ...
annotators:         2–3 trained dancers (consensus)
annotation_tool:    Sonic Visualiser / manual tapping
sample_rate:        44100
```

### Growth: Community Submissions (Optional Tier)
- Genre targets: Electronic, rock, pop, hip-hop, world music (to expand beyond ballroom)
- Format: User uploads WAV/MP3 + beat annotation (simple web form or GitHub issue template)
- Curation: Basic QA (tempo within range, annotation count ≥ 2, no copyright flags)

---

## 4. Evaluation Protocol: Aubio vs. Ground Truth

### 4.1 Annotation Normalization

**Input formats to support:**
1. **Plain text:** One timestamp per line (seconds, decimal)
2. **Sonic Visualiser:** .lab file (tab-separated: start_sec, end_sec, label)
3. **MIREX format:** Downbeat/beat distinction in separate columns
4. **MIDI:** Convert via librosa or onset detection

**Normalization steps:**
- Convert all to **beat times (list of floats, seconds)**
- Separate **downbeats** if annotated; else treat as every N beats per meter
- **Remove silence margins:** Exclude first/last 500ms
- **Deduplicate:** Merge events within 20ms (annotation jitter)

### 4.2 Aubio Execution Configs

**Current pipeline usage** (from `rhythm_pipeline.py`):
```bash
aubio tempo <file.wav>          # Returns BPM estimate
aubio beat <file.wav>           # Returns beat times (seconds)
aubio onset -m <method> -t <threshold> <file.wav>  # Onset detection
```

**Parameters to sweep:**
- `aubio beat` options:
  - `--hop-size` (frame step, default 512; smaller = higher temporal precision, slower)
  - `--buf-size` (analysis window, default 2048; affects frequency resolution)
  - Detection method: peak-picking (default), Viterbi (if available)
  
- `aubio onset` sweep:
  - `-m` (method): `default`, `specdiff`, `complex` (frequency-based), `kl` (KL divergence), `mkl`, `phase`
  - `-t` (threshold): 0.1–0.9 (lower = more sensitive, more false positives)
  
- `aubio tempo` options:
  - `--with-jack` (off by default; affects precision on some systems)

**Coarse search strategy:**
1. Fix `hop-size=512, buf-size=2048` (defaults)
2. Grid search `aubio beat` detection method + threshold
3. For each, evaluate vs. ground truth
4. For winners, fine-tune `hop-size` (±256)

### 4.3 Event Matching Rules

**Tolerance windows:**
- **Default (loose):** ±70ms (common in MIR literature; ~0.5 frame at 44kHz, hop=512)
- **Strict (high-quality):** ±50ms
- **Permissive (baseline):** ±150ms

**Match logic:**
- One-to-one matching; greedy nearest-neighbor within tolerance
- **False positives:** Predicted events with no ground truth match
- **False negatives:** Ground truth events without predicted match
- **Double matches:** Penalize (e.g., two predictions matching one truth)

### 4.4 Metrics: Beat-Level Evaluation

For each track and tolerance level:
```
Precision  = TP / (TP + FP)
Recall     = TP / (TP + FN)
F1         = 2 * (P * R) / (P + R)
MAE        = Mean(|predicted_beat_i - truth_beat_i|) [seconds, over matched pairs]
Signed Bias = Mean(predicted_beat_i - truth_beat_i) [tracks overall tempo bias]
```

**Per-genre aggregation:**
- Macro-average: Mean(F1 per track)
- Weighted-average: Sum(F1 * track_weight) [weight by genre or difficulty]

**Drift detection:**
- Split track into thirds; compute F1 per section
- Flag if drift(section_1, section_3) > ±0.1 BPM equivalent

### 4.5 BPM Evaluation

```
BPM_MAE    = Mean(|predicted_BPM - truth_BPM|) [ground truth BPM = 60 / mean_beat_interval]
BPM_Bias   = Mean(predicted_BPM - truth_BPM)
Octave Error = % of predictions at BPM/2 or BPM*2 (common aubio artifact)
```

---

## 5. Parameter-Tuning Loop Strategy

### Phase 1: Coarse Search (Days 1–2)
**Goal:** Identify top 3–5 aubio configurations.

1. **BPM estimation baseline:**
   - Run `aubio tempo` on all 15 BALLROOM tracks (no tuning; just measure baseline MAE)
   
2. **Beat tracking sweep:**
   - Test on 5 waltz tracks (simplest case; steady tempo, clear beats)
   - Grid: `hop_size=[256, 512, 1024]` × `method=[default, specdiff, complex]` × `threshold=[0.2, 0.3, 0.5]`
   - Total combinations: 27; ~5 min per track = ~2 hours
   - **Winner selection:** Highest F1 at ±70ms tolerance
   
3. **Quick filter:** Discard configs with F1 < 0.6 on waltz baseline

### Phase 2: Fine-Tune Winners (Days 2–3)
**Goal:** Optimize top 3 configs across full BALLROOM dataset.

1. Evaluate each winner on **all 15 BALLROOM tracks**
2. Compute macro-average F1 + BPM accuracy
3. Threshold tuning: For each winner, sweep `-t` in finer increments (±0.05 around coarse choice)
4. Metric: F1 @ ±50ms (strict) and ±70ms (loose)

### Phase 3: Genre Validation (Optional; if HAINSWORTH/MEDIAEVAL negotiated)
**Goal:** Check robustness to non-ballroom music.

1. Apply winner configs to diverse tracks (if dataset available)
2. Compute per-genre F1 scores
3. If genre variance high (>±0.15 F1), flag config as potentially overfitted to ballroom

### Phase 4: Regression & Deployment (Day 4)
1. Lock winner config in `rhythm_pipeline.py`
2. Document: BPM MAE, beat F1, best tolerance, test set
3. Add A/B test mode (toggle old vs. new params; log metrics to JSON)

---

## 6. Identified Risks & Mitigation

| Risk | Impact | Mitigation |
|------|--------|-----------|
| **Dataset license mismatch** | Legal liability; can't redistrib. | Use BALLROOM first (public domain); defer HAINSWORTH until licensing confirmed via email |
| **Annotation quality variance** | Inconsistent ground truth | Multi-annotator consensus (≥2 annotators); flag outliers (beat times >100ms apart); consider per-annotator agreement metrics |
| **Genre overfitting** | Ballroom-optimized params fail on synth/rock | Validate on ≥2 diverse genres; accept that ballroom params may not generalize |
| **Tempo drift over time** | Aubio output shifts mid-track | Check MAE per track section (thirds); flag if >0.15 BPM drift; may indicate onset instability |
| **Sample rate / frame-step artifacts** | 44.1 kHz vs. 48 kHz; hop_size mismatch | Normalize all audio to 44.1 kHz before testing; document aubio hop_size semantics (samples vs. ms) |
| **Aubio version lock** | Future aubio upgrades break params | Document aubio version (current: 0.4.x); pin in CI; test against 0.5.x (beta) if considering upgrade |
| **Insufficient benchmark size** | 15 tracks too small for robust tuning | Plan growth to ≥30 tracks; accept current set as MVP; use stratification (tempo, meter, style buckets) |

---

## 7. Execution Checklist: "How We Do This Now"

### Week 1: Setup

- [ ] **Download BALLROOM dataset**
  - URL: https://www.audiocontentanalysis.org/research/
  - Verify: 50 .wav files + annotation metadata
  - Store in `./data/benchmark/ballroom/` with `.txt` ground truth files
  
- [ ] **Create annotation schema**
  - Define JSON format: `{ "filename": "...", "bpm": 86.5, "meter": "3/4", "beats": [0.465, ...], "downbeats": [...], "annotators": [...] }`
  - Create schema validator (simple JSON schema or Python dataclass)

- [ ] **Set up evaluation framework** (Python script outline; no coding, just structure)
  - Input: `(predicted_beats, predicted_bpm, ground_truth_file)`
  - Output: JSON `{ "f1_70ms": 0.92, "f1_50ms": 0.88, "mae_bpm": 2.3, "bias_bpm": 0.5, "drift": 0.08 }`
  - Metrics: F1, MAE, bias, drift (no code yet; define algorithm step-by-step)

### Week 2: Coarse Search

- [ ] **Baseline aubio run**
  - `aubio tempo` on all 15 BALLROOM tracks; log BPM predictions → compare vs. annotated ground truth BPM
  - Export results to CSV
  
- [ ] **Grid search beat tracking**
  - Config: 27 (hop_size × method × threshold) × 5 waltz tracks
  - Log F1 @ ±70ms for each config
  - Identify top 3 configs (highest F1 mean)

- [ ] **Filter & document**
  - Create summary table: config | avg_F1 | avg_BPM_MAE | notes
  - Flag any config with F1 < 0.6 as "unlikely"

### Week 3: Fine-Tune & Validate

- [ ] **Fine-tune on full BALLROOM**
  - Run top 3 winners on all 15 tracks
  - Compute macro-average F1 @ ±70ms and ±50ms
  - Rank by (F1 @ ±70ms, then BPM_MAE tiebreaker)

- [ ] **Winner selection**
  - Choose single best config OR declare "no significant improvement" if winner F1 < baseline + 0.05

- [ ] **Threshold sweep (optional)**
  - If winner is close to two candidates, fine-tune `-t` parameter ±0.02

### Week 4: Integration & Documentation

- [ ] **Update `rhythm_pipeline.py`**
  - Hardcode winning aubio params (or add --tune-mode flag for A/B testing)
  - Add comments: "Optimized for BALLROOM BPM/beat accuracy; see AUBIO_OPTIMIZATION_PLAN.md"

- [ ] **Log results**
  - Document: BPM MAE, beat F1 @ standard tolerances, test set size, aubio version, assumptions

- [ ] **Plan next phase (optional)**
  - If HAINSWORTH negotiated: timeline for re-tuning on diverse genres
  - Else: plan for community-submitted benchmark pack growth

---

## 8. Risk Mitigation: Specific Decisions

**Q: What if BALLROOM alone isn't representative?**  
A: Accept it as MVP baseline. Immediate next step: collect 5–10 user-submitted beats from diverse genres (hip-hop, electronic, rock). Retune if macro-genre F1 variance > ±0.15.

**Q: What if aubio params drift significantly on different sample rates?**  
A: Normalize all audio to 44.1 kHz before benchmarking. Document in evaluation protocol: "All ground truth and aubio testing performed at 44.1 kHz, 512-sample hop-size."

**Q: What if we can't get HAINSWORTH/MEDIAEVAL datasets?**  
A: Start with BALLROOM (guaranteed). Design framework to import custom annotations (simple JSON schema). Invite community contribution.

**Q: Who certifies annotation quality?**  
A: Multi-annotator consensus (≥2 independent annotators). Flag disagreements > 100ms as "disputed beats"; exclude from evaluation or mark as uncertain.

---

## Recommended Immediate Next Action

**This week:** Contact BALLROOM dataset host (ISMIR/TU Darmstadt) to confirm public domain license + redistribution rights; download full 50-track set. In parallel, create a simple Python dataclass for annotation schema and write one sample evaluation function (F1 computation only; no aubio integration yet). By end of week, have a populated `./data/benchmark/ballroom/` directory with at least 5 annotated tracks ready for testing. This de-risks the entire project: if licensing is clear and metadata imports cleanly, proceed to coarse grid search immediately; if licensing is ambiguous, pivot to custom annotation workflow + BALLROOM baseline as reference only.

