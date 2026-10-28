// The tests of the game files (docs/WEB.md, "The game files in the bucket"). The test
// server has no bucket, so /api/game-files gives the files of the server. The page then
// keeps them in the Cache Storage, and a second load downloads nothing.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");

const GAME_FILE = /\/index\.(wasm|data)$/;

test("the game files api refuses a person without a member session", async ({ request }) => {
  const response = await request.get("/api/game-files");
  expect(response.status()).toBe(401);
});

test("a member gets the addresses of the build", async ({ page }) => {
  await h.signIn(page);
  const info = await page.evaluate(() => fetch("/api/game-files").then((r) => r.json()));
  expect(info.build).toMatch(/^[0-9a-f]{16}$/);
  expect(info.source).toBe("server");
  for (const name of ["index.wasm", "index.data"]) {
    expect(info.files[name].url).toBe(name);
    expect(info.files[name].fallback).toBe(name);
    expect(info.files[name].size).toBeGreaterThan(0);
  }
});

test("a second load takes the game files from the Cache Storage", async ({ page }) => {
  await h.signIn(page);
  const kept = await page.evaluate(async () => (await (await caches.open("hexdom-game")).keys()).map((r) => r.url));
  expect(kept.length).toBe(2);

  const downloads = [];
  page.on("request", (request) => {
    if (GAME_FILE.test(new URL(request.url()).pathname)) downloads.push(request.url());
  });
  await page.reload();
  await h.waitForTitle(page);
  expect(downloads).toEqual([]);
  expect(await h.windowShown(page, "Title")).toBe(true);
});
