// Regression test for issue #426: toolbar buttons that rely on emoji/symbol
// glyphs need an explicit aria-label so screen readers expose a meaningful
// accessible name (title= alone is not announced reliably by NVDA/JAWS/VO).
// Follows the precedent set by issue #389 (settings close button).
//
// Generalized for issue #440: every interactive form control in the editor
// (<select>, <input>, <textarea>) must also carry an accessible name —
// either via an associated <label for=…>, an aria-labelledby pointing at a
// visible label element, or an aria-label. This catches unlabeled selects
// like #default-level-select and #playback-rate.

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

// ── Issue #440: every form control needs an accessible name ─────────────

function extractTags(html, tagName) {
  // Captures the full opening tag for each <tagName ...> occurrence.
  const re = new RegExp(`<${tagName}\\b[^>]*>`, 'gi');
  return [...html.matchAll(re)].map((m) => m[0]);
}

function attr(tag, name) {
  const m = tag.match(new RegExp(`${name}="([^"]*)"`, 'i'));
  return m ? m[1] : null;
}

function hasAccessibleName(tag, html) {
  if (attr(tag, 'aria-label')) return true;

  const labelledby = attr(tag, 'aria-labelledby');
  if (labelledby) {
    // All referenced ids must exist in the document.
    const ids = labelledby.split(/\s+/).filter(Boolean);
    return ids.every((id) => html.includes(`id="${id}"`));
  }

  const id = attr(tag, 'id');
  if (id) {
    const labelRe = new RegExp(`<label\\b[^>]*for="${id}"[^>]*>`, 'i');
    if (labelRe.test(html)) return true;
  }

  // Wrapped <label>…<input/select>…</label> pattern: detect by looking
  // at the surrounding context for a wrapping <label> tag.
  if (id) {
    const idIdx = html.indexOf(`id="${id}"`);
    if (idIdx > 0) {
      const before = html.slice(0, idIdx);
      const lastLabelOpen = before.lastIndexOf('<label');
      const lastLabelClose = before.lastIndexOf('</label>');
      if (lastLabelOpen > lastLabelClose) return true;
    }
  }

  return false;
}

test('every editor form control exposes an accessible name (#440)', () => {
  const html = read('index.html');
  const controls = [
    ...extractTags(html, 'select'),
    // Inputs with no user-visible label (hidden/file/checkbox without
    // surrounding text) are still required to have an accessible name —
    // exclude only type="hidden" which is not focusable.
    ...extractTags(html, 'input').filter(
      (t) => !/type="hidden"/i.test(t) && !/type="file"/i.test(t),
    ),
    ...extractTags(html, 'textarea'),
  ];

  assert.ok(controls.length > 0, 'expected at least one form control to audit');

  for (const tag of controls) {
    const id = attr(tag, 'id') || '(no id)';
    assert.ok(
      hasAccessibleName(tag, html),
      `form control #${id} must have an accessible name via <label for>, aria-label, or aria-labelledby (issue #440): ${tag}`,
    );
  }
});

test('known previously-unlabeled selects now carry an accessible name (#440)', () => {
  const html = read('index.html');
  const lines = html.split('\n');

  // #default-level-select uses aria-labelledby pointing at the visible
  // "Level" label span.
  const levelSelect = lines.find((l) => l.includes('id="default-level-select"'));
  assert.ok(levelSelect, '#default-level-select must exist');
  const labelledby = levelSelect.match(/aria-labelledby="([^"]+)"/);
  assert.ok(labelledby, '#default-level-select must use aria-labelledby (#440)');
  assert.match(
    html,
    new RegExp(`id="${labelledby[1]}"[^>]*>\\s*Level`),
    `aria-labelledby target #${labelledby[1]} must reference a visible "Level" label`,
  );

  // #playback-rate uses an explicit aria-label since it sits in the
  // transport bar with no visible label text.
  const rateSelect = lines.find((l) => l.includes('id="playback-rate"'));
  assert.ok(rateSelect, '#playback-rate must exist');
  const rateAria = rateSelect.match(/aria-label="([^"]+)"/);
  assert.ok(
    rateAria && rateAria[1].length > 0,
    '#playback-rate must carry a non-empty aria-label (#440)',
  );
});
