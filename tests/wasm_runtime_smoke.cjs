#!/usr/bin/env node

const { chromium } = require('playwright-core');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const BUILD_ASSETS = ['index.js', 'index.wasm', 'index.data'];

function sha256(buf) {
  return crypto.createHash('sha256').update(buf).digest('hex');
}

function parseArgs(argv) {
  const options = {
    buildDir: null,
    seedStaleBuildCache: false,
    url: null,
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--build-dir') {
      i += 1;
      if (i >= argv.length) {
        throw new Error('--build-dir requires a path');
      }
      options.buildDir = argv[i];
    } else if (arg === '--seed-stale-build-cache') {
      options.seedStaleBuildCache = true;
    } else if (!options.url) {
      options.url = arg;
    } else {
      throw new Error(`unexpected argument: ${arg}`);
    }
  }

  if (!options.url) {
    throw new Error('usage: node tests/wasm_runtime_smoke.cjs [--build-dir <dir>] [--seed-stale-build-cache] <url>');
  }
  if (options.seedStaleBuildCache && !options.buildDir) {
    throw new Error('--seed-stale-build-cache requires --build-dir');
  }

  return options;
}

function loadBuildAssetHashes(buildDir) {
  const hashes = new Map();
  for (const name of BUILD_ASSETS) {
    const assetPath = path.join(buildDir, name);
    hashes.set(name, sha256(fs.readFileSync(assetPath)));
  }
  return hashes;
}

function readServiceWorkerCacheName(buildDir) {
  const swText = fs.readFileSync(path.join(buildDir, 'sw.js'), 'utf8');
  const literalName = swText.match(/\bCACHE_NAME\s*=\s*'([^']+)'/);
  if (literalName) {
    return literalName[1];
  }

  const prefix = swText.match(/\bCACHE_PREFIX\s*=\s*'([^']+)'/);
  const suffix = swText.match(/\bCACHE_NAME\s*=\s*CACHE_PREFIX\s*\+\s*'([^']+)'/);
  if (!prefix || !suffix) {
    throw new Error('could not determine service worker cache name from build sw.js');
  }

  return `${prefix[1]}${suffix[1]}`;
}

async function seedStaleBuildCache(context, url, buildDir) {
  const cacheName = readServiceWorkerCacheName(buildDir);
  const seedPage = await context.newPage();
  try {
    await seedPage.goto(url, { waitUntil: 'domcontentloaded', timeout: 60000 });
    await seedPage.waitForFunction(() => 'serviceWorker' in navigator, undefined, { timeout: 10000 });
    await seedPage.waitForFunction(() => navigator.serviceWorker.ready.then(() => true), undefined, { timeout: 30000 });
    await seedPage.evaluate(async ({ assets, cacheName: activeCacheName }) => {
      const cache = await caches.open(activeCacheName);
      await Promise.all(assets.map(name => cache.put(
        new Request(new URL(name, location.href).href),
        new Response(`stale cached ${name}`, {
          status: 200,
          headers: { 'content-type': 'application/octet-stream' },
        }),
      )));
    }, { assets: BUILD_ASSETS, cacheName });
  } finally {
    await seedPage.close();
  }
}

