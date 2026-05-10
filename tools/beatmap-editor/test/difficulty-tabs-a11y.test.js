// Regression test for issue #439: difficulty tabs in the editor toolbar
// must expose proper tab semantics (role="tab", aria-selected) so screen
// readers can announce them as a tablist with a current selection. Also
// verifies that JS-rebuilt tabs (panels.js rebuildDifficultyTabs /
// syncDifficultyTabsToDOM) keep aria-selected in sync with state.
//
// Static checks read index.html; dynamic checks load panels.js source and
// assert the relevant attribute wiring is present.

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

test('difficulty tabs container is marked as a tablist (#439)', () => {
  const html = read('index.html');
  const containerLine = html
    .split('\n')
    .find((l) => l.includes('id="difficulty-tabs"'));
  assert.ok(containerLine, '#difficulty-tabs container must exist');
  assert.match(
    containerLine,
    /role="tablist"/,
    '#difficulty-tabs must declare role="tablist"',
  );
  assert.match(
    containerLine,
    /aria-label="[^"]+"/,
    '#difficulty-tabs must carry an aria-label naming the tablist',
  );
});

test('each static .diff-tab[data-diff] declares role="tab" and aria-selected (#439)', () => {
  const html = read('index.html');
  const tabRe = /<button[^>]*class="diff-tab(?:\s+active)?"[^>]*data-diff="([^"]+)"[^>]*>/g;
  const matches = [...html.matchAll(tabRe)];
  assert.ok(matches.length >= 3, 'expected at least the three default difficulty tabs');

  let activeCount = 0;
  for (const m of matches) {
    const tag = m[0];
    const diff = m[1];
    assert.match(tag, /role="tab"/, `diff-tab[data-diff="${diff}"] must have role="tab"`);
    const sel = tag.match(/aria-selected="(true|false)"/);
    assert.ok(sel, `diff-tab[data-diff="${diff}"] must have aria-selected`);
    const isActiveClass = /class="diff-tab\s+active"/.test(tag);
    if (sel[1] === 'true') activeCount++;
    assert.equal(
      sel[1] === 'true',
      isActiveClass,
      `diff-tab[data-diff="${diff}"] aria-selected must match .active class`,
    );
  }
  assert.equal(activeCount, 1, 'exactly one default tab must be aria-selected="true"');
});

test('panels.js rebuilds tabs with role="tab" and syncs aria-selected (#439)', () => {
  const js = read('js/panels.js');

  assert.match(
    js,
    /setAttribute\(\s*['"]role['"]\s*,\s*['"]tab['"]\s*\)/,
    'rebuildDifficultyTabs must set role="tab" on created tabs',
  );
  assert.match(
    js,
    /setAttribute\(\s*['"]aria-selected['"]\s*,/,
    'panels.js must set aria-selected on difficulty tabs',
  );
  // The sync function must keep aria-selected in lockstep with the
  // active class so screen-reader state never drifts from visual state.
  assert.match(
    js,
    /classList\.toggle\(\s*['"]active['"][\s\S]{0,200}setAttribute\(\s*['"]aria-selected['"]/,
    'syncDifficultyTabsToDOM must update aria-selected together with the .active class',
  );
});
