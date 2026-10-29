// The test of a shared game on two days (docs/MULTIPLAYER.md, MP5). A correspondence game
// runs until the server has a snapshot. Both players close the page. On the next day of the
// test clock, both players open "My games" and continue the game from the same step.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { bothAgree, startTogether, room } = require("./together");

const DAY_MS = 24 * 3600 * 1000;

async function latestSnapshot(page, gid) {
  return page.evaluate(async (id) => {
    const response = await fetch(`api/games/${id}/snap/latest`);
    return response.ok ? Number(response.headers.get("X-Snapshot-Step")) : -1;
  }, gid);
}

// A new page of the context on the next day. It opens the game from "My games".
async function nextDay(context, gid) {
  const page = await context.newPage();
  await page.clock.setFixedTime(new Date(Date.now() + DAY_MS));
  await page.goto("/");
  await expect(page.locator(`#game-${gid}`)).toBeVisible({ timeout: 120_000 });
  return page;
}

test("two players stop a game and continue it from the same step on the next day", async ({ browser }) => {
  test.setTimeout(480_000);
  const { host, guest, guestContext, gid } = await startTogether(browser, async (page, id) => {
    await page.selectOption("#room-mode", "correspondence");
    await expect.poll(async () => (await room(page, id)).mode).toBe("correspondence");
  });

  // The players end their faction turns until a game turn passes and the server has a snapshot.
  let state = await bothAgree(host, guest);
  await expect.poll(async () => {
    const actor = (await h.gameState(host)).local_acts ? host : guest;
    if ((await h.gameState(actor)).local_acts) await h.call(actor, "wasm_test_end_turn", "number", [], []);
    return latestSnapshot(host, gid);
  }, { timeout: 120_000, intervals: [1000], message: "the server has a snapshot" }).toBeGreaterThanOrEqual(0);
  state = await bothAgree(host, guest);
  const acting = (await h.gameState(host)).local_acts ? "host" : "guest";

  // Both players stop the game.
  const hostContext = host.context();
  await host.close();
  await guest.close();

  // On the next day, "My games" shows the game and "Your turn" for the player whose faction acts.
  const hostAgain = await nextDay(hostContext, gid);
  const guestAgain = await nextDay(guestContext, gid);
  await expect((acting === "host" ? hostAgain : guestAgain).locator(`#turn-${gid}`)).toBeVisible();
  await expect((acting === "host" ? guestAgain : hostAgain).locator(`#turn-${gid}`)).toHaveCount(0);
  await h.waitForTitle(hostAgain);
  await h.waitForTitle(guestAgain);
  await hostAgain.click(`#game-${gid}`);
  await guestAgain.click(`#game-${gid}`);
  for (const page of [hostAgain, guestAgain]) {
    await page.waitForFunction(() => typeof Module !== "undefined" && typeof Module._wasm_test_state === "function",
                               null, { timeout: 120_000 });
    // Before its first frame, the hook has no engine and throws.
    await expect.poll(async () => { try { return (await h.gameState(page)).shared; } catch (error) { return -1; } },
                      { timeout: 120_000 }).toBe(1);
  }

  // Both pages go on from the snapshot and the log after it, at the step of the day before.
  const again = await bothAgree(hostAgain, guestAgain);
  expect(again.steps).toBe(state.steps);
  expect(again.checksum).toBe(state.checksum);
  expect(await hostAgain.evaluate(() => net.joinStep)).toBeGreaterThan(0);

  // The game goes on.
  const actor = again.local_acts ? hostAgain : guestAgain;
  expect(await h.call(actor, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
  await expect.poll(async () => (await h.gameState(hostAgain)).steps).toBeGreaterThan(again.steps);
  await bothAgree(hostAgain, guestAgain);
  expect((await h.gameState(hostAgain)).net_error).toBe("");
  expect((await h.gameState(guestAgain)).net_error).toBe("");
});
