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

  const page = await browser.newPage();
  const fatal = [];

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
    await page.click('#canvas', { position: { x: 360, y: 640 } });
    await page.waitForTimeout(600);
    let afterStart = await page.screenshot();
    let afterStartHash = sha256(afterStart);
    if (beforeHash === afterStartHash) {
      await page.click('#canvas', { position: { x: 360, y: 640 } });
      await page.waitForTimeout(600);
      afterStart = await page.screenshot();
      afterStartHash = sha256(afterStart);
    }
    if (beforeHash === afterStartHash) {
      fatal.push('no-visual-response-after-title-clicks');
    }

    // Once on level select, clicking a different card should visibly update selection.
    await page.click('#canvas', { position: { x: 360, y: 500 } });
    await page.waitForTimeout(600);
    const afterLevelSelectClick = await page.screenshot();
    const afterLevelSelectClickHash = sha256(afterLevelSelectClick);
    if (afterStartHash === afterLevelSelectClickHash) {
      fatal.push('no-visual-response-after-level-select-click');
    }

    await page.keyboard.press('Enter');
    await page.waitForTimeout(1000);
    const afterInput = await page.screenshot();
    const afterHash = sha256(afterInput);
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
