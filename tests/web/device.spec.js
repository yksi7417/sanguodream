// The requirements page: a phone with a touch screen and no mouse sees the requirements
// and does not download the game (web/shell.html, deviceProblems).
const { test, expect } = require("@playwright/test");


test.use({ isMobile: true, hasTouch: true, viewport: { width: 390, height: 844 } });

test("a phone sees the requirements page and does not load the game", async ({ page }) => {
  const gameFiles = [];
  page.on("request", (request) => { if (/index\.(js|wasm|data)$/.test(request.url())) gameFiles.push(request.url()); });
  await page.goto("/auth/dev/start?name=Phone");
  await page.waitForURL((url) => url.pathname === "/");
  await expect(page.locator("#unsupported")).toBeVisible();
  await expect(page.locator("#unsupported-reasons")).toContainText("touch screen");
  await expect(page.locator("#unsupported-try")).toBeVisible();
  expect(gameFiles).toEqual([]);

  // The page sends the reason of a stop to the log of the server.
  const report = await page.request.post("/api/client-error", { data: { message: "test: out of memory" } });
  expect(report.status()).toBe(200);
});
