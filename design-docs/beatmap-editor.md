# SHAPESHIFTER — Beatmap Editor
## Design & Implementation Plan · v1.0

> **Format reference:** `rhythm-spec.md` §1 (Beat Map Format)
> **Design guidelines:** `beatmap-design-guidelines.md`
> **Existing beatmaps:** `content/beatmaps/*_beatmap.json`
> **Analysis files:** `content/beatmaps/*_analysis.json`

---

## 1. Goal

A browser-based visual editor for authoring and editing SHAPESHIFTER beatmap JSON files.
The editor runs as a standalone HTML file — no build step, no server, no dependencies beyond
what the browser provides natively.

**Location:** `tools/beatmap-editor/index.html`

---

## 2. Why HTML

- Zero setup — open the file in a browser and start editing
- Web Audio API handles `.flac` playback and waveform rendering natively
- Canvas gives us a fast, zoomable timeline
- File System Access API (or fallback drag-and-drop) for loading audio + JSON
- No C++ build impact, no new vcpkg dependencies
- Can be used by non-programmers (level designers, musicians)

---

## 3. Feature Set

### 3.1 Audio

| Feature | Details |
|---------|---------|
| Load audio | Drag-and-drop or file picker. Supports `.flac`, `.mp3`, `.wav`, `.ogg` |
| Waveform | Rendered to canvas from decoded AudioBuffer |
| Playback | Play / pause / seek. Spacebar toggle. Click-to-seek on waveform |
| Playback rate | 0.25×, 0.5×, 0.75×, 1.0× speed control |
| Metronome | Optional click track on each beat gridline |

### 3.2 Timeline

```
  ┌─ Beat Grid ────────────────────────────────────────────────────┐
  │                                                                │
  │  BPM: 159.01    Offset: 2.27s    Lead: 4 beats                │
  │                                                                │
  │  Beat:  1    2    3    4    5    6    7    8    9   10   11    │
  │         │    │    │    │    │    │    │    │    │    │    │     │
  │  ───────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────    │
  │  Lane 0 │    │    │    │    │    │    │ ■  │    │    │ ▲       │
  │  ───────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────    │
  │  Lane 1 │    │    │    │    │    │    │    │ ●  │    │         │
  │  ───────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────    │
  │  Lane 2 │    │    │    │    │    │    │    │    │ ■  │         │
  │  ───────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────    │
  │                                                                │
  │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  (waveform)  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓   │
  │  ──────────────────────────────────────────────────────────    │
  │                       ▲ playhead                               │
  └────────────────────────────────────────────────────────────────┘
```

| Feature | Details |
|---------|---------|
| Beat grid | Vertical lines at each beat position derived from BPM + offset |
| 3-lane rows | Horizontal rows for lanes 0, 1, 2 |
| Zoom | Scroll-wheel or +/- to zoom in/out horizontally |
| Pan | Click-drag on empty area, or horizontal scroll |
| Playhead | Vertical line synced to audio position during playback |
| Structure overlay | Optional colored regions from analysis file `structure` array |

### 3.3 Obstacle Editing

| Feature | Details |
|---------|---------|
| Place | Click on a lane×beat intersection to place an obstacle |
| Context menu | Right-click an obstacle to open an inline context menu |
| Obstacle palette | Select active obstacle kind before placing (for new placements) |
| Shape selector | For shape_gate/combo_gate/split_path: pick circle/square/triangle |
| Lane block editor | For lane_block: pick which lanes are blocked (bitmask) |
| Drag to move | Drag an obstacle to a different beat or lane |
| Multi-select | Shift-click or box-select, then move/delete as group |
| Copy/paste | Select a range of beats, copy, paste at a different position |

### 3.3.1 Right-Click Context Menu

Right-clicking an obstacle (or an empty lane×beat cell) opens an inline context menu
at the cursor position. The menu is context-sensitive — it shows different options
depending on whether the click target is an existing obstacle or empty space.

**On an existing obstacle:**

