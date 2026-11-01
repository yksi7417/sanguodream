// The helpers of the tests of a shared game (docs/MULTIPLAYER.md): two browser contexts
// with two "dev" users, the start of a game together, and the check that each page has
// the same steps and the same checksum.
const { expect } = require("@playwright/test");
const h = require("./hexdom");

const TITLE_SELECT_MS = 1500;

async function signInAs(page, name) {
  page.on("console", (message) => {
    const text = message.text();
    if (/^platform|desync|error/i.test(text) && !/peak memory/.test(text)) console.log(`  [${name}] ${text}`);
  });
  await page.goto(`/auth/dev/start?name=${encodeURIComponent(name)}`);
  await page.waitForURL((url) => url.pathname === "/");
}

async function bothAgreeOnce(host, guest) {
  await expect.poll(async () => {
    const [a, b] = [await h.gameState(host), await h.gameState(guest)];
    return a.shared && b.shared && a.steps === b.steps && a.checksum === b.checksum ? a.steps : -1;
  }, { timeout: 60_000, message: "the two pages have the same steps and the same checksum" }).toBeGreaterThanOrEqual(0);
  return h.gameState(host);
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

// The host chooses two rulers and clicks "Play online". The guest opens the invite link
// and takes a seat. The hook beforeStart(host, gid) can set the options of the room.
// Then the host starts the game. The reply has the pages, the forces and the game id.
async function startTogether(browser, beforeStart) {
  const hostContext = await browser.newContext();
  const guestContext = await browser.newContext();
  const host = await hostContext.newPage();
  const guest = await guestContext.newPage();

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
  const gid = new URL(host.url()).searchParams.get("game");
  await host.click(`#seat-${forces[0]}`);
  await expect(host.locator("#room")).toContainText("Tester");

  await signInAs(guest, "Guest");
  await guest.goto(link);
  await guest.waitForURL((url) => url.searchParams.has("game"));
  await h.waitForTitle(guest);
  await guest.click(`#seat-${forces[1]}`);
  await expect(guest.locator("#room")).toContainText("Guest");
  await expect(host.locator("#room")).toContainText("Guest", { timeout: 10_000 });
  if (beforeStart) await beforeStart(host, gid);
  await host.click("#room-start");

  const state = await bothAgree(host, guest);
  expect((await h.gameState(host)).local_force).toBe(forces[0]);
  expect((await h.gameState(guest)).local_force).toBe(forces[1]);
  return { host, guest, guestContext, forces, gid, state };
}

// The room as the server gives it to the page.
async function room(page, gid) {
  return page.evaluate(async (id) => (await fetch(`api/games/${id}`)).json(), gid);
}

function isHuman(state, force) {
  return ((BigInt("0x" + state.humans) >> BigInt(force)) & 1n) === 1n;
}

module.exports = { signInAs, bothAgree, startTogether, room, isHuman };