async function assertBuildAssetHashes(page, expectedAssetHashes) {
  if (expectedAssetHashes.size === 0) {
    return [];
  }

  const expected = Object.fromEntries(expectedAssetHashes);
  const results = await page.evaluate(async ({ assetNames }) => {
    const toHex = buffer => Array.from(new Uint8Array(buffer))
      .map(byte => byte.toString(16).padStart(2, '0'))
      .join('');

    return Promise.all(assetNames.map(async name => {
      try {
        const response = await fetch(new URL(name, location.href).href, { cache: 'no-store' });
        if (!response.ok) {
          return { name, status: response.status };
        }
        const digest = await crypto.subtle.digest('SHA-256', await response.arrayBuffer());
        return { name, hash: toHex(digest), status: response.status };
      } catch (err) {
        return { name, error: err instanceof Error ? err.message : String(err) };
      }
    }));
  }, { assetNames: [...expectedAssetHashes.keys()] });

  const failures = [];
  for (const result of results) {
    if (result.error) {
      failures.push(`build-asset-hash-check-failed:${result.name}:${result.error}`);
    } else if (result.status !== 200) {
      failures.push(`build-asset-response-not-ok:${result.name}:${result.status}`);
    } else if (result.hash !== expected[result.name]) {
      failures.push(`stale-build-asset-served:${result.name}:expected=${expected[result.name]}:actual=${result.hash}`);
    }
  }

  return failures;
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const expectedAssetHashes = options.buildDir ? loadBuildAssetHashes(options.buildDir) : new Map();

  const executablePath = process.env.CHROMIUM_EXECUTABLE_PATH;
  if (!executablePath) {
    throw new Error('CHROMIUM_EXECUTABLE_PATH is required');
  }

  const browser = await chromium.launch({
    executablePath,
    headless: true,
    args: ['--no-sandbox', '--disable-dev-shm-usage'],
  });

  const context = await browser.newContext({
    viewport: { width: 1280, height: 720 },
    hasTouch: true,
    isMobile: true,
  });
  const fatal = [];
  let page;
  let cdp;

  async function clickCanvasAt(xRatio, yRatio) {
    const canvas = page.locator('#canvas');
    const box = await canvas.boundingBox();
    if (!box) {
      fatal.push('canvas-bounding-box-unavailable');
      return;
    }
    const x = Math.max(4, Math.floor(box.width * xRatio));
    const y = Math.max(4, Math.floor(box.height * yRatio));
    await canvas.focus();
    await canvas.click({
      position: { x, y },
      delay: 100,
      force: true,
    });
    await page.waitForTimeout(100);
  }

  async function waitForVisualChange(previousHash, timeoutMs) {
    const started = Date.now();
    while (Date.now() - started < timeoutMs) {
      await page.waitForTimeout(300);
      const nextHash = sha256(await page.screenshot());
      if (nextHash !== previousHash) {
        return nextHash;
      }
    }
    return previousHash;
  }

  async function swipeCanvas(xStartRatio, yStartRatio, xEndRatio, yEndRatio) {
    const canvas = page.locator('#canvas');
    const box = await canvas.boundingBox();
    if (!box) {
      fatal.push('canvas-bounding-box-unavailable-for-swipe');
      return;
    }

    const startX = box.x + Math.max(4, Math.floor(box.width * xStartRatio));
    const startY = box.y + Math.max(4, Math.floor(box.height * yStartRatio));
    const endX = box.x + Math.max(4, Math.floor(box.width * xEndRatio));
    const endY = box.y + Math.max(4, Math.floor(box.height * yEndRatio));

    await cdp.send('Input.dispatchTouchEvent', {
      type: 'touchStart',
      touchPoints: [{ x: startX, y: startY, radiusX: 8, radiusY: 8, force: 1, id: 1 }],
    });
    for (let i = 1; i <= 6; i += 1) {
      const t = i / 6;
      await cdp.send('Input.dispatchTouchEvent', {
        type: 'touchMove',
        touchPoints: [{
          x: startX + (endX - startX) * t,
          y: startY + (endY - startY) * t,
          radiusX: 8,
          radiusY: 8,
          force: 1,
          id: 1,
        }],
      });
    }
    await cdp.send('Input.dispatchTouchEvent', {
      type: 'touchEnd',
      touchPoints: [],
    });

    await page.evaluate(({ sx, sy, ex, ey }) => {
      const canvasEl = document.querySelector('#canvas');
      if (!canvasEl) return;
      const steps = 6;
      const PointerCtor = window.PointerEvent || window.MouseEvent;

      const dispatch = (type, x, y, buttons) => {
        canvasEl.dispatchEvent(new PointerCtor(type, {
          bubbles: true,
          cancelable: true,
          composed: true,
          pointerId: 1,
          pointerType: 'touch',
          isPrimary: true,
          buttons,
          button: 0,
          clientX: x,
          clientY: y,
        }));
      };

      const dispatchTouch = (type, x, y) => {
        if (typeof window.TouchEvent !== 'function' || typeof window.Touch !== 'function') {
          return;
        }
        const touch = new window.Touch({
          identifier: 1,
          target: canvasEl,
          clientX: x,
          clientY: y,
          screenX: x,
          screenY: y,
          pageX: x,
          pageY: y,
          radiusX: 8,
          radiusY: 8,
          rotationAngle: 0,
          force: type === 'touchend' ? 0 : 1,
        });
        const activeTouches = type === 'touchend' ? [] : [touch];
        canvasEl.dispatchEvent(new window.TouchEvent(type, {
          bubbles: true,
          cancelable: true,
          composed: true,
          touches: activeTouches,
          targetTouches: activeTouches,
          changedTouches: [touch],
        }));
      };

      dispatch('pointerdown', sx, sy, 1);
      dispatchTouch('touchstart', sx, sy);
      for (let i = 1; i <= steps; i += 1) {
        const t = i / steps;
        const x = sx + (ex - sx) * t;
        const y = sy + (ey - sy) * t;
        dispatch('pointermove', x, y, 1);
        dispatchTouch('touchmove', x, y);
      }
      dispatchTouch('touchend', ex, ey);
      dispatch('pointerup', ex, ey, 0);
    }, { sx: startX, sy: startY, ex: endX, ey: endY });

    await page.waitForTimeout(150);
  }

  async function waitForTitlePhase(expectedPhase, timeoutMs) {
    const started = Date.now();
    const marker = `[${expectedPhase}]`;
    while (Date.now() - started < timeoutMs) {
      const title = await page.title();
      if (title.includes(marker)) {
        return true;
      }
      await page.waitForTimeout(100);
    }
    return false;
  }

  function parseLaneFromTitle(title) {
    const match = title.match(/\[Lane:(\d+)\]/);
    if (!match) {
      return null;
    }
    return Number.parseInt(match[1], 10);
  }

  async function waitForPlayingLane(predicate, timeoutMs) {
    const started = Date.now();
    while (Date.now() - started < timeoutMs) {
      const title = await page.title();
      if (title.includes('[Playing]')) {
        const lane = parseLaneFromTitle(title);
        if (lane !== null && predicate(lane)) {
          return lane;
        }
      }
      await page.waitForTimeout(100);
    }
    return null;
  }

  try {
    if (options.seedStaleBuildCache) {
      await seedStaleBuildCache(context, options.url, options.buildDir);
    }

    page = await context.newPage();
    cdp = await context.newCDPSession(page);

  page.on('console', msg => {
    const text = msg.text();
    if (/RuntimeError: Aborted|Aborted\(|abort\(|emscripten_sleep/i.test(text)) {
      fatal.push(`console:${text}`);
    }
  });

  page.on('pageerror', err => {
    fatal.push(`pageerror:${err.message}`);
  });

  page.on('requestfailed', req => {
    const requestUrl = req.url();
    if (/\.(?:js|wasm|data)(?:\?|$)/i.test(requestUrl)) {
      const reason = req.failure()?.errorText ?? 'unknown';
      fatal.push(`requestfailed:${requestUrl}:${reason}`);
    }
  });

    await page.goto(options.url, { waitUntil: 'domcontentloaded', timeout: 60000 });
    if (options.seedStaleBuildCache) {
      await page.waitForFunction(() => navigator.serviceWorker.controller !== null, undefined, { timeout: 10000 });
    }
    await page.waitForSelector('#canvas', { state: 'visible', timeout: 30000 });
    await page.waitForFunction(() => {
      const loader = document.querySelector('#loader');
      if (!loader) {
        return true;
      }
      const inline = (loader).style?.display ?? '';
      const computed = getComputedStyle(loader).display;
      return inline === 'none' || computed === 'none';
    }, undefined, { timeout: 45000 });
    await page.waitForFunction(() => {
      return typeof document.title === 'string' && document.title.includes('SHAPESHIFTER');
    }, undefined, { timeout: 30000 });
    fatal.push(...await assertBuildAssetHashes(page, expectedAssetHashes));

    const beforeInput = await page.screenshot();
    const beforeHash = sha256(beforeInput);

    // Some browser stacks can consume the first click as focus acquisition.
    // Click the title card body, not the lower button row, so generated
    // Settings/Exit dead zones cannot accidentally swallow Start. Keep
    // retrying until the phase marker changes; title animations or focus
    // changes can otherwise look like a successful screen transition.
    let afterStartHash = beforeHash;
    let reachedLevelSelect = false;
    for (let i = 0; i < 3 && !reachedLevelSelect; i += 1) {
      await clickCanvasAt(0.5, 0.5);
      const nextHash = await waitForVisualChange(afterStartHash, 2500);
      if (nextHash !== afterStartHash) {
        afterStartHash = nextHash;
      }
      reachedLevelSelect = await waitForTitlePhase('LevelSelect', 4000);
    }
    if (beforeHash === afterStartHash) {
      fatal.push('no-visual-response-after-title-clicks');
    }
    if (!reachedLevelSelect) {
      fatal.push(`missing-level-select-phase-marker:${await page.title()}`);
    }
    afterStartHash = sha256(await page.screenshot());

    // Once on level select, keyboard navigation should visibly update selection.
    // The title transition above covers the browser click path; using keyboard
    // here avoids depending on CSS-to-virtual-coordinate details for card rows.
    await page.keyboard.press('ArrowDown');
    const afterLevelSelectNavigationHash = await waitForVisualChange(afterStartHash, 2500);
    if (afterStartHash === afterLevelSelectNavigationHash) {
      fatal.push('no-visual-response-after-level-select-navigation');
    }
    if (!(await waitForTitlePhase('LevelSelect', 1500))) {
      fatal.push(`unexpected-phase-after-level-select-navigation:${await page.title()}`);
    }

    // Regression coverage for #934: selecting a difficulty before confirming
    // must not leave the browser build in a frozen/scattered gameplay frame.
    await page.keyboard.press('ArrowRight');
    const afterDifficultyNavigationHash = await waitForVisualChange(afterLevelSelectNavigationHash, 2500);
    if (afterLevelSelectNavigationHash === afterDifficultyNavigationHash) {
      fatal.push('no-visual-response-after-difficulty-navigation');
    }
    if (!(await waitForTitlePhase('LevelSelect', 1500))) {
      fatal.push(`unexpected-phase-after-difficulty-navigation:${await page.title()}`);
    }

    // Start the selected level. The controller intentionally ignores instant
    // confirm presses for the first 0.2s after entering Level Select, so wait
    // a frame budget before sending confirm.
    await page.waitForTimeout(500);
    let afterHash = afterDifficultyNavigationHash;
    for (let i = 0; i < 3 && afterHash === afterDifficultyNavigationHash; i += 1) {
      await page.keyboard.press('Enter');
      afterHash = await waitForVisualChange(afterDifficultyNavigationHash, 3500);
    }
    if (afterDifficultyNavigationHash === afterHash) {
      fatal.push('no-visual-response-after-enter');
    }
    if (await waitForTitlePhase('Tutorial', 1500)) {
      await page.waitForTimeout(500);
      const beforeTutorialStartHash = sha256(await page.screenshot());
      let afterTutorialStartHash = beforeTutorialStartHash;
      for (let i = 0; i < 3 && afterTutorialStartHash === beforeTutorialStartHash; i += 1) {
        await page.keyboard.press('Enter');
        await clickCanvasAt(0.5, 0.88);
        afterTutorialStartHash = await waitForVisualChange(beforeTutorialStartHash, 3500);
      }
      if (beforeTutorialStartHash === afterTutorialStartHash) {
        fatal.push('no-visual-response-after-tutorial-start');
      }
    }
    if (!(await waitForTitlePhase('Playing', 4500))) {
      fatal.push(`missing-playing-phase-marker:${await page.title()}`);
    }
    const laneBeforeSwipe = await waitForPlayingLane(() => true, 2500);
    if (laneBeforeSwipe === null) {
      fatal.push(`missing-playing-lane-marker:${await page.title()}`);
    }
    const playingHash = sha256(await page.screenshot());
    const afterPlayingAdvanceHash = await waitForVisualChange(playingHash, 3000);
    if (playingHash === afterPlayingAdvanceHash) {
      fatal.push('no-visual-response-after-playing-start');
    }

    const beforeSwipeHash = sha256(await page.screenshot());
    await swipeCanvas(0.35, 0.25, 0.80, 0.25);
    const afterSwipeRightHash = await waitForVisualChange(beforeSwipeHash, 2000);
    if (beforeSwipeHash === afterSwipeRightHash) {
      fatal.push('no-visual-response-after-playing-touch-swipe-right');
    }
    if (!(await waitForTitlePhase('Playing', 1500))) {
      fatal.push(`unexpected-phase-after-playing-touch-swipe-right:${await page.title()}`);
    }
    const laneAfterRightSwipe = await waitForPlayingLane(
      lane => laneBeforeSwipe !== null && lane > laneBeforeSwipe,
      2500,
    );
    if (laneAfterRightSwipe === null) {
      fatal.push(`missing-lane-advance-after-playing-touch-swipe-right:${await page.title()}`);
    }

    await page.waitForTimeout(250);
    await swipeCanvas(0.80, 0.25, 0.35, 0.25);
    const afterSwipeLeftHash = await waitForVisualChange(afterSwipeRightHash, 2000);
    if (afterSwipeRightHash === afterSwipeLeftHash) {
      fatal.push('no-visual-response-after-playing-touch-swipe-left');
    }
    if (!(await waitForTitlePhase('Playing', 1500))) {
      fatal.push(`unexpected-phase-after-playing-touch-swipe-left:${await page.title()}`);
    }
    const laneAfterLeftSwipe = await waitForPlayingLane(
      lane => laneAfterRightSwipe !== null && lane < laneAfterRightSwipe,
      2500,
    );
    if (laneAfterLeftSwipe === null) {
      fatal.push(`missing-lane-return-after-playing-touch-swipe-left:${await page.title()}`);
    }
  } finally {
    await context.close();
    await browser.close();
  }

  if (fatal.length > 0) {
    throw new Error(`WASM runtime smoke failures:\n${fatal.join('\n')}`);
  }

  console.log('WASM runtime smoke passed');
}

main().catch(err => {
  console.error(err.stack || err.message || String(err));
  process.exit(1);
});