```
  ┌─────────────────────────────┐
  │  Change Kind ►              │
  │  ├─ ShapeGate               │
  │  ├─ LaneBlock               │
  │  ├─ LowBar                  │
  │  ├─ HighBar                 │
  │  ├─ ComboGate               │
  │  └─ SplitPath               │
  │─────────────────────────────│
  │  Change Shape ►             │
  │  ├─ ● Circle                │
  │  ├─ ■ Square                │
  │  └─ ▲ Triangle              │
  │─────────────────────────────│
  │  Change Lane ►              │
  │  ├─ Lane 0 (Left)           │
  │  ├─ Lane 1 (Center)         │
  │  └─ Lane 2 (Right)          │
  │─────────────────────────────│
  │  Duplicate                  │
  │  Delete                     │
  └─────────────────────────────┘
```

**On empty space:**

```
  ┌─────────────────────────────┐
  │  Place ShapeGate ►          │
  │  ├─ ● Circle                │
  │  ├─ ■ Square                │
  │  └─ ▲ Triangle              │
  │─────────────────────────────│
  │  Place LaneBlock            │
  │  Place LowBar               │
  │  Place HighBar              │
  │  Place ComboGate ►          │
  │  ├─ ● Circle                │
  │  ├─ ■ Square                │
  │  └─ ▲ Triangle              │
  │  Place SplitPath ►          │
  │  ├─ ● Circle                │
  │  ├─ ■ Square                │
  │  └─ ▲ Triangle              │
  │─────────────────────────────│
  │  Paste                      │
  └─────────────────────────────┘
```

- Submenus expand on hover (or tap on touch)
- The current value is highlighted/checked in each submenu
- Changing kind/shape updates the obstacle in-place (preserves beat + lane)
- Menu dismisses on click-outside or `Escape`
- All context menu actions are undo-able

### 3.4 Obstacle Palette

```
  ┌──────────────────────────────────────────────────┐
  │  Kind:                                           │
  │  [■ ShapeGate] [▬ LaneBlock] [⌐ LowBar]         │
  │  [¬ HighBar]   [◈ ComboGate] [⑂ SplitPath]      │
  │                                                  │
  │  Shape (for shape_gate/combo/split):             │
  │  [● Circle]  [■ Square]  [▲ Triangle]            │
  └──────────────────────────────────────────────────┘
```

### 3.5 Metadata Panel

```
  ┌──────────────────────────────────────────────────┐
  │  Song ID:     [1_stomper         ]               │
  │  Title:       [1_stomper         ]               │
  │  BPM:         [159.01            ]               │
  │  Offset (s):  [2.27              ]               │
  │  Lead Beats:  [4                 ]               │
  │  Duration (s):[200.24            ]               │
  │  Song Path:   [content/audio/1_stomper.flac]     │
  └──────────────────────────────────────────────────┘
```

- BPM and offset changes re-draw the beat grid in real time
- "Tap BPM" button: tap along with the music to detect BPM
- "Set Offset" button: click during playback to mark beat 1

### 3.6 Difficulty Tabs

```
  [ Easy ] [ Medium ] [ Hard ]  [+ Add Difficulty]
```

- Each difficulty has its own `beats` array
- Switching tabs preserves the other difficulties' data
- "Copy from..." to duplicate one difficulty as a starting point

### 3.7 Import / Export

| Feature | Details |
|---------|---------|
| Import beatmap JSON | Load an existing `*_beatmap.json` via file picker or drag-drop |
| Import analysis JSON | Overlay beat timestamps and structure sections from `*_analysis.json` |
| Export beatmap JSON | Download as `{song_id}_beatmap.json` matching the game's schema |
| Auto-validate | Run validation rules from `beat_map_loader.cpp` before export |
| Validation panel | Show errors/warnings inline (e.g., "different-shape gates < 3 beats apart") |

### 3.8 Validation Rules (from `validate_beat_map`)

The editor enforces these in real-time with visual indicators:

1. Beat indices monotonically increasing (auto-sorted)
2. No beat index beyond song duration
3. BPM in [60, 300]
4. Offset in [0.0, 5.0]
5. Lead beats in [2, 8]
6. At least 1 beat entry
7. shape_gate / split_path lane must be 0–2
8. Different-shape gates must be ≥ 3 beats apart

