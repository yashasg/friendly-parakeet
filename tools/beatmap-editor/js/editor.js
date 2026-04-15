// editor.js — Mouse and keyboard interaction handler for the timeline canvas.
// Translates user input into state mutations via state.js.

import {
  state, getActiveBeats, addBeat, removeBeat, updateBeat,
  pushUndo, undo, redo, emit, on
} from './state.js';

import { hitTest, beatToX, xToBeat, timeToX } from './timeline.js';

import {
  KINDS_WITH_SHAPE, MIN_ZOOM, MAX_ZOOM,
  HEADER_HEIGHT, LANE_HEIGHT
} from './constants.js';

// ── Drag State ──────────────────────────────────────

let dragState = {
  dragging: false,
  startBeat: 0,
  startLane: 0,
  indices: [],
  originals: [],  // { beat, lane } for each dragged obstacle at drag start
  moved: false,
};

function resetDrag() {
  dragState.dragging = false;
  dragState.startBeat = 0;
  dragState.startLane = 0;
  dragState.indices = [];
  dragState.originals = [];
  dragState.moved = false;
}

// ── Helpers ─────────────────────────────────────────

function canvasCoords(canvas, e) {
  const rect = canvas.getBoundingClientRect();
  return { cx: e.clientX - rect.left, cy: e.clientY - rect.top };
}

function laneFromY(cy) {
  const raw = Math.floor((cy - HEADER_HEIGHT) / LANE_HEIGHT);
  if (raw < 0 || cy < HEADER_HEIGHT) return -1;
  return Math.min(raw, 2);
}

function clampLane(lane) {
  return Math.max(0, Math.min(2, lane));
}

function clampBeat(beat) {
  return Math.max(0, beat);
}

// ── Init ────────────────────────────────────────────

