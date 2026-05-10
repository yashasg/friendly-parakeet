import test from 'node:test';
import assert from 'node:assert/strict';

import { init } from '../js/editor.js';
import { state, getActiveBeats, on } from '../js/state.js';

function resetState() {
  state.activeDifficulty = 'easy';
  state.difficulties = {
    easy: { beats: [] },
    medium: { beats: [] },
    hard: { beats: [] },
  };
  state.cursor = { beat: 0, lane: 1 };
  state.selectedIndices = [];
  state.tool = { kind: 'shape_gate', shape: 'circle' };
  state.zoom = 1;
  // Cursor upper bound: 60s @ 120bpm + 4 lead beats = 124 beats.
  state.bpm = 120;
  state.duration = 60;
  state.leadBeats = 4;
}

function setupKeyboardHarness() {
  const originalWindow = globalThis.window;
  const windowListeners = {};

  globalThis.window = {
    addEventListener(type, handler) {
      windowListeners[type] = handler;
    },
  };

  const canvas = {
    addEventListener() {},
    getBoundingClientRect() {
      return { left: 0, top: 0 };
    },
  };

  const audio = {
    playing: false,
    isPlaying() {
      return this.playing;
    },
    play() {
      this.playing = true;
    },
    pause() {
      this.playing = false;
    },
  };

  init(canvas, { show() {} }, audio);

  const onKeydown = windowListeners.keydown;
  assert.equal(typeof onKeydown, 'function', 'keydown handler should be registered');

  return {
    audio,
    dispatch(overrides = {}) {
      let prevented = false;
      const event = {
        key: '',
        shiftKey: false,
        ctrlKey: false,
        metaKey: false,
        target: { tagName: 'DIV' },
        preventDefault() {
          prevented = true;
        },
        ...overrides,
      };
      onKeydown(event);
      return { prevented };
    },
    restore() {
      globalThis.window = originalWindow;
    },
  };
}

const harness = setupKeyboardHarness();

test('Space toggles play/pause', () => {
  resetState();

  let result = harness.dispatch({ key: ' ' });
  assert.equal(result.prevented, true);
  assert.equal(harness.audio.playing, true);

  result = harness.dispatch({ key: ' ' });
  assert.equal(result.prevented, true);
  assert.equal(harness.audio.playing, false);
});

test('Arrow keys move cursor with shift step', () => {
  resetState();
  state.cursor.beat = 10;

  harness.dispatch({ key: 'ArrowRight' });
  assert.equal(state.cursor.beat, 11);

  harness.dispatch({ key: 'ArrowLeft', shiftKey: true });
  assert.equal(state.cursor.beat, 7);

  state.cursor.beat = 1;
  harness.dispatch({ key: 'ArrowLeft', shiftKey: true });
  assert.equal(state.cursor.beat, 0);
});

test('1/2/3 and q/w/e update lane and shape', () => {
  resetState();

  harness.dispatch({ key: '1' });
  assert.equal(state.cursor.lane, 0);
  harness.dispatch({ key: '2' });
  assert.equal(state.cursor.lane, 1);
  harness.dispatch({ key: '3' });
  assert.equal(state.cursor.lane, 2);

  harness.dispatch({ key: 'q' });
  assert.equal(state.tool.shape, 'circle');
  harness.dispatch({ key: 'w' });
  assert.equal(state.tool.shape, 'square');
  harness.dispatch({ key: 'e' });
  assert.equal(state.tool.shape, 'triangle');
});

test('Enter places an obstacle at cursor and Delete removes selected', () => {
  resetState();
  state.cursor = { beat: 3, lane: 1 };
  state.tool = { kind: 'shape_gate', shape: 'square' };

  harness.dispatch({ key: 'Enter' });
  let beats = getActiveBeats();
  assert.equal(beats.length, 1);
  assert.deepEqual(beats[0], {
    beat: 3,
    kind: 'shape_gate',
    shape: 'square',
    lane: 1,
  });

  state.selectedIndices = [0];
  const result = harness.dispatch({ key: 'Delete' });
  assert.equal(result.prevented, true);
  beats = getActiveBeats();
  assert.equal(beats.length, 0);
});

test('Ctrl/Cmd+S emits export-requested intent via preventDefault path', () => {
  resetState();

  const ctrlResult = harness.dispatch({ key: 's', ctrlKey: true });
  assert.equal(ctrlResult.prevented, true);

  const cmdResult = harness.dispatch({ key: 's', metaKey: true });
  assert.equal(cmdResult.prevented, true);
});

test('shortcuts are ignored while typing in form fields', () => {
  resetState();
  state.cursor.beat = 5;

  harness.dispatch({ key: 'ArrowRight', target: { tagName: 'INPUT' } });
  assert.equal(state.cursor.beat, 5);
});

test('ArrowRight clamps to song-derived upper bound', () => {
  resetState();
  // Max beat = floor(60 * 120 / 60) + 4 = 124.
  state.cursor.beat = 123;
  harness.dispatch({ key: 'ArrowRight' });
  assert.equal(state.cursor.beat, 124);
  harness.dispatch({ key: 'ArrowRight' });
  assert.equal(state.cursor.beat, 124, 'cursor must not advance beyond max beat');
  harness.dispatch({ key: 'ArrowRight', shiftKey: true });
  assert.equal(state.cursor.beat, 124, 'shift+ArrowRight respects upper bound');
});

test("'?' and F1 emit help-requested with preventDefault", () => {
  resetState();
  let count = 0;
  const handler = () => { count += 1; };
  on('help-requested', handler);

  const r1 = harness.dispatch({ key: '?' });
  assert.equal(r1.prevented, true);
  assert.equal(count, 1);

  const r2 = harness.dispatch({ key: 'F1' });
  assert.equal(r2.prevented, true);
  assert.equal(count, 2);
});

test('Enter calls preventDefault so focused toolbar buttons do not re-fire', () => {
  resetState();
  state.cursor = { beat: 2, lane: 0 };
  const result = harness.dispatch({ key: 'Enter' });
  assert.equal(result.prevented, true);
  const beats = getActiveBeats();
  assert.equal(beats.length, 1);
});

test('cleanup keyboard harness', () => {
  harness.restore();
  assert.ok(true);
});
