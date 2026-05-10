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

  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  const fatal = [];

  async function clickCanvasAt(xRatio, yRatio) {
    const box = await page.locator('#canvas').boundingBox();
    if (!box) {
      fatal.push('canvas-bounding-box-unavailable');
      return;
    }
    const x = box.x + Math.max(4, Math.floor(box.width * xRatio));
    const y = box.y + Math.max(4, Math.floor(box.height * yRatio));
    await page.mouse.click(x, y);
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

    // Once on level select, clicking a different card should visibly update selection.
    await clickCanvasAt(0.5, 0.42);
    const afterLevelSelectClickHash = await waitForVisualChange(afterStartHash, 2500);
    if (afterStartHash === afterLevelSelectClickHash) {
      fatal.push('no-visual-response-after-level-select-click');
    }

    // Start the selected level through the same canvas click path.
    await clickCanvasAt(0.5, 0.845);
    const afterHash = await waitForVisualChange(afterLevelSelectClickHash, 3500);
    if (afterLevelSelectClickHash === afterHash) {
      fatal.push('no-visual-response-after-enter');
    }
  } finally {
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
