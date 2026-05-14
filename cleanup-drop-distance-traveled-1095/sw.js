const CACHE_PREFIX = 'shapeshifter-';
const CACHE_NAME = CACHE_PREFIX + '7ccb481';
const ASSETS_TO_CACHE = ['index.js', 'index.wasm', 'index.data'];

function isBuildAssetRequest(request) {
  if (request.method !== 'GET') return false;

  const url = new URL(request.url);
  if (url.origin !== self.location.origin) return false;

  const filename = url.pathname.split('/').pop();
  return ASSETS_TO_CACHE.includes(filename);
}

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
  if (!isBuildAssetRequest(e.request)) return;

  e.respondWith(
    caches.open(CACHE_NAME).then(cache =>
      fetch(new Request(e.request, { cache: 'no-store' })).then(resp => {
        if (resp.ok) {
          e.waitUntil(cache.put(e.request, resp.clone()));
        }
        return resp;
      }).catch(() =>
        cache.match(e.request).then(cached => {
          if (cached) return cached;
          return new Response('Offline', { status: 503 });
        })
      )
    )
  );
});