### 3.9 Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Space` | Play / Pause |
| `←` / `→` | Step back / forward 1 beat |
| `Shift+←/→` | Step back / forward 4 beats |
| `1` / `2` / `3` | Select lane 0 / 1 / 2 |
| `Q` / `W` / `E` | Select circle / square / triangle |
| `Enter` | Place obstacle at cursor position |
| `Delete` / `Backspace` | Remove selected obstacle(s) |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `Ctrl+S` | Export / download JSON |
| `Ctrl+C` / `Ctrl+V` | Copy / paste selected obstacles |
| `+` / `-` | Zoom in / out |

---

## 4. Architecture

### 4.1 File Structure

```
tools/beatmap-editor/
├── index.html          ← Shell: DOM layout only (no logic)
├── css/
│   └── editor.css      ← All styles
├── js/
│   ├── main.js         ← Bootstrap: wires modules together, owns render loop
│   ├── state.js         ← Module A: data model, undo/redo, dirty tracking
│   ├── audio.js         ← Module B: Web Audio, waveform, playback
│   ├── timeline.js      ← Module C: canvas rendering (grid, lanes, obstacles, playhead)
│   ├── editor.js        ← Module D: mouse/keyboard interaction on the canvas
│   ├── context-menu.js  ← Module E: right-click context menu
│   ├── panels.js        ← Module F: metadata form, palette toolbar, difficulty tabs
│   ├── io.js            ← Module G: import/export JSON, validation
│   └── constants.js     ← Shared enums, colors, sizes
└── README.md
```

Each `js/` file is a self-contained ES module (`import`/`export`).
No bundler — the browser loads them via `<script type="module">`.
Modules communicate through a shared `state` object and a minimal event bus.

### 4.2 Module Boundaries & Interfaces

```
  ┌─────────────────────────────────────────────────────────────────┐
  │                        main.js (bootstrap)                      │
  │  - Creates state, wires modules, owns requestAnimationFrame    │
  │  - Calls timeline.render() and audio.sync() each frame         │
  └──────────┬──────────┬──────────┬──────────┬──────────┬─────────┘
             │          │          │          │          │
     ┌───────▼──┐ ┌─────▼────┐ ┌──▼───────┐ ┌▼────────┐│
     │ state.js │ │ audio.js │ │timeline.js│ │editor.js ││
     │ Module A │ │ Module B │ │ Module C  │ │Module D  ││
     └───────┬──┘ └──────────┘ └──────────┘ └─────┬────┘│
             │                                     │     │
     ┌───────▼──────────────────────────────────┐  │     │
     │  context-menu.js │ panels.js │   io.js   │  │     │
     │    Module E      │ Module F  │ Module G  │◄─┘     │
     └──────────────────┴───────────┴───────────┘        │
             ▲                                           │
             └───────── constants.js ────────────────────┘
```

### 4.3 Module Contracts

Each module exports a well-defined API. Sub-agents build to these contracts.

---

#### Module A — `state.js` (Data Model)

**Owner of all beatmap data. Single source of truth.**

```javascript
// ── Shared State Object ─────────────────────────────
// Modules read from state freely. Mutations MUST go through exported functions.

export const state = {
  // Beatmap data (exported to JSON)
  songId: "", title: "", bpm: 120, offset: 0, leadBeats: 4,
  duration: 180, songPath: "",
  difficulties: { easy: { beats: [] }, medium: { beats: [] }, hard: { beats: [] } },
  activeDifficulty: "easy",

  // Editor-only state (not exported)
  audioBuffer: null,
  zoom: 1.0,           // pixels per beat
  scrollX: 0,          // horizontal scroll in pixels
  playheadTime: 0,     // current audio time in seconds
  playing: false,
  cursor: { beat: -1, lane: -1 },  // hovered grid cell
  selectedIndices: [],  // indices into active beats array
  tool: { kind: "shape_gate", shape: "circle" },
  analysisData: null,
};

// ── Beat Entry shape ────────────────────────────────
// { beat: int, kind: string, shape: string, lane: int, blocked?: int[] }

// ── Exported mutation functions ─────────────────────
export function getActiveBeats()                          → BeatEntry[]
export function addBeat(entry: BeatEntry)                 → void
export function removeBeat(index: int)                    → void
export function updateBeat(index: int, fields: Partial)   → void
export function setActiveDifficulty(name: string)         → void
export function addDifficulty(name: string)               → void
export function copyDifficulty(from: string, to: string)  → void
export function setMetadata(fields: Partial)              → void

// ── Undo / Redo ─────────────────────────────────────
export function pushUndo(description: string)             → void  // snapshot before mutation
export function undo()                                    → void
export function redo()                                    → void
export function canUndo() → bool
export function canRedo() → bool

// ── Event bus (pub/sub) ─────────────────────────────
export function on(event: string, callback: fn)           → void
export function emit(event: string, data?: any)           → void

// Events emitted:
//   "beats-changed"       — beats array was mutated
//   "metadata-changed"    — bpm/offset/duration/etc changed
//   "difficulty-changed"  — active difficulty switched
//   "selection-changed"   — selectedIndices changed
//   "tool-changed"        — active palette tool changed
//   "cursor-changed"      — hovered grid cell changed
```

