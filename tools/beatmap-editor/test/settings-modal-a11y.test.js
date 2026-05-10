// Regression test for issue #389: settings modal close button needs aria-label.

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

test('settings modal close button exposes an aria-label for screen readers', () => {
  const indexHtml = read('index.html');

  // Locate the close button line and assert it carries an aria-label.
  const settingsCloseLine = indexHtml
    .split('\n')
    .find((line) => line.includes('id="btn-settings-close"'));

  assert.ok(settingsCloseLine, 'settings close button must exist in index.html');
  assert.match(
    settingsCloseLine,
    /aria-label="Close settings"/,
    'settings close button must carry aria-label="Close settings" (issue #389)',
  );
});
