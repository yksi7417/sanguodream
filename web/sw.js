/* The service worker of the site (docs/WEB.md, "The first second of a visit").

   The machine of the site stops when nobody uses it, and it needs seconds to start. A browser
   shows nothing in that time. The worker runs in the browser after the first visit, so it
   can answer without the server:
   - If the server gives the page in WAIT_MS, the browser gets that page. The worker does not change it.
   - Else, the browser gets web/boot.html: the splash and the loading icon. That page waits for
     the server, and then opens the address again.

   The worker keeps only the boot page and the picture. The server still decides each page, the
   sign-in and the redirects. The worker reads only the two addresses of PAGES. Thus it does
   not touch an address of a sign-in, a form or the api. */
var BOOT_CACHE = "hexdom-boot-1";   // Change the number after a change to web/boot.html.
var BOOT_PAGE = "/boot.html";
var PICTURE = "/splash.jpg";
var PAGES = ["/", "/login"];
var WAIT_MS = 500;
var AWAKE_MS = 30000;   // After the boot page, a slow answer of the server is not a stopped machine.
var awakeUntil = 0;

self.addEventListener("install", function (event) {
  event.waitUntil(caches.open(BOOT_CACHE).then(function (cache) {
    return cache.addAll([BOOT_PAGE, PICTURE]);
  }).then(function () { return self.skipWaiting(); }));
});

self.addEventListener("activate", function (event) {
  event.waitUntil((async function () {
    var names = await caches.keys();
    // "hexdom-game" holds the game files of web/shell.html. The worker keeps it.
    await Promise.all(names.filter(function (name) {
      return name.indexOf("hexdom-boot-") === 0 && name !== BOOT_CACHE;
    }).map(function (name) { return caches.delete(name); }));
    if (self.registration.navigationPreload) await self.registration.navigationPreload.enable();
    await self.clients.claim();
  })());
});

self.addEventListener("message", function (event) {
  if (event.data === "hexdom-server-awake") awakeUntil = Date.now() + AWAKE_MS;
});

async function pageOrBoot(event) {
  var page = Promise.resolve(event.preloadResponse).then(function (response) {
    return response || fetch(event.request);
  });
  if (Date.now() < awakeUntil) return page;
  var usable = page.then(function (response) { return response.status < 500 ? response : null; })
                   .catch(function () { return null; });
  event.waitUntil(usable);
  var late = new Promise(function (resolve) { setTimeout(function () { resolve(null); }, WAIT_MS); });
  var first = await Promise.race([usable, late]);
  if (first) return first;
  var cache = await caches.open(BOOT_CACHE);
  return (await cache.match(BOOT_PAGE)) || page;
}

async function pictureFromCache(event) {
  var cache = await caches.open(BOOT_CACHE);
  var kept = await cache.match(PICTURE);
  if (!kept) return fetch(event.request);
  event.waitUntil(cache.add(PICTURE).catch(function () { /* the next visit tries again */ }));
  return kept;
}

self.addEventListener("fetch", function (event) {
  var url = new URL(event.request.url);
  if (event.request.method !== "GET" || url.origin !== location.origin) return;
  if (url.pathname === PICTURE) event.respondWith(pictureFromCache(event));
  else if (event.request.mode === "navigate" && PAGES.indexOf(url.pathname) >= 0) event.respondWith(pageOrBoot(event));
});
