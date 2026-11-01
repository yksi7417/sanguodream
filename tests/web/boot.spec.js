// The tests of the first second of a visit (docs/WEB.md, "The first second of a visit"):
// the art of the sign-in page, the loading icon of the page, and the boot page of the
// service worker while the machine of the site starts.
const { test, expect } = require("@playwright/test");

async function workerReady(page) {
  await page.evaluate(() => navigator.serviceWorker.ready.then(() => true));
}

test("the sign-in page shows the splash art", async ({ page }) => {
  await page.goto("/login");
  await expect(page.locator("#login-dev")).toBeVisible();
  const background = await page.evaluate(() => getComputedStyle(document.body).backgroundImage);
  expect(background).toContain("/splash.jpg");
  const picture = await page.request.get("/splash.jpg");
  expect(picture.status()).toBe(200);
  // The panel continues the card of the art: it hides the empty bar of the art.
  const box = await page.locator("main").boundingBox();
  const barTop = (800 - (1422.2 * 9 / 16)) / 2 + 0.65 * (1422.2 * 9 / 16);
  expect(box.y).toBeLessThan(barTop);
  expect(box.y + box.height).toBeLessThanOrEqual(800);
});

test("the page of the game shows the splash and a moving icon before the game files", async ({ page }) => {
  await page.route("**/api/game-files", () => {});   // The answer never comes, so the page stays in the load.
  await page.goto("/login");
  await page.click("#login-dev");
  await page.waitForURL((url) => url.pathname === "/");
  await expect(page.locator("#splash")).toBeVisible();
  const icon = await page.evaluate(() => {
    const style = getComputedStyle(document.getElementById("status"), "::before");
    return { name: style.animationName, state: style.animationPlayState };
  });
  expect(icon.name).toBe("hexdom-spin");
  expect(icon.state).toBe("running");
});

test("the page paints before the check of the device", async ({ page }) => {
  await page.addInitScript(() => {
    const getContext = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (kind, ...rest) {
      if (kind === "webgl2" && window.probeFrames === undefined) window.probeFrames = window.frameCount || 0;
      return getContext.call(this, kind, ...rest);
    };
    const count = () => { window.frameCount = (window.frameCount || 0) + 1; requestAnimationFrame(count); };
    requestAnimationFrame(count);
  });
  await page.route("**/api/game-files", () => {});
  await page.goto("/login");
  await page.click("#login-dev");
  await page.waitForURL((url) => url.pathname === "/");
  await page.waitForFunction(() => window.probeFrames !== undefined);
  expect(await page.evaluate(() => window.probeFrames)).toBeGreaterThan(0);
});

test("the service worker shows the boot page while the site does not answer", async ({ page, context }) => {
  await page.goto("/login");
  await workerReady(page);
  await context.setOffline(true);
  await page.goto("/");
  await expect(page.locator("#boot")).toBeVisible();
  const picture = await page.evaluate(() => fetch("/splash.jpg").then((response) => response.status));
  expect(picture).toBe(200);   // The worker has the picture, so the picture needs no server.
  const icon = await page.evaluate(() => getComputedStyle(document.getElementById("status"), "::before").animationName);
  expect(icon).toBe("hexdom-spin");

  // The site answers again: the boot page opens the real address, and the server decides the page.
  await context.setOffline(false);
  await expect(page.locator("#login-dev")).toBeVisible();
  expect(new URL(page.url()).pathname).toBe("/login");
});

test("the service worker does not touch a sign-in or a form", async ({ page }) => {
  await page.goto("/login");
  await workerReady(page);
  await page.click("#login-dev");
  await page.waitForURL((url) => url.pathname === "/");
  const me = await page.evaluate(() => fetch("/api/me").then((response) => response.json()));
  expect(me.provider).toBe("dev");
  await expect(page.locator("#boot")).toHaveCount(0);
  await page.evaluate(() => document.getElementById("user-signout-form").submit());
  await page.waitForURL((url) => url.pathname === "/login");
  await expect(page.locator("#login-dev")).toBeVisible();
});
