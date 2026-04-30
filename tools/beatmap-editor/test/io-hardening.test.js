import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { importBeatmap, exportBeatmap, validate } from '../js/io.js';
import {
  DEFAULT_LEVELS,
  EDITOR_OBSTACLE_KINDS,
  canAutoLoadBundledContent,
  getContentUrl,
} from '../js/constants.js';

const TEST_DIR = path.dirname(fileURLToPath(import.meta.url));

function makeState(beats) {
  return {
    bpm: 120,
    offset: 0,
    leadBeats: 4,
    duration: 180,
    activeDifficulty: 'hard',
    difficulties: {
      hard: { beats },
    },
  };
}

function expectImportError(jsonString, messagePattern) {
  const { data, errors } = importBeatmap(jsonString);
  assert.ok(Array.isArray(errors), 'errors must be an array');
  assert.ok(errors.length > 0, 'expected import errors');
  if (messagePattern) {
    assert.match(errors.join('\n'), messagePattern);
  }
  if (data !== null) {
    assert.equal(typeof data, 'object', 'invalid imports may return partial normalized data');
  }
}

test('valid beatmap import/export round-trip still works', () => {
  const sourceState = {
    songId: 'song_001',
    title: 'Round Trip',
    bpm: 128,
    offset: 0.25,
    leadBeats: 4,
    duration: 95,
    songPath: 'content/audio/song_001.flac',
    difficulties: {
      easy: {
        beats: [
          { beat: 1, kind: 'shape_gate', shape: 'circle', lane: 1 },
          { beat: 5, kind: 'shape_gate', shape: 'square', lane: 2 },
        ],
      },
      hard: {
        beats: [
          { beat: 3, kind: 'combo_gate', shape: 'circle', lane: 1, blocked: [0, 2] },
          { beat: 6, kind: 'shape_gate', shape: 'triangle', lane: 0 },
        ],
      },
    },
  };

  const exported = exportBeatmap(sourceState);
  const imported = importBeatmap(exported);

  assert.deepEqual(imported.errors, []);
  assert.ok(imported.data);
  assert.equal(imported.data.songId, sourceState.songId);
  assert.equal(imported.data.title, sourceState.title);
  assert.equal(imported.data.bpm, sourceState.bpm);
  assert.equal(imported.data.offset, sourceState.offset);
  assert.equal(imported.data.leadBeats, sourceState.leadBeats);
  assert.equal(imported.data.duration, sourceState.duration);
  assert.deepEqual(imported.data.difficulties.easy.beats.map((b) => b.beat), [1, 5]);
  assert.deepEqual(imported.data.difficulties.hard.beats.map((b) => b.beat), [3, 6]);
});

test('malformed JSON import is rejected with parse error', () => {
  const { data, errors } = importBeatmap('{"song_id":');
  assert.equal(data, null);
  assert.ok(errors.length > 0);
  assert.match(errors[0], /Unexpected|JSON|position|end/i);
});

test('non-object top-level JSON is rejected', () => {
  expectImportError('[]', /top-level|object/i);
});

test('missing beats/difficulties top-level fields is rejected', () => {
  expectImportError(
    JSON.stringify({ song_id: 'x', title: 'No beats', bpm: 120 }),
    /beats|difficulties/i,
  );
});

test('invalid difficulties shape is rejected', () => {
  expectImportError(
    JSON.stringify({ song_id: 'x', difficulties: ['easy'] }),
    /difficulties|object/i,
  );
});

test('unknown kind is rejected with useful error', () => {
  expectImportError(
    JSON.stringify({
      song_id: 'x',
      difficulties: {
        hard: {
          beats: [
            { beat: 4, kind: 'warp_tunnel', shape: 'circle', lane: 1 },
          ],
        },
      },
    }),
    /kind must be one of|warp_tunnel/i,
  );
});

test('lane push kinds are rejected on import', () => {
  expectImportError(
    JSON.stringify({
      song_id: 'x',
      difficulties: {
        hard: {
          beats: [
            { beat: 4, kind: 'lane_push_left', lane: 1 },
          ],
        },
      },
    }),
    /kind must be one of|lane_push_left/i,
  );
});

test('lane_block is rejected on import', () => {
  expectImportError(
    JSON.stringify({
      song_id: 'x',
      difficulties: {
        hard: {
          beats: [
            { beat: 4, kind: 'lane_block', lane: 1, blocked: [0, 2] },
          ],
        },
      },
    }),
    /kind must be one of|lane_block/i,
  );
});

test('unknown shape is rejected with useful error', () => {
  expectImportError(
    JSON.stringify({
      song_id: 'x',
      difficulties: {
        hard: {
          beats: [
            { beat: 4, kind: 'shape_gate', shape: 'hexagon', lane: 1 },
          ],
        },
      },
    }),
    /shape must be one of|hexagon/i,
  );
});

