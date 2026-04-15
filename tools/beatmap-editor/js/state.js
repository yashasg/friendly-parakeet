// state.js — Single source of truth for all beatmap editor data.
// Pure data logic: no DOM, no canvas.

import { DEFAULT_BPM, DEFAULT_ZOOM } from "./constants.js";

// ── Event Bus ───────────────────────────────────────
const listeners = {};

export function on(event, callback) {
  if (!listeners[event]) listeners[event] = [];
  listeners[event].push(callback);
}

export function off(event, callback) {
  const list = listeners[event];
  if (!list) return;
  const idx = list.indexOf(callback);
  if (idx !== -1) list.splice(idx, 1);
}

export function emit(event, data) {
  const list = listeners[event];
  if (!list) return;
  for (const cb of list) cb(data);
}

// ── Shared State Object ─────────────────────────────
export const state = {
  // Beatmap data (exported to JSON)
  songId: "",
  title: "",
  bpm: DEFAULT_BPM,
  offset: 0,
  leadBeats: 4,
  duration: 180,
  songPath: "",
  difficulties: {
    easy:   { beats: [] },
    medium: { beats: [] },
    hard:   { beats: [] },
  },
  activeDifficulty: "easy",

  // Editor-only state (not exported)
  audioBuffer: null,
  zoom: DEFAULT_ZOOM,
  scrollX: 0,
  playheadTime: 0,
  playing: false,
  cursor: { beat: -1, lane: -1 },
  selectedIndices: [],
  tool: { kind: "shape_gate", shape: "circle" },
  analysisData: null,
};

// ── Helpers ─────────────────────────────────────────

const SNAPSHOT_KEYS = [
  "songId", "title", "bpm", "offset",
  "leadBeats", "duration", "songPath",
  "activeDifficulty",
];

function deepCloneDifficulties(diffs) {
  const clone = {};
  for (const key of Object.keys(diffs)) {
    clone[key] = { beats: diffs[key].beats.map((b) => ({ ...b })) };
  }
  return clone;
}

function takeSnapshot() {
  const snap = {};
  for (const k of SNAPSHOT_KEYS) snap[k] = state[k];
  snap.difficulties = deepCloneDifficulties(state.difficulties);
  return snap;
}

function applySnapshot(snap) {
  for (const k of SNAPSHOT_KEYS) state[k] = snap[k];
  state.difficulties = deepCloneDifficulties(snap.difficulties);
}

// ── Mutation Functions ──────────────────────────────

export function getActiveBeats() {
  return state.difficulties[state.activeDifficulty].beats;
}

export function addBeat(entry) {
  const beats = getActiveBeats();
  // Insert in sorted order by beat field
  let i = 0;
  while (i < beats.length && beats[i].beat <= entry.beat) i++;
  beats.splice(i, 0, entry);
  emit("beats-changed");
}

export function removeBeat(index) {
  const beats = getActiveBeats();
  if (index < 0 || index >= beats.length) return;
  beats.splice(index, 1);
  state.selectedIndices = [];
  emit("beats-changed");
  emit("selection-changed");
}

export function updateBeat(index, fields) {
  const beats = getActiveBeats();
  if (index < 0 || index >= beats.length) return;
  if ('beat' in fields && fields.beat !== beats[index].beat) {
    // Beat value changed — remove and reinsert to maintain sorted order.
    const updated = { ...beats[index], ...fields };
    beats.splice(index, 1);
    let newIndex = 0;
    while (newIndex < beats.length && beats[newIndex].beat <= updated.beat) newIndex++;
    beats.splice(newIndex, 0, updated);
    // Keep selected indices pointing at the moved entry.
    const selIdx = state.selectedIndices.indexOf(index);
    if (selIdx !== -1) {
      state.selectedIndices[selIdx] = newIndex;
    }
  } else {
    Object.assign(beats[index], fields);
  }
  emit("beats-changed");
}

export function setActiveDifficulty(name) {
  state.activeDifficulty = name;
  state.selectedIndices = [];
  emit("difficulty-changed");
  emit("selection-changed");
}

export function addDifficulty(name) {
  if (!state.difficulties[name]) {
    state.difficulties[name] = { beats: [] };
  }
}

export function copyDifficulty(from, to) {
  const src = state.difficulties[from];
  if (!src) return;
  state.difficulties[to] = { beats: src.beats.map((b) => ({ ...b })) };
}

const METADATA_FIELDS = [
  "songId", "title", "bpm", "offset",
  "leadBeats", "duration", "songPath",
];

export function setMetadata(fields) {
  for (const k of METADATA_FIELDS) {
    if (k in fields) state[k] = fields[k];
  }
  emit("metadata-changed");
}

// ── Undo / Redo ─────────────────────────────────────
const MAX_UNDO = 50;
const undoStack = [];
const redoStack = [];

export function pushUndo(description) {
  undoStack.push({ description, snapshot: takeSnapshot() });
  if (undoStack.length > MAX_UNDO) undoStack.shift();
  redoStack.length = 0;
}

export function undo() {
  if (!undoStack.length) return;
  const entry = undoStack.pop();
  redoStack.push({ description: entry.description, snapshot: takeSnapshot() });
  applySnapshot(entry.snapshot);
  state.selectedIndices = [];
  emit("beats-changed");
  emit("metadata-changed");
  emit("difficulty-changed");
  emit("selection-changed");
}

export function redo() {
  if (!redoStack.length) return;
  const entry = redoStack.pop();
  undoStack.push({ description: entry.description, snapshot: takeSnapshot() });
  applySnapshot(entry.snapshot);
  state.selectedIndices = [];
  emit("beats-changed");
  emit("metadata-changed");
  emit("difficulty-changed");
  emit("selection-changed");
}

export function canUndo() {
  return undoStack.length > 0;
}

export function canRedo() {
  return redoStack.length > 0;
}
