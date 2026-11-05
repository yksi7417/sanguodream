// The test of the lobby of a shared game (docs/MULTIPLAYER.md). The host chooses one ruler.
// A guest sees each faction, changes the seat, and has no start button. A third person
// chooses to watch and joins at the start. The faction of the guest keeps its first turn.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { signInAs, bothAgree, room } = require("./together");

test("a guest chooses any faction, a person chooses to watch, and only the host can start", async ({ browser }) => {
  test.setTimeout(420_000);
  const host = await (await browser.newContext()).newPage();
  await signInAs(host, "Tester");
  await h.waitForTitle(host);
  await h.clickItem(host, "title_new");
  await h.clickItem(host, "sc0");
  const rulers = JSON.parse(await h.call(host, "wasm_test_item_ids", "string", ["string"], ["ruler"]));
  const forces = rulers.map((id) => Number(id.slice("ruler".length)));
  expect(forces.length).toBeGreaterThanOrEqual(3);
  const hostForce = forces[forces.length - 1];
  await h.clickItem(host, `ruler${hostForce}`);
  await h.clickItem(host, "rulers_ok");
  await h.clickItem(host, "start_online");
  await expect(host.locator("#room-link")).toContainText("/r/");
  const link = (await host.locator("#room-link").textContent()).split(" ").pop();
  const gid = new URL(host.url()).searchParams.get("game");
  await host.click(`#seat-${hostForce}`);
  await expect(host.locator(`#seat-${hostForce}`)).toHaveText("Leave");

  // The room has a seat for each ruler, and not only for the ruler of the host.
  expect((await room(host, gid)).seats.map((seat) => seat.force).sort((a, b) => a - b)).toEqual([...forces].sort((a, b) => a - b));

  // The guest has no start button and no button on the seat of the host.
  const guest = await (await browser.newContext()).newPage();
  await signInAs(guest, "Guest");
  await guest.goto(link);
  await guest.waitForURL((url) => url.searchParams.has("game"));
  await h.waitForTitle(guest);
  await expect(guest.locator("#room-wait")).toBeVisible();
  await expect(guest.locator("#room-start")).toHaveCount(0);
  await expect(guest.locator(`#seat-${hostForce}`)).toHaveCount(0);

  // The guest takes a seat and then changes it. The first ruler in the order of the game stays.
  await guest.click(`#seat-${forces[1]}`);
  await expect(guest.locator(`#seat-${forces[1]}`)).toHaveText("Leave");
  await guest.click(`#seat-${forces[0]}`);
  await expect(guest.locator(`#seat-${forces[0]}`)).toHaveText("Leave");
  await expect(guest.locator(`#seat-${forces[1]}`)).toHaveText("Take");

  // The third person chooses to watch, and the page joins after the start without a click.
  const watcher = await (await browser.newContext()).newPage();
  await signInAs(watcher, "Watcher");
  await watcher.goto(link);
  await watcher.waitForURL((url) => url.searchParams.has("game"));
  await h.waitForTitle(watcher);
  await watcher.click("#room-watch");
  await expect(watcher.locator("#room-watching")).toBeVisible();
  await expect(host.locator("#room-watchers")).toHaveText("Spectators: 1");
  await expect(watcher.locator("#room-start")).toHaveCount(0);

  await host.click("#room-start");
  const shared = async (page) => expect.poll(async () => { try { return (await h.gameState(page)).shared; } catch { return -1; } },
                                             { timeout: 120_000, message: "the page joins the game" }).toBe(1);
  await shared(watcher);
  await bothAgree(host, guest);
  await bothAgree(host, watcher);
  expect((await h.gameState(host)).local_force).toBe(hostForce);
  expect((await h.gameState(guest)).local_force).toBe(forces[0]);
  expect((await h.gameState(watcher)).local_force).toBe(255);

  // The guest has the first ruler, and the game reaches that faction with its turn.
  await expect.poll(async () => (await h.gameState(guest)).local_acts, { timeout: 60_000, message: "the first faction of the guest acts" }).toBeTruthy();
  expect(await h.call(watcher, "wasm_test_end_turn", "number", [], [])).toBe(0);   // a spectator gives no command
  expect((await h.gameState(guest)).net_error).toBe("");
});
