// The test of a late player (docs/MULTIPLAYER.md, MP9). Two players start a game with free
// seats. A member takes a free seat from the room panel and plays. A spectator replays the
// log from the start image and agrees, and then takes a free seat in the invite panel.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { signInAs, startTogether, room, isHuman } = require("./together");

async function allAgree(pages) {
  try {
    await expect.poll(async () => {
      const states = await Promise.all(pages.map((page) => h.gameState(page)));
      const first = states[0];
      return states.every((state) => state.shared && state.steps === first.steps && state.checksum === first.checksum);
    }, { timeout: 60_000, message: "all pages have the same steps and the same checksum" }).toBe(true);
  } catch (error) {
    for (const page of pages) {
      console.log("  state: " + JSON.stringify(await h.gameState(page)) + " room: " + JSON.stringify(await page.evaluate(() => ({
        lastStep: net.lastStep, joinStep: net.joinStep, outbox: net.outbox.length, socket: net.socket && net.socket.readyState, seat: net.seat }))));
    }
    throw error;
  }
  return h.gameState(pages[0]);
}

async function endTurnOfTheActor(pages) {
  const before = (await h.gameState(pages[0])).steps;
  await expect.poll(async () => {
    for (const page of pages) {
      if ((await h.gameState(page)).local_acts) return h.call(page, "wasm_test_end_turn", "number", [], []);
    }
    return 0;
  }, { timeout: 60_000, message: "a page acts" }).toBeGreaterThan(0);
  await expect.poll(async () => (await h.gameState(pages[0])).steps, { timeout: 60_000 }).toBeGreaterThan(before);
}

async function shared(page) {
  await expect.poll(async () => { try { return (await h.gameState(page)).shared; } catch { return -1; } },
                    { timeout: 120_000, message: "the page joins the game" }).toBe(1);
}

test("a member and a spectator take free seats of a game that runs", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, forces, gid } = await startTogether(browser, null, 4);
  const [, , lateForce, spectatorForce] = forces;
  await allAgree([host, guest]);
  await endTurnOfTheActor([host, guest]);
  await allAgree([host, guest]);

  // A member opens the link. The panel shows the free seats and no seat of a player.
  const late = await (await browser.newContext()).newPage();
  await signInAs(late, "Late");
  await late.goto(`/?game=${gid}`);
  await h.waitForTitle(late);
  await expect(late.locator("#room-link")).toContainText("/r/");
  await expect(late.locator(`#seat-${forces[0]}`)).toHaveCount(0);
  await late.click(`#seat-${lateForce}`);
  await shared(late);
  expect((await h.gameState(late)).local_force).toBe(lateForce);

  // The faction goes to the player with a step of the server, and the other pages know the name.
  await expect.poll(async () => isHuman(await h.gameState(host), lateForce), { timeout: 60_000, message: "the faction goes to the late player" }).toBe(true);
  await allAgree([host, guest, late]);
  const players = await room(host, gid);
  expect(players.seats.find((seat) => seat.force === lateForce).user).toBe("Late");
  expect(players.start_humans).toEqual(forces.slice(0, 2));

  // The late player acts in the turn order of the game.
  await expect.poll(async () => {
    if ((await h.gameState(late)).local_acts) return true;
    await endTurnOfTheActor([host, guest]);
    await allAgree([host, guest, late]);
    return false;
  }, { timeout: 180_000, intervals: [200], message: "the turn comes to the late player" }).toBe(true);
  const state = await allAgree([host, guest, late]);
  expect(await h.call(late, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
  await expect.poll(async () => (await h.gameState(host)).steps).toBeGreaterThan(state.steps);
  await allAgree([host, guest, late]);

  // A spectator replays the log. The faction of the late player is an AI faction in the start image.
  const spectator = await (await browser.newContext()).newPage();
  await signInAs(spectator, "Watcher");
  await spectator.goto(`/?game=${gid}`);
  await h.waitForTitle(spectator);
  await spectator.click("#room-watch");
  await shared(spectator);
  await allAgree([host, guest, late, spectator]);
  expect(isHuman(await h.gameState(spectator), lateForce)).toBe(true);

  // The spectator takes a free seat in the invite panel of the game. The page loads again.
  await h.clickItem(spectator, "net_invite");
  await h.clickItem(spectator, `take_seat${spectatorForce}`).catch((error) => {
    if (!/context was destroyed/.test(error.message)) throw error;   // the page loads again in the click
  });
  await expect.poll(async () => {
    try { return (await h.gameState(spectator)).local_force; } catch { return -1; }
  }, { timeout: 120_000, message: "the spectator gets the seat" }).toBe(spectatorForce);
  await expect.poll(async () => isHuman(await h.gameState(host), spectatorForce), { timeout: 60_000 }).toBe(true);
  await allAgree([host, guest, late, spectator]);

  // A step with an old number: the page loads the state of the room and goes on.
  const lines = [];
  late.on("console", (message) => lines.push(message.text()));
  await late.evaluate(() => sendToRoom(new Uint8Array(8 + 40)));
  await expect.poll(() => lines.some((line) => /the page loads the good state/.test(line)), { timeout: 30_000 }).toBe(true);

  // In the load, the old engine still runs. A step of that engine must not change the steps that the page asks for.
  expect(await late.evaluate(() => {
    const [joined, last] = [net.joined, net.lastStep];
    net.joined = false;
    Module.hexdomNetSend(NET.STEP, last + 50, 0, 0, 0, new Uint8Array(12), "", "");
    const same = net.lastStep === last;
    net.joined = joined;
    return same;
  })).toBe(true);

  // The old engine agrees with the other pages too. Wait for the end of the load first.
  await expect.poll(() => late.evaluate(() => net.joined && !!net.socket && net.socket.readyState === WebSocket.OPEN), { timeout: 60_000 }).toBe(true);
  await allAgree([host, guest, late, spectator]);
  await endTurnOfTheActor([host, guest, late, spectator]);
  await allAgree([host, guest, late, spectator]);
  for (const page of [host, guest, late, spectator]) expect((await h.gameState(page)).net_error).toBe("");
});
