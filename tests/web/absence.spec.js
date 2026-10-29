// The test of an absent player (docs/MULTIPLAYER.md, MP4). The server of the tests allows a
// faction turn of TURN_SECONDS (HEXDOM_TEST_MIN_TURN_SECONDS in playwright.config.js).
// The guest closes the page. After two faction turns of the guest without a player, the AI
// commands the faction of the guest. The guest opens the page again and gets the faction back.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { bothAgree, startTogether, room, isHuman } = require("./together");

const TURN_SECONDS = 8;
const SAFE_SECONDS = 3;   // a command near the end of the timer can meet the end_turn of the server

// The page ends the turn of its faction if it acts and the timer has time left. Gives true
// if it ended the turn.
async function endTurnInTime(page) {
  const state = await h.gameState(page);
  if (!state.local_acts || (state.timer >= 0 && state.timer < SAFE_SECONDS)) return false;
  return (await h.call(page, "wasm_test_end_turn", "number", [], [])) > 0;
}

test("the AI commands the faction of an absent guest, and the guest gets it back", async ({ browser }) => {
  test.setTimeout(480_000);
  const { host, guest, guestContext, forces, gid } = await startTogether(browser, async (page, id) => {
    await page.selectOption("#room-turn", "600");   // the lobby sends the options of the host
    await expect.poll(async () => (await room(page, id)).turn_seconds).toBe(600);
    await page.evaluate((seconds) => setRoomOptions({ mode: "live", turn_seconds: seconds, pause_absent: false }), TURN_SECONDS);
    await expect.poll(async () => (await room(page, id)).turn_seconds).toBe(TURN_SECONDS);
  });
  const guestForce = forces[1];
  await expect.poll(async () => (await h.gameState(host)).timer, { message: "the banner shows the timer" }).toBeGreaterThanOrEqual(0);

  // The guest goes away. The host ends each turn of its faction at once.
  await guest.close();
  await expect.poll(async () => {
    await endTurnInTime(host);
    const seat = (await room(host, gid)).seats.find((item) => item.force === guestForce);
    return seat.ai && !isHuman(await h.gameState(host), guestForce);
  }, { timeout: 180_000, intervals: [500], message: "the AI takes the faction of the absent guest" }).toBe(true);

  // The guest comes back. The host waits: the server ends the turns of the host meanwhile.
  const back = await guestContext.newPage();
  await back.goto(`/?game=${gid}`);
  await h.waitForGame(back);   // The page joins the room immediately. The join closes the splash, and no title shows.
  await expect.poll(async () => (await h.gameState(back)).shared, { timeout: 120_000 }).toBe(1);
  await expect.poll(async () => (await room(host, gid)).seats.find((item) => item.force === guestForce).ai,
                    { message: "the hello of the guest gives the faction back" }).toBe(false);
  let state = await bothAgree(host, back);
  expect(isHuman(state, guestForce)).toBe(true);
  expect(isHuman(await h.gameState(back), guestForce)).toBe(true);

  // The faction of the guest acts again, and the guest commands it.
  await expect.poll(async () => {
    await endTurnInTime(host);
    return endTurnInTime(back);
  }, { timeout: 120_000, intervals: [500], message: "the guest ends a turn of its faction" }).toBe(true);
  state = await bothAgree(host, back);
  expect((await h.gameState(host)).net_error).toBe("");
  expect((await h.gameState(back)).net_error).toBe("");
});