---

#### Module B — `audio.js` (Audio Engine)

**Owns Web Audio API. Provides playback control and waveform data.**

```javascript
export async function loadAudioFile(file: File)     → Promise<AudioBuffer>
export function play()                               → void
export function pause()                              → void
export function seek(timeSeconds: float)             → void
export function getCurrentTime()                     → float
export function isPlaying()                          → bool
export function setPlaybackRate(rate: float)         → void
export function getWaveformData(width: int)          → Float32Array  // downsampled peaks
export function getDuration()                        → float
export function setMetronome(enabled: bool, bpm, offset) → void
```

Emits on state event bus: none (polled by main.js each frame).

---

#### Module C — `timeline.js` (Canvas Renderer)

**Pure rendering — reads state, draws to canvas. No mutation.**

```javascript
export function init(canvas: HTMLCanvasElement)       → void
export function render(state)                         → void  // called each frame

// Render responsibilities:
//   - Waveform (from audio.getWaveformData)
//   - Beat grid (vertical lines from bpm + offset)
//   - Lane rows (3 horizontal bands)
//   - Obstacle glyphs (shape icons per beat entry)
//   - Playhead (vertical line at current time)
//   - Selection highlight (on selectedIndices)
//   - Cursor highlight (hovered cell)
//   - Validation error markers (red outlines)
//   - Analysis overlay (structure sections, onset markers)

// Hit testing (used by editor.js for mouse interaction)
export function hitTest(canvasX, canvasY) → { beat: int, lane: int, obstacleIndex: int|null }
export function beatToX(beat: int)        → float   // beat index → canvas X
export function xToBeat(x: float)         → int     // canvas X → nearest beat index
export function timeToX(seconds: float)   → float   // audio time → canvas X
```

---

#### Module D — `editor.js` (Interaction Handler)

**Translates mouse/keyboard events into state mutations.**

```javascript
export function init(canvas, state, timeline, contextMenu)  → void

// Handles:
//   - Left-click on empty cell → addBeat with current tool
//   - Left-click on obstacle → select it
//   - Right-click → open context menu (delegates to Module E)
//   - Shift-click → multi-select
//   - Mouse-move → update cursor position
//   - Drag → move obstacle(s) to new beat/lane
//   - Keyboard shortcuts (space, arrows, 1/2/3, Q/W/E, del, ctrl+z, etc.)
//   - Scroll wheel → zoom
//   - Middle-click drag or shift+scroll → pan
```

---

#### Module E — `context-menu.js` (Context Menu)

**Standalone DOM component. Renders and handles the right-click menu.**

```javascript
export function init(container: HTMLElement)                          → void
export function show(x, y, { beat, lane, obstacleIndex, state })     → void
export function hide()                                                → void
export function isVisible()                                           → bool

// Menu builds itself from the arguments:
//   obstacleIndex !== null → "Change Kind / Shape / Lane / Duplicate / Delete"
//   obstacleIndex === null → "Place ShapeGate▸ / LaneBlock / LowBar / ..."
// Selecting a menu item calls the appropriate state.* mutation function.
```

---

#### Module F — `panels.js` (DOM Panels)

**Metadata form, obstacle palette toolbar, difficulty tabs.**

