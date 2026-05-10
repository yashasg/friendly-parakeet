#!/usr/bin/env node

const { chromium } = require('playwright-core');
const crypto = require('crypto');

function sha256(buf) {
  return crypto.createHash('sha256').update(buf).digest('hex');
}

async function main() {
  const url = process.argv[2];
  if (!url) {
    throw new Error('usage: node tests/wasm_runtime_smoke.cjs <url>');
  }

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
  const page = await context.newPage();
  const fatal = [];

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

  try {
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 60000 });
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

    const beforeInput = await page.screenshot();
    const beforeHash = sha256(beforeInput);

    // Some browser stacks can consume the first click as focus acquisition.
    // Click the title card body, not the lower button row, so generated
    // Settings/Exit dead zones cannot accidentally swallow Start.
    let afterStartHash = beforeHash;
    for (let i = 0; i < 3 && beforeHash === afterStartHash; i += 1) {
      await clickCanvasAt(0.5, 0.5);
      afterStartHash = await waitForVisualChange(beforeHash, 2500);
    }
    if (beforeHash === afterStartHash) {
      fatal.push('no-visual-response-after-title-clicks');
    }
    if (!(await waitForTitlePhase('LevelSelect', 4000))) {
      fatal.push(`missing-level-select-phase-marker:${await page.title()}`);
    }

    // Once on level select, keyboard navigation should visibly update selection.
    // The title transition above covers the browser click path; using keyboard
    // here avoids depending on CSS-to-virtual-coordinate details for card rows.
    await page.keyboard.press('ArrowDown');
    const afterLevelSelectClickHash = await waitForVisualChange(afterStartHash, 2500);
    if (afterStartHash === afterLevelSelectClickHash) {
      fatal.push('no-visual-response-after-level-select-navigation');
    }
    if (!(await waitForTitlePhase('LevelSelect', 1500))) {
      fatal.push(`unexpected-phase-after-level-select-navigation:${await page.title()}`);
    }

    // Start the selected level. The controller intentionally ignores instant
    // confirm presses for the first 0.2s after entering Level Select, so wait
    // a frame budget before sending confirm.
    await page.waitForTimeout(500);
    let afterHash = afterLevelSelectClickHash;
    for (let i = 0; i < 3 && afterHash === afterLevelSelectClickHash; i += 1) {
      await page.keyboard.press('Enter');
      afterHash = await waitForVisualChange(afterLevelSelectClickHash, 3500);
    }
    if (afterLevelSelectClickHash === afterHash) {
      fatal.push('no-visual-response-after-enter');
    }
    if (!(await waitForTitlePhase('Playing', 4500))) {
      fatal.push(`missing-playing-phase-marker:${await page.title()}`);
    }
    const laneBeforeSwipe = await waitForPlayingLane(() => true, 2500);
    if (laneBeforeSwipe === null) {
      fatal.push(`missing-playing-lane-marker:${await page.title()}`);
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
