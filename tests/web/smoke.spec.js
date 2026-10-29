// The smoke tests of the web game: sign in, load a save, move between cities, move
// and attack with a unit, go back to the title, and sign out. Run: build_web.bat test-game, then npx playwright test
// in tests/web (docs/WEB.md, "The browser tests").
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");

test("a user signs in, joins the house and sees the title menu", async ({ page }) => {
  await h.signIn(page);
  const me = await page.evaluate(() => fetch("/api/me").then((r) => r.json()));
  expect(me.name).toBe("Tester");
  expect(await h.windowShown(page, "Title")).toBe(true);
});

test("the splash art shows before the title menu, and a click closes it", async ({ page }) => {
  const lines = [];
  page.on("console", (message) => lines.push(message.text()));
  await h.enterSite(page);
  await h.waitForGame(page);
  expect(await h.item(page, "splash")).not.toBeNull();
  expect(await h.windowShown(page, "Splash")).toBe(true);
  expect(lines).toContain("platform: splash art loaded");
  expect(await h.item(page, "title_load")).toBeNull();
  await h.clickItem(page, "splash");
  await expect.poll(() => h.item(page, "title_load")).not.toBeNull();
  expect(await h.windowShown(page, "Splash")).toBe(false);
});

test("the title menu loads a save from the server", async ({ page }) => {
  await h.signIn(page);
  await h.loadFirstSave(page);
  expect(await h.windowShown(page, "HUD")).toBe(true);
});

test("the city list moves the camera between cities", async ({ page }) => {
  await h.signIn(page);
  await h.loadFirstSave(page);
  // The rows of the cities in the list of the HUD: "hud_row0_<city>".
  const rows = JSON.parse(await h.call(page, "wasm_test_item_ids", "string", ["string"], ["hud_row0_"]));
  const cities = rows.map((id) => Number(id.slice("hud_row0_".length))).slice(0, 2);
  expect(cities.length).toBe(2);

  await h.clickItem(page, `hud_row0_${cities[0]}`);
  await expect.poll(async () => (await h.gameState(page)).city).toBe(cities[0]);
  await h.frames(page, 10);
  const first = await h.gameState(page);
  await h.clickItem(page, `hud_row0_${cities[1]}`);
  await expect.poll(async () => (await h.gameState(page)).city).toBe(cities[1]);
  await h.frames(page, 10);
  const second = await h.gameState(page);
  expect(Math.hypot(second.camera_x - first.camera_x, second.camera_z - first.camera_z)).toBeGreaterThan(1);

  // A right click opens the dialog of the city.
  await h.clickItem(page, `hud_row0_${cities[1]}`, "right");
  await expect.poll(() => h.windowShown(page, "City")).toBe(true);
});

test("a unit moves beside an enemy base and attacks it", async ({ page }) => {
  await h.signIn(page);
  await h.loadFirstSave(page);
  const setup = JSON.parse(await h.call(page, "wasm_test_battle_setup", "string", [], []));
  expect(setup.error).toBeUndefined();
  await h.call(page, "wasm_test_frame_hex", null, ["number", "number"], [setup.start_q, setup.start_r]);
  await h.frames(page, 10);

  // A click selects the unit. A click on the hex beside the base opens the unit menu there.
  await h.clickHex(page, setup.start_q, setup.start_r);
  await expect.poll(async () => (await h.gameState(page)).unit).toBe(setup.unit);
  await h.clickHex(page, setup.beside_q, setup.beside_r);
  await expect.poll(() => h.windowShown(page, "UnitMenu")).toBe(true);
  await h.clickItem(page, "attack");
  await h.clickHex(page, setup.base_q, setup.base_r);

  const battle = async () =>
    JSON.parse(await h.call(page, "wasm_test_battle_state", "string", ["number", "number"], [setup.unit, setup.base]));
  await expect.poll(async () => (await battle()).acted).toBe(1);
  const after = await battle();
  expect([after.unit_q, after.unit_r]).toEqual([setup.beside_q, setup.beside_r]);
  // The base had 1 durability, so the attack takes it.
  expect(after.base_force).toBe(setup.player);
});

test("the system menu goes back to the title", async ({ page }) => {
  await h.signIn(page);
  await h.loadFirstSave(page);
  await h.clickItem(page, "hud_system");
  await h.clickItem(page, "to_title");
  await expect.poll(() => h.item(page, "title_load")).not.toBeNull();
});

test("the user badge shows the name and signs the user out", async ({ page }) => {
  await h.signIn(page);
  await expect(page.locator("#user-name")).toHaveText("Tester");
  await page.click("#user-button");
  // ADMIN_EMAILS of the config makes Tester an admin (playwright.config.js).
  await expect(page.locator("#user-admin")).toBeVisible();
  await page.click("#user-signout");
  await page.waitForURL((url) => url.pathname === "/login");
  expect((await page.request.get("/api/me")).status()).toBe(401);
});

// docs/MAPS.md: the scenario list of the title has the 1200 scenario of the pack medieval_europe.
// The game starts the engine again with that pack and its map, then shows the rulers.
test("a new game of 1200 starts the engine again with the map of Europe", async ({ page }) => {
  await h.signIn(page);
  expect((await h.gameState(page)).map_width).toBe(200);
  await h.clickItem(page, "title_new");
  await expect.poll(() => h.windowShown(page, "Scenarios")).toBe(true);
  await h.clickItem(page, "sc8");
  await expect.poll(() => h.windowShown(page, "Rulers"), { timeout: 30_000 }).toBe(true);
  const state = await h.gameState(page);
  expect(state.map_width).toBe(372);
  expect(state.map_height).toBe(213);
  const rulers = JSON.parse(await h.call(page, "wasm_test_item_ids", "string", ["string"], ["ruler"]));
  expect(rulers.length).toBeGreaterThanOrEqual(10);   // the visible rows of the list of 24 rulers
});

test("the quit button of the title asks, then signs the user out", async ({ page }) => {
  await h.signIn(page);
  page.once("dialog", (dialog) => dialog.accept());
  await h.clickItem(page, "title_quit");
  await page.waitForURL((url) => url.pathname === "/login", { timeout: 30_000 });
});
