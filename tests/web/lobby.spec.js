// The test of the lobby of a shared game (docs/MULTIPLAYER.md). The host chooses one ruler.
// A guest sees each faction, changes the seat, and has no start button. A third person
// chooses to watch and joins at the start. The first faction of the order keeps its first
// turn. With no player on that faction, the host plays it for the AI and the game goes on.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { signInAs, bothAgree, room } = require("./together");

// The host chooses one ruler and clicks "Play online". The reply has the rulers of the
// scenario and the first faction of the order, from the state of the start image.
async function hostLobby(browser) {
  const host = await (await browser.newContext()).newPage();
  await signInAs(host, "Tester");
  await h.waitForTitle(host);
  await h.clickItem(host, "title_new");
  await h.clickItem(host, "sc0");
  const rulers = JSON.parse(await h.call(host, "wasm_test_item_ids", "string", ["string"], ["ruler"]));
  const forces = rulers.filter((id) => /^ruler[0-9]+$/.test(id)).map((id) => Number(id.slice("ruler".length)));
  expect(forces.length).toBeGreaterThanOrEqual(3);
  await h.clickItem(host, `ruler${forces[forces.length - 1]}`);
  await h.clickItem(host, "rulers_ok");
  await h.clickItem(host, "start_online");
  await expect(host.locator("#room-link")).toContainText("/r/");
  const link = (await host.locator("#room-link").textContent()).split(" ").pop();
  const gid = new URL(host.url()).searchParams.get("game");
  const first = (await h.gameState(host)).whose_turn;
  expect(forces).toContain(first);
  return { host, link, gid, forces, first, others: forces.filter((force) => force !== first) };
}

async function openLink(browser, name, link) {
  const page = await (await browser.newContext()).newPage();
  await signInAs(page, name);
  await page.goto(link);
  await page.waitForURL((url) => url.searchParams.has("game"));
  await h.waitForTitle(page);
  return page;
}

async function joined(page) {
  await expect.poll(async () => { try { return (await h.gameState(page)).shared; } catch { return -1; } },
                    { timeout: 120_000, message: "the page joins the game" }).toBe(1);
}

test("a guest chooses any faction, a person chooses to watch, and only the host can start", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, link, gid, forces, first, others } = await hostLobby(browser);
  const hostForce = others[0];
  await host.click(`#seat-${hostForce}`);
  await expect(host.locator(`#seat-${hostForce}`)).toHaveText("Leave");

  // The room has a seat for each ruler, and not only for the ruler that the host chose.
  expect((await room(host, gid)).seats.map((seat) => seat.force).sort((a, b) => a - b)).toEqual([...forces].sort((a, b) => a - b));

  // The guest has no start button and no button on the seat of the host.
  const guest = await openLink(browser, "Guest", link);
  await expect(guest.locator("#room-wait")).toBeVisible();
  await expect(guest.locator("#room-start")).toHaveCount(0);
  await expect(guest.locator(`#seat-${hostForce}`)).toHaveCount(0);

  // The guest takes a seat and then changes it to the first faction of the order.
  await guest.click(`#seat-${others[1]}`);
  await expect(guest.locator(`#seat-${others[1]}`)).toHaveText("Leave");
  await guest.click(`#seat-${first}`);
  await expect(guest.locator(`#seat-${first}`)).toHaveText("Leave");
  await expect(guest.locator(`#seat-${others[1]}`)).toHaveText("Take");

  // The third person chooses to watch, and the page joins after the start without a click.
  const watcher = await openLink(browser, "Watcher", link);
  await watcher.click("#room-watch");
  await expect(watcher.locator("#room-watching")).toBeVisible();
  await expect(host.locator("#room-watchers")).toHaveText("Spectators: 1");
  await expect(watcher.locator("#room-start")).toHaveCount(0);

  await host.click("#room-start");
  await joined(watcher);
  await bothAgree(host, guest);
  await bothAgree(host, watcher);
  expect((await h.gameState(host)).local_force).toBe(hostForce);
  expect((await h.gameState(watcher)).local_force).toBe(255);

  // The faction of the guest keeps its first turn: it acts before any step of the game.
  const state = await h.gameState(guest);
  expect(state.local_force).toBe(first);
  expect(state.local_acts).toBe(1);
  expect(state.steps).toBe(0);
  expect(await h.call(watcher, "wasm_test_end_turn", "number", [], [])).toBe(0);   // a spectator gives no command
  expect(state.net_error).toBe("");
});

test("with no player on the first faction, the host plays it for the AI and the game goes on", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, link, first, others } = await hostLobby(browser);
  await host.click(`#seat-${others[0]}`);
  const guest = await openLink(browser, "Guest", link);
  await guest.click(`#seat-${others[1]}`);
  await expect(host.locator("#room")).toContainText("Guest", { timeout: 10_000 });
  await host.click("#room-start");

  // The turn leaves the first faction, and a player then acts. The two pages agree.
  await expect.poll(async () => {
    const [a, b] = [await h.gameState(host), await h.gameState(guest)];
    return a.shared && b.shared && a.whose_turn !== first && (a.local_acts || b.local_acts) && a.steps === b.steps && a.checksum === b.checksum;
  }, { timeout: 120_000, message: "the host plays the first faction for the AI, and a player acts" }).toBeTruthy();
  const state = await bothAgree(host, guest);
  expect(state.steps).toBeGreaterThan(0);
  expect(state.net_error).toBe("");
});