```javascript
export function init(state)  → void

// Binds DOM inputs to state:
//   - Metadata fields (songId, title, bpm, offset, leadBeats, duration, songPath)
//   - Palette buttons (kind selector, shape selector)
//   - Difficulty tabs (easy/medium/hard + add/copy)
//   - Tap BPM button
//   - Import/export buttons (delegates to io.js)
//   - Validation error list panel

// Listens to state events to update DOM when state changes programmatically
// (e.g., after import or undo).
```

---

#### Module G — `io.js` (Import / Export / Validation)

**File I/O and beatmap validation. Pure functions, no DOM.**

```javascript
export function importBeatmap(jsonString: string)     → { data: object, errors: string[] }
export function exportBeatmap(state)                   → string  // JSON
export function importAnalysis(jsonString: string)     → object
export function validate(state)                        → ValidationError[]
export function downloadFile(filename, content)        → void  // triggers browser download

// ValidationError = { beatIndex: int, message: string, severity: "error"|"warning" }
//
// Validation rules (from beat_map_loader.cpp validate_beat_map):
//   1. Beat indices monotonically increasing
//   2. No beat index beyond song duration
//   3. BPM in [60, 300]
//   4. Offset in [0.0, 5.0]
//   5. Lead beats in [2, 8]
//   6. At least 1 beat entry
//   7. shape_gate / split_path lane must be 0–2
//   8. Different-shape gates must be ≥ 3 beats apart
```

---

#### `constants.js` (Shared Constants)

```javascript
export const OBSTACLE_KINDS = ["shape_gate","lane_block","low_bar","high_bar","combo_gate","split_path"];
export const SHAPES = ["circle", "square", "triangle"];
export const LANES = [0, 1, 2];
export const KINDS_WITH_SHAPE = ["shape_gate", "combo_gate", "split_path"];
export const COLORS = { /* per-kind and per-shape colors */ };
export const GLYPHS = { /* unicode/emoji per obstacle kind */ };
export const DEFAULT_BPM = 120;
export const DEFAULT_ZOOM = 40; // pixels per beat
```

---

### 4.4 Internal Data Model

```javascript
// Mirrors the C++ BeatMap / BeatEntry structs exactly
// (see Module A state definition above for full shape)
```

### 4.5 Export Format

Exactly matches what `beat_map_loader.cpp` expects:

```json
{
  "song_id": "1_stomper",
  "title": "1_stomper",
  "bpm": 159.01,
  "offset": 2.27,
  "lead_beats": 4,
  "duration_sec": 200.24,
  "song_path": "content/audio/1_stomper.flac",
  "difficulties": {
    "easy":   { "beats": [{ "beat": 8, "kind": "shape_gate", "shape": "square", "lane": 1 }] },
    "medium": { "beats": [] },
    "hard":   { "beats": [] }
  }
}
```

---

## 5. Implementation Plan — Parallelizable Work Units

### 5.0 Dependency Graph

```
  Work Unit 0: HTML Shell + CSS + constants.js
       │
       ├──────────────────┬──────────────────┬──────────────────┐
       ▼                  ▼                  ▼                  ▼
  WU 1: state.js     WU 2: audio.js    WU 3: io.js        WU 4: timeline.js
  (data + events)    (Web Audio)        (import/export/     (canvas render)
       │                  │              validation)              │
       │                  │                  │                    │
       └──────────┬───────┴──────────────────┘                   │
                  ▼                                              │
          WU 5: panels.js  ◄─────────────────────────────────────┘
          (DOM panels, wires io.js)
                  │
       ┌──────────┴──────────┐
       ▼                     ▼
  WU 6: editor.js      WU 7: context-menu.js
  (mouse/keyboard)      (right-click menu)
       │                     │
       └──────────┬──────────┘
                  ▼
          WU 8: main.js
          (bootstrap, render loop, integration)
```

### 5.1 Work Units

Each work unit (WU) can be assigned to a separate sub-agent.
Agents with no dependency arrows between them can run **in parallel**.

---

#### **WU 0 — HTML Shell + CSS + Constants** (foundation, must be first)

Build the empty scaffolding that all other modules plug into.

- `index.html` — DOM layout: toolbar, metadata panel, difficulty tabs, canvas container,
  palette sidebar, validation panel, context menu container. No logic — just elements with IDs.
