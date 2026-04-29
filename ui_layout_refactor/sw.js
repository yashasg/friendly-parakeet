const CACHE_PREFIX = 'shapeshifter-';
const CACHE_NAME = CACHE_PREFIX + '1c1b8d9';
const ASSETS_TO_CACHE = ['index.js', 'index.wasm', 'index.data'];

self.addEventListener('install', e => {
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys()
      .then(keys => Promise.all(
        keys
          .filter(name => name.startsWith(CACHE_PREFIX) && name !== CACHE_NAME)
          .map(name => caches.delete(name))
      ))
      .then(() => clients.claim())
  );
});

self.addEventListener('fetch', e => {
  const url = new URL(e.request.url);
  const filename = url.pathname.split('/').pop();
  if (!ASSETS_TO_CACHE.includes(filename)) return;

  e.respondWith(
    caches.open(CACHE_NAME).then(cache =>
      cache.match(e.request).then(cached => {
        if (cached) return cached;
        return fetch(e.request).then(resp => {
          if (resp.ok) cache.put(e.request, resp.clone());
          return resp;
        });
      })
    ).catch(() => new Response('Offline', { status: 503 }))
  );
});
