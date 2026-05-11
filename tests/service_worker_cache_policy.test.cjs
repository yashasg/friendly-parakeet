const assert = require('node:assert/strict');
const fs = require('node:fs');
const test = require('node:test');
const vm = require('node:vm');

function createServiceWorkerHarness(fetchImpl) {
  const listeners = new Map();
  const cachedResponses = new Map();
  const cache = {
    async match(request) {
      return cachedResponses.get(request.url);
    },
    async put(request, response) {
      cachedResponses.set(request.url, response);
    },
  };
  const context = {
    URL,
    Request,
    Response,
    caches: {
      async keys() {
        return [];
      },
      async open() {
        return cache;
      },
      async delete() {
        return true;
      },
    },
    clients: {
      async claim() {},
    },
    fetch: fetchImpl,
    self: {
      location: new URL('https://example.test/game/'),
      skipWaiting() {},
      addEventListener(type, handler) {
        listeners.set(type, handler);
      },
    },
  };

  vm.createContext(context);
  vm.runInContext(fs.readFileSync('app/sw.js', 'utf8'), context);

  return { listeners, cachedResponses };
}

async function dispatchFetch(listeners, request) {
  let responsePromise;
  const waitUntilPromises = [];
  listeners.get('fetch')({
    request,
    respondWith(promise) {
      responsePromise = Promise.resolve(promise);
    },
    waitUntil(promise) {
      waitUntilPromises.push(Promise.resolve(promise));
    },
  });

  assert.ok(responsePromise, 'service worker should handle build asset fetches');
  const response = await responsePromise;
  await Promise.all(waitUntilPromises);
  return response;
}

test('build assets use network response before stale cache entries', async () => {
  const { listeners, cachedResponses } = createServiceWorkerHarness(async request => {
    assert.equal(request.cache, 'no-store');
    return new Response('fresh wasm', { status: 200 });
  });
  const request = new Request('https://example.test/game/index.wasm');
  cachedResponses.set(request.url, new Response('stale wasm', { status: 200 }));

  const response = await dispatchFetch(listeners, request);

  assert.equal(await response.text(), 'fresh wasm');
  assert.equal(await cachedResponses.get(request.url).clone().text(), 'fresh wasm');
});

test('build assets fall back to cache when network is unavailable', async () => {
  const { listeners, cachedResponses } = createServiceWorkerHarness(async () => {
    throw new Error('network unavailable');
  });
  const request = new Request('https://example.test/game/index.data');
  cachedResponses.set(request.url, new Response('cached data', { status: 200 }));

  const response = await dispatchFetch(listeners, request);

  assert.equal(await response.text(), 'cached data');
});

test('missing network and cache returns explicit offline response', async () => {
  const { listeners } = createServiceWorkerHarness(async () => {
    throw new Error('network unavailable');
  });
  const request = new Request('https://example.test/game/index.js');

  const response = await dispatchFetch(listeners, request);

  assert.equal(response.status, 503);
  assert.equal(await response.text(), 'Offline');
});

test('non-build assets are ignored by the service worker fetch handler', () => {
  const { listeners } = createServiceWorkerHarness(async () => new Response('unused'));
  let handled = false;

  listeners.get('fetch')({
    request: new Request('https://example.test/game/cover.png'),
    respondWith() {
      handled = true;
    },
    waitUntil() {},
  });

  assert.equal(handled, false);
});