- `css/editor.css` — dark theme, flexbox layout, panel styling, canvas sizing, context menu styles.
- `js/constants.js` — shared enums, colors, obstacle kinds, shapes, default values.

**Outputs:** DOM element IDs that other modules reference. CSS classes. Constant imports.

---

#### **WU 1 — `state.js`** (can parallel with WU 2, 3, 4)

Depends on: constants.js

- State object with all fields
- Mutation functions: addBeat, removeBeat, updateBeat, setActiveDifficulty, etc.
- Undo/redo stack (snapshot-based)
- Pub/sub event bus: on(), emit()
- Unit-testable in isolation (no DOM, no canvas)

---

#### **WU 2 — `audio.js`** (can parallel with WU 1, 3, 4)

Depends on: constants.js

- AudioContext creation, file loading via FileReader + decodeAudioData
- Play/pause/seek/getCurrentTime
- Playback rate control
- Waveform peak extraction (downsample AudioBuffer to array of peaks for rendering)
- Metronome (oscillator scheduled on beat times)

---

#### **WU 3 — `io.js`** (can parallel with WU 1, 2, 4)

Depends on: constants.js

- `importBeatmap(json)` — parse JSON, normalize to internal state shape
- `exportBeatmap(state)` — serialize state to JSON matching `beat_map_loader.cpp` format
- `importAnalysis(json)` — parse analysis file, extract structure/onsets/beats
- `validate(state)` — all 8 validation rules, returns error array
- `downloadFile(name, content)` — Blob + URL.createObjectURL + click trick
- Pure functions, no side effects, easy to test

---

#### **WU 4 — `timeline.js`** (can parallel with WU 1, 2, 3)

Depends on: constants.js

- Canvas init, DPI scaling
- `render(state, waveformData)` — full redraw each frame:
  - Background + lane bands
  - Beat grid lines (from bpm + offset + zoom + scrollX)
  - Obstacle glyphs (colored icons per kind/shape at correct beat×lane)
  - Selection highlights
  - Cursor highlight (hovered cell)
  - Playhead line
  - Validation error markers (red outline on bad obstacles)
  - Analysis overlay (colored section bars, onset tick marks)
- `hitTest(x, y)` — returns { beat, lane, obstacleIndex }
- Coordinate conversion helpers: beatToX, xToBeat, timeToX

---

#### **WU 5 — `panels.js`** (after WU 1, 3)

Depends on: state.js, io.js, constants.js

- Bind metadata form inputs ↔ state (two-way: edit updates state, import updates form)
- Palette toolbar: kind buttons + shape buttons → updates state.tool
- Difficulty tabs: click to switch, "+ Add" button, "Copy from..." dropdown
- Import button: file picker → io.importBeatmap → load into state
- Export button: io.exportBeatmap → io.downloadFile
- Audio load button: file picker → delegates to audio.loadAudioFile
- Validation panel: subscribe to "beats-changed", run io.validate, show error list
- Tap BPM helper: records tap timestamps, computes average interval

---

#### **WU 6 — `editor.js`** (after WU 1, 4)

Depends on: state.js, timeline.js, constants.js

- Attach mouse event listeners to canvas
- Left-click: hitTest → select or place obstacle
- Right-click: preventDefault, delegate to context-menu.show()
- Mouse-move: update state.cursor
- Drag: track dragStart/dragEnd, move obstacles
- Shift-click: toggle multi-select
- Scroll-wheel: adjust state.zoom
- Middle-drag / shift+scroll: adjust state.scrollX
- Keyboard handler (attached to window):
  - Space → audio.play/pause
  - ←/→ → step cursor beat
  - 1/2/3 → select lane
  - Q/W/E → select shape
  - Enter → place at cursor
  - Del/Backspace → remove selected
  - Ctrl+Z / Ctrl+Shift+Z → undo/redo
  - Ctrl+S → export
  - +/- → zoom

---

#### **WU 7 — `context-menu.js`** (after WU 1)

Depends on: state.js, constants.js

