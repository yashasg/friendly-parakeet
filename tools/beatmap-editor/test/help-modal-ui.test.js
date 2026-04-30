import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const TEST_DIR = path.dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = path.resolve(TEST_DIR, '..');

function read(relPath) {
  return fs.readFileSync(path.resolve(ROOT_DIR, relPath), 'utf8');
}

test('editor shell contains help button and modal sections', () => {
  const indexHtml = read('index.html');

  assert.match(indexHtml, /id="btn-help"/);
  assert.match(indexHtml, /id="help-modal"/);
  assert.match(indexHtml, /id="btn-help-close"/);
  assert.match(indexHtml, /How to Use the Beatmap Editor/);
  assert.match(indexHtml, /Loading Audio &amp; Levels/);
  assert.match(indexHtml, /Playback &amp; Seek/);
  assert.match(indexHtml, /Placing, Selecting &amp; Moving Obstacles/);
  assert.match(indexHtml, /Editing Properties, Validation &amp; Export/);
  assert.match(indexHtml, /Keyboard Shortcuts/);
});

test('main wiring binds help modal and supports escape dismiss', () => {
  const mainJs = read('js/main.js');

  assert.match(mainJs, /document\.getElementById\('btn-help'\)/);
  assert.match(mainJs, /document\.getElementById\('help-modal'\)/);
  assert.match(mainJs, /document\.getElementById\('btn-help-close'\)/);
  assert.match(mainJs, /window\.addEventListener\('keydown',\s*\(e\)\s*=>\s*\{/);
  assert.match(mainJs, /if\s*\(e\.key\s*!==\s*'Escape'\)\s*return;/);
  assert.match(mainJs, /helpModal\?\.\s*classList\.contains\('visible'\)/);
});
