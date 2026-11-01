// The sign-in with an email link in a real browser (web/email_link.py; docs/WEB.md).
// The config sets EMAIL_OUTBOX, so the server writes the link to a file in place of an email.
const fs = require("fs");
const path = require("path");
const { test, expect } = require("@playwright/test");

const OUTBOX = path.resolve(__dirname, "..", "..", "build", "webtest-outbox.txt");

async function askForLink(page, address) {
  fs.rmSync(OUTBOX, { force: true });
  await page.goto("/login");
  await page.fill("#email-address", address);
  await page.click("#email-submit");
  await expect(page.locator("#email-sent")).toBeVisible();
  return fs.readFileSync(OUTBOX, "utf-8");
}

test("the sign-in page has the email form beside the providers", async ({ page }) => {
  await page.goto("/login");
  await expect(page.locator("#login-dev")).toBeVisible();
  await expect(page.locator("#email-address")).toBeVisible();
  await expect(page.locator("#house-password")).toHaveCount(0);
});

test("a click on the link and on the button signs the user in", async ({ page }) => {
  const link = await askForLink(page, "linktester@example.com");
  await page.goto(link);
  await expect(page.locator("body")).toContainText("linktester@example.com");
  expect((await page.request.get("/api/me")).status()).toBe(401);   // the GET of the link does not sign in
  await page.click("#email-confirm");
  await page.waitForURL((url) => url.pathname === "/");
  const me = await (await page.request.get("/api/me")).json();
  expect(me.provider).toBe("email");
  expect(me.name).toBe("linktester");
  const cookie = (await page.context().cookies()).find((item) => item.name === "hx_session");
  expect(cookie.httpOnly).toBe(true);
  expect(cookie.sameSite).toBe("Lax");
});

test("the link works one time", async ({ page, browser }) => {
  const link = await askForLink(page, "oncetester@example.com");
  await page.goto(link);
  await page.click("#email-confirm");
  await page.waitForURL((url) => url.pathname === "/");
  const other = await (await browser.newContext()).newPage();
  const response = await other.goto(link);
  expect(response.status()).toBe(400);
  await expect(other.locator("#email-confirm")).toHaveCount(0);
});