export function init(canvas, contextMenu, audioModule) {
  // ── Mouse: mousedown (left-click, shift-click) ──
  canvas.addEventListener('mousedown', (e) => {
    if (e.button !== 0) return;

    const { cx, cy } = canvasCoords(canvas, e);
    const { beat, lane, obstacleIndex } = hitTest(cx, cy);

    if (obstacleIndex !== null) {
      // Clicked on an existing obstacle
      if (e.shiftKey) {
        // Toggle in multi-select
        const idx = state.selectedIndices.indexOf(obstacleIndex);
        if (idx !== -1) {
          state.selectedIndices.splice(idx, 1);
        } else {
          state.selectedIndices.push(obstacleIndex);
        }
      } else {
        state.selectedIndices = [obstacleIndex];
      }
      emit('selection-changed');

      // Begin drag tracking
      const beats = getActiveBeats();
      const indices = [...state.selectedIndices];
      const originals = indices.map((i) => ({
        beat: beats[i].beat,
        lane: beats[i].lane,
      }));

      pushUndo('Move obstacles');
      dragState = {
        dragging: true,
        startBeat: beat,
        startLane: lane,
        indices,
        originals,
        moved: false,
      };
    } else if (beat >= 0 && lane >= 0) {
      // Clicked on empty cell — place obstacle
      pushUndo('Place obstacle');
      addBeat({ beat, kind: state.tool.kind, shape: state.tool.shape, lane });
      state.selectedIndices = [];
      emit('selection-changed');
    }
  });

  // ── Mouse: contextmenu (right-click) ──
  canvas.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const { cx, cy } = canvasCoords(canvas, e);
    const result = hitTest(cx, cy);
    contextMenu.show(e.clientX, e.clientY, result);
  });

  // ── Mouse: mousemove ──
  canvas.addEventListener('mousemove', (e) => {
    const { cx, cy } = canvasCoords(canvas, e);
    const beat = xToBeat(cx, state);
    const lane = laneFromY(cy);

    state.cursor = { beat, lane };
    emit('cursor-changed');

    // Handle drag
    if (dragState.dragging && (e.buttons & 1)) {
      const deltaBeat = beat - dragState.startBeat;
      const deltaLane = lane - dragState.startLane;

      if (deltaBeat !== 0 || deltaLane !== 0) {
        dragState.moved = true;
        const beats = getActiveBeats();
        for (let i = 0; i < dragState.indices.length; i++) {
          const idx = dragState.indices[i];
          if (idx < 0 || idx >= beats.length) continue;
          const orig = dragState.originals[i];
          updateBeat(idx, {
            beat: clampBeat(orig.beat + deltaBeat),
            lane: clampLane(orig.lane + deltaLane),
          });
        }
      }
    }
  });

  // ── Mouse: mouseup ──
  canvas.addEventListener('mouseup', () => {
    if (dragState.dragging) {
      if (!dragState.moved) {
        // Nothing moved — the undo we pushed is unnecessary, but harmless.
        // A production editor might pop it; we keep it simple.
      }
      resetDrag();
    }
  });

  // ── Mouse: mouseleave ──
  canvas.addEventListener('mouseleave', () => {
    state.cursor = { beat: -1, lane: -1 };
    emit('cursor-changed');
  });

  // ── Mouse: wheel (zoom / pan) ──
  canvas.addEventListener('wheel', (e) => {
    if (e.shiftKey) {
      // Pan horizontally
      state.scrollX = Math.max(0, state.scrollX + e.deltaY);
    } else {
      // Zoom
      e.preventDefault();
      const zoomFactor = e.deltaY < 0 ? 1.15 : 1 / 1.15;
      state.zoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, state.zoom * zoomFactor));
      emit('zoom-changed');
    }
  }, { passive: false });

  // ── Keyboard ──
  window.addEventListener('keydown', (e) => {
    // Don't intercept typing in form fields
    const tag = e.target.tagName;
    if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;

    const ctrl = e.ctrlKey || e.metaKey;

    switch (e.key) {
      // Play / Pause
      case ' ':
        e.preventDefault();
        if (audioModule.isPlaying()) {
          audioModule.pause();
        } else {
          audioModule.play();
        }
        break;

      // Step cursor
      case 'ArrowLeft':
        e.preventDefault();
        state.cursor.beat = clampBeat(state.cursor.beat - (e.shiftKey ? 4 : 1));
        emit('cursor-changed');
        break;

      case 'ArrowRight':
        e.preventDefault();
        state.cursor.beat = state.cursor.beat + (e.shiftKey ? 4 : 1);
        emit('cursor-changed');
        break;

      // Lane selection
      case '1':
        state.cursor.lane = 0;
        emit('cursor-changed');
        break;
      case '2':
        state.cursor.lane = 1;
        emit('cursor-changed');
        break;
      case '3':
        state.cursor.lane = 2;
        emit('cursor-changed');
        break;

      // Shape selection
      case 'q':
        state.tool.shape = 'circle';
        emit('tool-changed');
        break;
      case 'w':
        state.tool.shape = 'square';
        emit('tool-changed');
        break;
      case 'e':
        state.tool.shape = 'triangle';
        emit('tool-changed');
        break;

      // Place at cursor
      case 'Enter':
        if (state.cursor.beat >= 0 && state.cursor.lane >= 0) {
          pushUndo('Place obstacle');
          addBeat({
            beat: state.cursor.beat,
            kind: state.tool.kind,
            shape: state.tool.shape,
            lane: state.cursor.lane,
          });
        }
        break;

      // Delete selected
      case 'Delete':
      case 'Backspace':
        e.preventDefault();
        if (state.selectedIndices.length > 0) {
          pushUndo('Delete obstacles');
          // Remove in reverse index order to avoid index shifting
          const sorted = [...state.selectedIndices].sort((a, b) => b - a);
          for (const idx of sorted) {
            removeBeat(idx);
          }
          state.selectedIndices = [];
          emit('selection-changed');
        }
        break;

      // Undo / Redo
      case 'z':
        if (ctrl) {
          e.preventDefault();
          if (e.shiftKey) {
            redo();
          } else {
            undo();
          }
        }
        break;

      // Export
      case 's':
        if (ctrl) {
          e.preventDefault();
          emit('export-requested');
        }
        break;

      // Zoom in
      case '=':
      case '+':
        state.zoom = Math.min(state.zoom * 1.2, MAX_ZOOM);
        emit('zoom-changed');
        break;

      // Zoom out
      case '-':
        state.zoom = Math.max(state.zoom / 1.2, MIN_ZOOM);
        emit('zoom-changed');
        break;

      default:
        break;
    }
  });
}
