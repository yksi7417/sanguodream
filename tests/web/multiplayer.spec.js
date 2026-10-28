// The test of a game together (docs/MULTIPLAYER.md, MP3). Two browser contexts with two
// "dev" users. The host chooses two rulers and clicks "Play online". The guest opens the
// invite link and takes a seat, and the host starts the game. Then each player ends a
// turn, and the checksums of the two pages agree after each step.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");

const HOUSE_PASSWORD = "house-test";
const TITLE_SELECT_MS = 1500;

async function signInAs(page, name) {
  page.on("console", (message) => {
    const text = message.text();
    if (/^platform|desync|error/i.test(text) && !/peak memory/.test(text)) console.log(`  [${name}] ${text}`);
  });
  await page.goto(`/auth/dev/start?name=${encodeURIComponent(name)}`);
  if (new URL(page.url()).pathname === "/house") {
    await page.fill("#house-password", HOUSE_PASSWORD);
    await page.click("#house-submit");
  }
  await page.waitForURL((url) => url.pathname === "/");
}

async function bothAgree(host, guest) {
  try {
    await bothAgreeOnce(host, guest);
  } catch (error) {
    console.log("  host state: " + JSON.stringify(await h.gameState(host)));
    console.log("  guest state: " + JSON.stringify(await h.gameState(guest)));
    console.log("  host room: " + await host.locator("#room").textContent().catch(() => ""));
    console.log("  guest room: " + await guest.locator("#room").textContent().catch(() => ""));
    throw error;
  }
  return h.gameState(host);
}

async function bothAgreeOnce(host, guest) {
  await expect.poll(async () => {
    const [a, b] = [await h.gameState(host), await h.gameState(guest)];
    return a.shared && b.shared && a.steps === b.steps && a.checksum === b.checksum ? a.steps : -1;
  }, { timeout: 60_000, message: "the two pages have the same steps and the same checksum" }).toBeGreaterThanOrEqual(0);
  return h.gameState(host);
}

test("two members start one game from an invite link, and each page has the same state", async ({ browser }) => {
  test.setTimeout(420_000);
  const host = await (await browser.newContext()).newPage();
  const guest = await (await browser.newContext()).newPage();

  await signInAs(host, "Tester");
  await h.waitForTitle(host);
  await h.clickItem(host, "title_new");
  await host.waitForTimeout(TITLE_SELECT_MS);
  await h.clickItem(host, "sc0");
  const rulers = JSON.parse(await h.call(host, "wasm_test_item_ids", "string", ["string"], ["ruler"]));
  expect(rulers.length).toBeGreaterThanOrEqual(2);
  const forces = rulers.slice(0, 2).map((id) => Number(id.slice("ruler".length)));
  for (const force of forces) await h.clickItem(host, `ruler${force}`);
  await h.clickItem(host, "rulers_ok");
  await h.clickItem(host, "start_online");

  // The page makes the room and shows the lobby with the invite link.
  await expect(host.locator("#room-link")).toContainText("/r/");
  const link = (await host.locator("#room-link").textContent()).split(" ").pop();
  await host.click(`#seat-${forces[0]}`);
  await expect(host.locator("#room")).toContainText("Tester");

  await signInAs(guest, "Guest");
  await guest.goto(link);
  await guest.waitForURL((url) => url.searchParams.has("game"));
  await h.waitForTitle(guest);
  await guest.click(`#seat-${forces[1]}`);
  await expect(guest.locator("#room")).toContainText("Guest");
  await expect(host.locator("#room")).toContainText("Guest", { timeout: 10_000 });
  await host.click("#room-start");

  let state = await bothAgree(host, guest);
  expect((await h.gameState(host)).local_force).toBe(forces[0]);
  expect((await h.gameState(guest)).local_force).toBe(forces[1]);
  expect(await h.windowShown(host, "NetBanner")).toBe(true);

  // Each player ends the turn of the faction when it acts. The other page applies the steps.
  for (let round = 0; round < 3; round++) {
    const actor = (await h.gameState(host)).local_acts ? host : guest;
    expect(await h.call(actor, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
    const before = state.steps;
    await expect.poll(async () => (await h.gameState(host)).steps).toBeGreaterThan(before);
    state = await bothAgree(host, guest);
  }
  expect((await h.gameState(host)).net_error).toBe("");
  expect((await h.gameState(guest)).net_error).toBe("");
});
