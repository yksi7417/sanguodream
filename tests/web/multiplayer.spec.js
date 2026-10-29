// The test of a game together (docs/MULTIPLAYER.md, MP3). Two browser contexts with two
// "dev" users. The host chooses two rulers and clicks "Play online". The guest opens the
// invite link and takes a seat, and the host starts the game. Then each player ends a
// turn, and the checksums of the two pages agree after each step.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { bothAgree, startTogether } = require("./together");

test("two members start one game from an invite link, and each page has the same state", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest } = await startTogether(browser);
  let state = await bothAgree(host, guest);
  expect(await h.windowShown(host, "NetBanner")).toBe(true);
  expect(state.pack).toBe("placeholder");   // MP4: the room records the pack of the host

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
