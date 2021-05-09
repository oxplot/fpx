const version = 1;
const cacheName = "offline";
const cacheURLs = [
  "/index.html",
  "/pico.min.css",
  "/fpx.png",
  "/youtube-placeholder.svg",
];

self.addEventListener("install", (event) => {
  event.waitUntil((async () => {
    const cache = await caches.open(cacheName);
    for (const u of cacheURLs) {
      cache.add(new Request(u, { cache: "reload" }));
    }
  })());
});

self.addEventListener("activate", (event) => {
  event.waitUntil((async () => {
    if ("navigationPreload" in self.registration) {
      await self.registration.navigationPreload.enable();
    }
  })());
  self.clients.claim();
});

self.addEventListener("fetch", (event) => {
  if (event.request.cache === 'only-if-cached' && event.request.mode !== 'same-origin') {
    return;
  }
  event.respondWith((async () => {
    const cache = await caches.open(cacheName);
    var response = await cache.match(event.request);
    if (!response) {
      response = await fetch(event.request);
      cache.put(event.request, response.clone());
    }
    return response;
  })());
});
