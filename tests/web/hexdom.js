// The helpers of the smoke tests. The game draws ImGui on a canvas, so the tests
// ask the test hooks (code/wasm_test_hooks.h) where an item is, and click there.
const { expect } = require("@playwright/test");

const HOUSE_PASSWORD = "house-test";
const TITLE_SELECT_MS = 1500;   // the title buttons wait 1 s before they act (UI_TITLE_SELECT_SECONDS)

async function frames(page, count) {
  await page.evaluate(async (n) => {
    for (let i = 0; i < n; i++) await new Promise((resolve) => requestAnimationFrame(resolve));
  }, count);
}

async function call(page, name, returns, types, args) {
  return page.evaluate(([n, r, t, a]) => Module.ccall(n, r, t, a), [name, returns, types, args]);
}

async function item(page, id) {
  return JSON.parse(await call(page, "wasm_test_item", "string", ["string"], [id]));
}

async function windowShown(page, name) {
  return (await call(page, "wasm_test_window", "number", ["string"], [name])) === 1;
}

async function gameState(page) {
  return JSON.parse(await call(page, "wasm_test_state", "string", [], []));
}

async function hexScreen(page, q, r) {
  return JSON.parse(await call(page, "wasm_test_hex_screen", "string", ["number", "number"], [q, r]));
}

// ImGui reads the mouse position one frame, and the button in the next frames.
async function clickAt(page, x, y, button = "left") {
  await page.mouse.move(x, y);
  await frames(page, 3);
  await page.mouse.down({ button });
  await frames(page, 3);
  await page.mouse.up({ button });
  await frames(page, 3);
}

async function clickItem(page, id, button = "left") {
  let rect = null;
  await expect.poll(async () => (rect = await item(page, id)), { message: `the item ###${id} shows` }).not.toBeNull();
  await clickAt(page, (rect.x0 + rect.x1) / 2, (rect.y0 + rect.y1) / 2, button);
}

async function clickHex(page, q, r) {
  let point = null;
  await expect.poll(async () => (point = await hexScreen(page, q, r)), { message: `the hex ${q},${r} is on the screen` }).not.toBeNull();
  await clickAt(page, point.x, point.y);
}

// Sign in with the test provider, join the house, and wait for the title menu.
// A user who joined before goes straight to the game.
async function signIn(page) {
  page.on("console", (message) => {
    const text = message.text();
    if (/^(platform|hexdom|broadcast):/.test(text)) console.log(`  [page] ${text}`);
  });
  await page.goto("/login");
  await page.click("#login-dev");
  await page.waitForURL((url) => url.pathname === "/house" || url.pathname === "/");
  if (new URL(page.url()).pathname === "/house") {
    await page.fill("#house-password", HOUSE_PASSWORD);
    await page.click("#house-submit");
    await page.waitForURL((url) => url.pathname === "/");
  }
  await page.waitForFunction(() => typeof Module !== "undefined" && typeof Module._wasm_test_item === "function",
                             null, { timeout: 120_000 });
  await expect.poll(() => item(page, "title_load"), { timeout: 120_000 }).not.toBeNull();
}

// Load the save of slot 1 from the title menu, and wait for the map.
async function loadFirstSave(page) {
  await clickItem(page, "title_load");
  await page.waitForTimeout(TITLE_SELECT_MS);
  await clickItem(page, "slot0");
  // The load transition covers the screen for 4 s, then the list of the HUD shows.
  await expect.poll(() => item(page, "hud_bases"), { timeout: 90_000 }).not.toBeNull();
  await expect.poll(() => windowShown(page, "LoadTransition")).toBe(false);
}

module.exports = { frames, call, item, windowShown, gameState, clickAt, clickItem, clickHex, signIn, loadFirstSave };
