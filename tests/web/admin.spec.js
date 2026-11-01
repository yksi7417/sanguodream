// The members page: an admin sees the table of the users, and another member gets 403.
// The config makes "Tester" an admin through ADMIN_EMAILS (playwright.config.js).
const { test, expect } = require("@playwright/test");


// Sign in with the test login as NAME. The game does not load.
async function join(page, name) {
  await page.route("**/index.*", (route) => route.abort());
  await page.goto(`/auth/dev/start?name=${encodeURIComponent(name)}`);
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
