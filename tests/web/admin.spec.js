// The members page: an admin sees the table of the users, and another member gets 403.
// The config makes "Tester" an admin through ADMIN_EMAILS (playwright.config.js).
const { test, expect } = require("@playwright/test");

const HOUSE_PASSWORD = "house-test";

// Sign in with the test login as NAME and join the house. The game does not load.
async function join(page, name) {
  await page.route("**/index.*", (route) => route.abort());
  await page.goto(`/auth/dev/start?name=${encodeURIComponent(name)}`);
  if (new URL(page.url()).pathname === "/house") {
    await page.fill("#house-password", HOUSE_PASSWORD);
    await page.click("#house-submit");
  }
  await page.waitForURL((url) => url.pathname === "/");
}

test("an admin sees the members page", async ({ page }) => {
  await join(page, "Tester");
  const response = await page.goto("/admin");
  expect(response.status()).toBe(200);
  const row = page.locator("#members tbody tr", { hasText: "tester@dev.test" });
  await expect(row).toContainText("admin");
});

test("a member who is not an admin gets 403 from the members page", async ({ page }) => {
  await join(page, "Guest");
  const response = await page.goto("/admin");
  expect(response.status()).toBe(403);
  await expect(page.locator("#members")).toHaveCount(0);
});
