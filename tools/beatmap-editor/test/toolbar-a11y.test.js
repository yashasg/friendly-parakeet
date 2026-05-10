// Regression test for issue #426: toolbar buttons that rely on emoji/symbol
// glyphs need an explicit aria-label so screen readers expose a meaningful
// accessible name (title= alone is not announced reliably by NVDA/JAWS/VO).
// Follows the precedent set by issue #389 (settings close button).

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

const REQUIRED = [
  { id: 'btn-undo',           label: 'Undo' },
  { id: 'btn-redo',           label: 'Redo' },
  { id: 'btn-settings',       label: 'Song settings' },
  { id: 'btn-zoom-in',        label: 'Zoom in' },
  { id: 'btn-zoom-out',       label: 'Zoom out' },
  { id: 'btn-add-difficulty', label: 'Add difficulty' },
];

test('toolbar glyph-only buttons expose an aria-label for screen readers (#426)', () => {
  const indexHtml = read('index.html');
  const lines = indexHtml.split('\n');

  for (const { id, label } of REQUIRED) {
    const line = lines.find((l) => l.includes(`id="${id}"`));
    assert.ok(line, `button #${id} must exist in index.html`);

    const ariaMatch = line.match(/aria-label="([^"]+)"/);
    assert.ok(
      ariaMatch && ariaMatch[1].length > 0,
      `button #${id} must carry a non-empty aria-label (issue #426)`,
    );
    assert.equal(
      ariaMatch[1],
      label,
      `button #${id} aria-label must be "${label}"`,
    );
  }
});
