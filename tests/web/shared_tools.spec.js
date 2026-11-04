// The tools of a shared game in the real pages: the save, the copy of the invite link, and the
// button that gives a faction to the AI and takes it back (docs/MULTIPLAYER.md).
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { bothAgree, startTogether, room, isHuman } = require("./together");

const NOTICE_SAVED = 2, NOTICE_LOAD_OFF = 4, NOTICE_COPIED = 5;

async function latestSnapshot(page, gid) {
  return page.evaluate(async (id) => {
    const response = await fetch(`api/games/${id}/snap/latest`);
    return response.ok ? Number(response.headers.get("X-Snapshot-Step")) : -1;
  }, gid);
}

async function notice(page, value) {
  await expect.poll(async () => (await h.gameState(page)).net_notice, { message: `the notice ${value} shows` }).toBe(value);
}

test("the save of a menu goes to the server, and the copy button copies the link", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, gid } = await startTogether(browser);
  await host.context().grantPermissions(["clipboard-read", "clipboard-write"]);

  const before = await bothAgree(host, guest);
  const actor = before.local_acts ? host : guest;
  expect(await h.call(actor, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
  await expect.poll(async () => (await h.gameState(host)).steps).toBeGreaterThan(before.steps);
  const state = await bothAgree(host, guest);

  // Save: no local dialog, and the server keeps the state of this step.
  await h.clickItem(host, "hud_system");
  await h.clickItem(host, "open_save");
  await notice(host, NOTICE_SAVED);
  expect(await h.item(host, "slot0")).toBeNull();
  expect(await latestSnapshot(host, gid)).toBe(state.steps - 1);

  // Load: no local dialog.
  await h.clickItem(host, "hud_system");
  await h.clickItem(host, "open_load");
  await notice(host, NOTICE_LOAD_OFF);
  expect(await h.item(host, "slot0")).toBeNull();

  // The copy button of the invite panel.
  await h.clickItem(host, "net_invite");
  await h.clickItem(host, "copy_link");
  await notice(host, NOTICE_COPIED);
  const link = await host.evaluate(() => navigator.clipboard.readText());
  expect(link).toMatch(/\/r\/[A-Z]{6}$/);

  // A new page of the guest continues from the saved state.
  await guest.reload();
  await expect.poll(async () => { try { return (await h.gameState(guest)).shared; } catch { return -1; } }, { timeout: 120_000 }).toBe(1);
  expect(await guest.evaluate(() => net.joinStep)).toBeGreaterThan(0);
  await bothAgree(host, guest);

  // A browser that refuses the clipboard: the page shows the link in a field.
  await host.evaluate(() => {
    navigator.clipboard.writeText = () => Promise.reject(new Error("refused"));
    document.execCommand = () => false;
  });
  await h.clickItem(host, "copy_link");
  await expect(host.locator("#copy-field")).toHaveValue(link);
  await host.click("#copy-close");
  expect((await h.gameState(host)).net_error).toBe("");
});

test("the copy button of the lobby copies the link", async ({ browser }) => {
  test.setTimeout(240_000);
  await startTogether(browser, async (host) => {
    await host.context().grantPermissions(["clipboard-read", "clipboard-write"]);
    await host.click("#room-copy");
    await expect(host.locator("#room-copy")).toHaveText("Copied");
    const link = (await host.locator("#room-link").textContent()).split(" ").pop();
    expect(await host.evaluate(() => navigator.clipboard.readText())).toBe(link);
  });
});

test("each player gives the faction to the AI, the game goes on, and the button takes it back", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, forces, gid } = await startTogether(browser);
  await bothAgree(host, guest);

  // The guest first, then the host: the two players watch the game of the AI.
  await h.clickItem(guest, "net_auto");
  await expect.poll(async () => isHuman(await h.gameState(host), forces[1]), { timeout: 60_000 }).toBe(false);
  await h.clickItem(host, "net_auto");
  await expect.poll(async () => isHuman(await h.gameState(guest), forces[0]), { timeout: 60_000 }).toBe(false);
  const seats = (await room(host, gid)).seats;
  expect(seats.filter((seat) => seat.auto && seat.ai).length).toBe(2);

  // The game goes on without a click. The option of the banner shows for each page.
  await h.clickItem(guest, "net_follow");
  await bothAgree(host, guest);
  const start = await h.gameState(host);
  await expect.poll(async () => (await h.gameState(host)).turn, { timeout: 60_000, message: "the game turns pass" }).toBeGreaterThan(start.turn + 1);
  expect((await h.gameState(guest)).local_acts).toBe(0);

  // The guest takes the faction back and gets a faction turn.
  await h.clickItem(guest, "net_auto");
  await expect.poll(async () => (await h.gameState(guest)).local_acts, { timeout: 60_000, message: "the faction turn of the guest comes" }).toBe(1);
  const state = await bothAgree(host, guest);
  expect(isHuman(state, forces[1])).toBe(true);
  expect(isHuman(state, forces[0])).toBe(false);
  expect(await h.call(guest, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
  await expect.poll(async () => (await h.gameState(host)).steps).toBeGreaterThan(state.steps);
  await bothAgree(host, guest);
  for (const page of [host, guest]) expect((await h.gameState(page)).net_error).toBe("");
});
