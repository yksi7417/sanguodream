// The test of a desync in a real browser (docs/MULTIPLAYER.md, MP6). Two players start a game.
// The page that does not act sends one false checksum. The server stops the game, the referee
// decides for the other page, and the page with the false report loads the good state.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { bothAgree, startTogether, room } = require("./together");

const FALSE_CHECKSUM = "0000000000000000";

test("a false checksum stops the game, and the referee gives the good state back", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, gid } = await startTogether(browser);
  const before = await bothAgree(host, guest);
  const actor = before.local_acts ? host : guest;
  const liar = before.local_acts ? guest : host;

  const lines = [];
  liar.on("console", (message) => lines.push(message.text()));
  await liar.evaluate((checksum) => {
    const realSend = sendJson;
    sendJson = (message) => {
      if (message.type === "checksum") {
        message.checksum = checksum;
        sendJson = realSend;
      }
      realSend(message);
    };
  }, FALSE_CHECKSUM);

  // The next step gives the false report. The page of the liar then loads the good state.
  expect(await h.call(actor, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
  await expect.poll(() => lines.some((line) => /the state of this page differs at step \d+/.test(line)),
                    { timeout: 60_000, message: "the room decides against the false report" }).toBe(true);
  await expect.poll(async () => (await room(host, gid)).state, { timeout: 60_000 }).toBe("running");
  const after = await bothAgree(host, guest);
  expect(after.steps).toBeGreaterThan(before.steps);

  // The game goes on after the recovery, and the checksums still agree.
  const next = after.local_acts ? host : guest;
  expect(await h.call(next, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
  await expect.poll(async () => (await h.gameState(host)).steps).toBeGreaterThan(after.steps);
  await bothAgree(host, guest);
  expect((await h.gameState(host)).net_error).toBe("");
  expect((await h.gameState(guest)).net_error).toBe("");
});
