importScripts(
  "https://storage.googleapis.com/workbox-cdn/releases/3.6.2/workbox-sw.js"
);
importScripts("/precache-manifest.28a724f0f2006c06cc0f95b05ea9c977.js");
workbox.clientsClaim();
self.__precacheManifest = [].concat(self.__precacheManifest || []);
workbox.precaching.suppressWarnings();
workbox.precaching.precacheAndRoute(self.__precacheManifest, {});
workbox.routing.registerNavigationRoute("/index.html", {
  blacklist: [/^\/_/, /\/[^/]+\.[^/]+$/]
});
