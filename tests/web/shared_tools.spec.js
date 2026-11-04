// The tools of a shared game in the real pages: the save, the copy of the invite link, and the
// button that gives a faction to the AI and takes it back (docs/MULTIPLAYER.md).
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { bothAgree, startTogether, room, isHuman } = require("./together");

const NOTICE_SAVED = 2, NOTICE_LOAD_OFF = 4, NOTICE_COPIED = 5, NOTICE_HOST_SAVES = 7;

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

// The test server has no bucket. These routes act as a bucket: the upload address goes to a
// fake host, and that host keeps the file on the server of the test through its own route.
async function fakeBucket(page, refuseUpload) {
  const seen = { uploads: 0, finishes: 0, serverPuts: 0 };
  const cors = { "Access-Control-Allow-Origin": "*", "Access-Control-Allow-Methods": "PUT", "Access-Control-Allow-Headers": "*" };
  let pending = null;
  await page.route(/\/snap\/\d+\/upload\?/, async (route) => {
    const step = /\/snap\/(\d+)\/upload\?(.*)$/.exec(route.request().url());
    pending = `${new URL(page.url()).origin}/api/games/${new URL(page.url()).searchParams.get("game")}/snap/${step[1]}?${step[2]}`;
    await route.fulfill({ json: { url: `https://bucket.test/${step[1].padStart(8, "0")}.hxs?signature=1` } });
  });
  await page.route("https://bucket.test/**", async (route) => {
    if (route.request().method() === "OPTIONS") return route.fulfill({ status: 204, headers: cors });
    seen.uploads++;
    if (refuseUpload) return route.abort();
    const stored = await page.request.put(pending, { data: route.request().postDataBuffer() });
    return route.fulfill({ status: stored.ok() ? 200 : 500, headers: cors });
  });
  await page.route(/\/snap\/\d+\/finish\?/, async (route) => {
    seen.finishes++;
    await route.fulfill({ json: { snapshot: 0 } });
  });
  page.on("request", (request) => {
    if (request.method() === "PUT" && /\/api\/games\/.*\/snap\/\d+\?/.test(request.url())) seen.serverPuts++;
  });
  return seen;
}

test("the host saves directly to the bucket, a guest cannot save, and a refused upload goes through the server", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, gid } = await startTogether(browser);
  const before = await bothAgree(host, guest);
  expect(await h.call(before.local_acts ? host : guest, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
  await expect.poll(async () => (await h.gameState(host)).steps).toBeGreaterThan(before.steps);
  await bothAgree(host, guest);

  // A guest: no snapshot, and the banner says why.
  const lines = [];
  guest.on("request", (request) => { if (/\/snap\/\d+/.test(request.url())) lines.push(request.url()); });
  await h.clickItem(guest, "hud_system");
  await h.clickItem(guest, "open_save");
  await notice(guest, NOTICE_HOST_SAVES);
  expect(lines).toEqual([]);
  const refused = await guest.evaluate(async (id) => (await fetch(`api/games/${id}/snap/0/upload?checksum=${"0".repeat(16)}`, { method: "POST" })).status, gid);
  expect(refused).toBeGreaterThanOrEqual(400);

  // The host: the file goes to the bucket, and the page tells the server.
  const seen = await fakeBucket(host, false);
  await h.clickItem(host, "hud_system");
  await h.clickItem(host, "open_save");
  await notice(host, NOTICE_SAVED);
  expect(seen.uploads).toBe(1);
  expect(seen.finishes).toBe(1);
  expect(seen.serverPuts).toBe(0);

  // The bucket refuses: the save goes through the server.
  await host.unrouteAll();
  const refusing = await fakeBucket(host, true);
  await h.clickItem(host, "hud_system");
  await h.clickItem(host, "open_save");
  await expect.poll(() => refusing.serverPuts, { message: "the save goes through the server" }).toBe(1);
  await notice(host, NOTICE_SAVED);
  expect(refusing.finishes).toBe(0);
  const latest = await latestSnapshot(host, gid);
  expect(latest).toBeGreaterThanOrEqual(0);

  // A page reads the save from the bucket after a redirect. The name of the file gives the step.
  const data = await (await host.request.get(`/api/games/${gid}/snap/latest`)).body();
  await guest.route(/\/snap\/latest$/, (route) => route.fulfill({ status: 302, headers: { location: `https://bucket.test/${String(latest).padStart(8, "0")}.hxs?signature=1` } }));
  await guest.route("https://bucket.test/**", (route) => route.fulfill({ body: data, headers: { "Access-Control-Allow-Origin": "*" } }));
  await guest.reload();
  await expect.poll(async () => { try { return (await h.gameState(guest)).shared; } catch { return -1; } }, { timeout: 120_000 }).toBe(1);
  expect(await guest.evaluate(() => net.joinStep)).toBe(latest + 1);
  await bothAgree(host, guest);

  // The bucket cannot be reached: the page reads the save from the server.
  await guest.unrouteAll();
  await guest.route("https://bucket.test/**", (route) => route.abort());
  await guest.route(/\/snap\/latest$/, (route) => route.fulfill({ status: 302, headers: { location: "https://bucket.test/00000000.hxs" } }));
  await guest.reload();
  await expect.poll(async () => { try { return (await h.gameState(guest)).shared; } catch { return -1; } }, { timeout: 120_000 }).toBe(1);
  expect(await guest.evaluate(() => net.joinStep)).toBe(latest + 1);
  await bothAgree(host, guest);
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

test("the follow option shows captions, a guest votes, and the host ends the game for each page", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, gid } = await startTogether(browser);
  await bothAgree(host, guest);

  // The AI plays both factions, so the actions come without a click.
  await h.clickItem(guest, "net_auto");
  await h.clickItem(host, "net_auto");
  await h.clickItem(guest, "net_follow");
  await expect.poll(async () => (await h.gameState(guest)).follow_shown, { timeout: 120_000, message: "a caption shows" }).toBe(1);
  expect((await h.gameState(guest)).follow_order).not.toBe("");

  // The vote of the guest does not end the game. The second click takes the vote back.
  await h.clickItem(guest, "net_end_game");
  await h.clickItem(guest, "net_end_yes");
  await expect.poll(async () => (await h.gameState(host)).end_votes).toBe(1);
  expect((await room(host, gid)).state).toBe("running");
  await h.clickItem(guest, "net_end_game");
  await expect.poll(async () => (await h.gameState(host)).end_votes).toBe(0);

  // The host ends the game. Each page shows the panel, and no page has an error.
  await h.clickItem(host, "net_end_game");
  await h.clickItem(host, "net_end_yes");
  for (const page of [host, guest]) {
    await expect.poll(async () => (await h.gameState(page)).net_ended, { timeout: 60_000 }).toBe(1);
    expect((await h.gameState(page)).shared).toBe(0);
    expect((await h.gameState(page)).net_error).toBe("");
  }
  expect((await room(host, gid)).state).toBe("ended");

  // The page opens the ended game again, and the panel shows after the log.
  await host.reload();
  await expect.poll(async () => (await h.gameState(host).catch(() => ({}))).net_ended, { timeout: 180_000 }).toBe(1);
  // The click starts a navigation, so the wait of the helper for a frame can fail. The address proves the result.
  await h.clickItem(host, "net_leave").catch((error) => expect(String(error)).toContain("Execution context was destroyed"));
  await expect.poll(() => new URL(host.url()).pathname, { timeout: 60_000 }).toBe("/");
});