test('lane push kinds are rejected on export', () => {
  assert.throws(
    () => exportBeatmap({
      songId: 'song_001',
      title: 'No Push',
      bpm: 120,
      offset: 0,
      leadBeats: 4,
      duration: 90,
      songPath: 'content/audio/song_001.flac',
      difficulties: {
        hard: {
          beats: [{ beat: 8, kind: 'lane_push_right', lane: 1 }],
        },
      },
    }),
    /kind must be one of|lane_push_right/i,
  );
});

test('lane_block is rejected on export', () => {
  assert.throws(
    () => exportBeatmap({
      songId: 'song_001',
      title: 'No Lane Block',
      bpm: 120,
      offset: 0,
      leadBeats: 4,
      duration: 90,
      songPath: 'content/audio/song_001.flac',
      difficulties: {
        hard: {
          beats: [{ beat: 8, kind: 'lane_block', lane: 1, blocked: [0, 2] }],
        },
      },
    }),
    /kind must be one of|lane_block/i,
  );
});

test('invalid lane values are surfaced by validation', () => {
  const errors = validate(makeState([
    { beat: 1, kind: 'shape_gate', shape: 'circle', lane: -1 },
    { beat: 5, kind: 'split_path', shape: 'triangle', lane: 5 },
  ]));

  const messages = errors.map((e) => e.message).join('\n');
  assert.match(messages, /Lane must be 0-2/i);
});

test('dangerous difficulty keys are rejected', () => {
  expectImportError(
    '{"song_id":"x","difficulties":{"__proto__":{"beats":[{"beat":1,"kind":"shape_gate","shape":"circle","lane":1}]}}}',
    /__proto__|constructor|prototype|difficulty/i,
  );
});

test('unexpected difficulty names are rejected', () => {
  expectImportError(
    JSON.stringify({
      song_id: 'x',
      difficulties: {
        nightmare: { beats: [{ beat: 1, kind: 'shape_gate', shape: 'circle', lane: 1 }] },
      },
    }),
    /difficulty|easy|medium|hard|nightmare/i,
  );
});

test('combo_gate blocked mask: no blocked lanes is surfaced', () => {
  const errors = validate(makeState([
    { beat: 1, kind: 'combo_gate', shape: 'circle', lane: 1, blocked: [] },
  ]));

  assert.match(
    errors.map((e) => e.message).join('\n'),
    /combo.?gate.*block at least one lane|blocked/i,
  );
});

test('combo_gate blocked mask: all lanes blocked is surfaced', () => {
  const errors = validate(makeState([
    { beat: 1, kind: 'combo_gate', shape: 'circle', lane: 1, blocked: [0, 1, 2] },
  ]));

  assert.match(
    errors.map((e) => e.message).join('\n'),
    /combo.?gate.*leave at least one lane open|all lanes/i,
  );
});

test('combo_gate blocked mask: partial mask stays valid', () => {
  const errors = validate(makeState([
    { beat: 1, kind: 'combo_gate', shape: 'circle', lane: 1, blocked: [0, 2] },
  ]));

  const comboErrors = errors.filter((e) => /combo.?gate|blocked lane|lane open/i.test(e.message));
  assert.equal(comboErrors.length, 0);
});

test('combo_gate is not exposed as an authoring kind', () => {
  assert.ok(Array.isArray(EDITOR_OBSTACLE_KINDS));
  assert.ok(!EDITOR_OBSTACLE_KINDS.includes('combo_gate'));
});

test('combo_gate is absent from palette and context-menu authoring surfaces', () => {
  const indexHtml = fs.readFileSync(path.resolve(TEST_DIR, '../index.html'), 'utf8');
  assert.doesNotMatch(indexHtml, /data-kind="combo_gate"/i);

  const contextMenuJs = fs.readFileSync(path.resolve(TEST_DIR, '../js/context-menu.js'), 'utf8');
  assert.match(contextMenuJs, /AUTHORING_KINDS/);
  assert.doesNotMatch(contextMenuJs, /Place ComboGate/i);
});

test('bundled level list points at shipped beatmap and audio assets', () => {
  assert.deepEqual(DEFAULT_LEVELS.map((level) => level.id), [
    '1_stomper',
    '2_drama',
    '3_mental_corruption',
  ]);

  for (const level of DEFAULT_LEVELS) {
    assert.match(level.beatmapPath, /^content\/beatmaps\/.+_beatmap\.json$/);
    assert.match(level.audioPath, /^content\/audio\/.+\.flac$/);
  }

  assert.ok(getContentUrl('content/constants.json').href.endsWith('/content/constants.json'));
  assert.ok(getContentUrl('content/beatmaps/1_stomper_beatmap.json').href.endsWith('/content/beatmaps/1_stomper_beatmap.json'));
  assert.equal(canAutoLoadBundledContent(), false);
});