- Pure DOM component (no canvas)
- `show(x, y, context)` — build menu items dynamically:
  - If obstacle exists at click: Change Kind ▸, Change Shape ▸, Change Lane ▸, Duplicate, Delete
  - If empty: Place ShapeGate ▸ (circle/square/triangle), Place LaneBlock, etc.
- Submenus on hover with current value checkmarked
- Click handler calls state mutation functions
- `hide()` on click-outside or Escape
- CSS-positioned absolutely at cursor coordinates

---

#### **WU 8 — `main.js`** (final integration, after all others)

Depends on: all modules

- Import all modules
- Create shared state
- Call init() on each module with DOM elements + state
- requestAnimationFrame loop:
  - `state.playheadTime = audio.getCurrentTime()`
  - `timeline.render(state, audio.getWaveformData(...))`
- Wire up cross-module interactions not handled by event bus
- Handle window resize → canvas resize

---

### 5.2 Parallelism Map

```
  TIME ──────────────────────────────────────────────►

  Agent 0: ██ WU 0 (shell + CSS + constants) ██
                │
                ├─── Agent 1: ██ WU 1 (state.js)     ██
                ├─── Agent 2: ██ WU 2 (audio.js)     ██
                ├─── Agent 3: ██ WU 3 (io.js)        ██
                └─── Agent 4: ██ WU 4 (timeline.js)  ██
                                    │
                     Agent 5: ██ WU 5 (panels.js)        ██
                     Agent 6: ██ WU 6 (editor.js)        ██
                     Agent 7: ██ WU 7 (context-menu.js)  ██
                                         │
                              Agent 8: ██ WU 8 (main.js — integration) ██
```

**Round 1 (sequential):** WU 0 — one agent builds the scaffold  
**Round 2 (4 parallel agents):** WU 1 + WU 2 + WU 3 + WU 4  
**Round 3 (3 parallel agents):** WU 5 + WU 6 + WU 7  
**Round 4 (sequential):** WU 8 — integration + smoke test  

---

## 6. Sub-Agent Task Tracking

Each sub-agent **must** create and maintain a task file at:

```
tasks/wu{N}_{module_name}.md
```

Examples: `tasks/wu1_state.md`, `tasks/wu4_timeline.md`, `tasks/wu8_integration.md`

### 6.1 Task File Format

Every task file must follow this template:

```markdown
# WU{N}: {Module Name}

## Status: {not_started | in_progress | blocked | done}

## Scope
Brief description of what this work unit delivers.

## Checklist
- [ ] Item 1
- [ ] Item 2
- [ ] Item 3

## Files Created / Modified
- `tools/beatmap-editor/js/{module}.js` — created
- `tools/beatmap-editor/index.html` — modified (added X)

## Dependencies
- Depends on: WU 0 (constants.js, index.html DOM IDs)
- Blocked by: (none, or describe what's blocking)

## API Contract
Paste the exported function signatures this module implements
(copy from §4.3 of beatmap-editor.md).

## Notes / Decisions
Log any design decisions, deviations from the spec, or open questions here.
```

### 6.2 Update Rules

| When | What to update |
|------|----------------|
| **Starting work** | Create the file, set Status to `in_progress` |
| **Completing a checklist item** | Check it off (`- [x]`), add file paths to Files section |
| **Hitting a blocker** | Set Status to `blocked`, describe in Dependencies section |
| **Making a design decision** | Add to Notes / Decisions |
| **Finishing all work** | Set Status to `done`, verify all checklist items checked |

### 6.3 Rules for Sub-Agents

1. **Create your task file first** before writing any code.
2. **Update the checklist** as you complete each item — don't batch updates at the end.
3. **Don't modify other agents' files.** If you need a change to a dependency (e.g., a new event in state.js), document it in your Notes section. The orchestrator or integration agent (WU 8) will reconcile.
4. **Code to the contract.** Use the exported API signatures from §4.3 exactly. If you need to add a function, document it in your task file and use it — WU 8 will wire it up.
5. **Don't import from modules you don't depend on.** Check the dependency graph in §5.0.

---

## 7. Non-Goals (for now)

- No auto-generation of beatmaps (that's `rhythm_pipeline.py` + `level_designer.py`)
- No in-game editor integration (this is a standalone offline tool)
- No server/backend — everything runs client-side
- No waveform editing or audio processing
