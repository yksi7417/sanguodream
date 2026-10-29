// The test of a spectator (docs/MULTIPLAYER.md, MP7). Two players start a game. A third member
// of the house opens the game and clicks "Watch". The spectator sees each move of the players,
// cannot take a seat while the game runs, and the chat of a player reaches the spectator.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { signInAs, bothAgree, startTogether } = require("./together");

test("a spectator sees the moves and cannot take a seat while the game runs", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, forces, gid } = await startTogether(browser);
  const watcher = await (await browser.newContext()).newPage();
  await signInAs(watcher, "Watcher");
  await watcher.goto(`/?game=${gid}`);
  await h.waitForTitle(watcher);

  // A member without a seat gets the button "Watch", and no seat button.
  await expect(watcher.locator("#room-watch")).toBeVisible({ timeout: 30_000 });
  await expect(watcher.locator(`#seat-${forces[0]}`)).toHaveCount(0);
  const seat = await watcher.evaluate(async ([id, force]) => {
    const response = await fetch(`api/games/${id}/seat`, { method: "POST", headers: { "Content-Type": "application/json" },
                                                         body: JSON.stringify({ force }) });
    return { status: response.status, body: await response.json() };
  }, [gid, forces[1]]);
  expect(seat.status).toBe(409);
  expect(seat.body.error).toBe("the game started");
  await watcher.click("#room-watch");
  await expect.poll(async () => (await h.gameState(watcher)).shared, { timeout: 120_000 }).toBe(1);
  expect((await h.gameState(watcher)).local_force).toBe(255);

  // The players end their faction turns. The spectator applies each step.
  let state = await bothAgree(host, watcher);
  for (let round = 0; round < 2; round++) {
    const actor = (await h.gameState(host)).local_acts ? host : guest;
    expect(await h.call(actor, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
    await expect.poll(async () => (await h.gameState(watcher)).steps).toBeGreaterThan(state.steps);
    state = await bothAgree(host, watcher);
    await bothAgree(guest, watcher);
  }
  expect(await h.call(watcher, "wasm_test_end_turn", "number", [], [])).toBe(0);   // a spectator gives no command

  // A line of the chat of a player reaches the spectator.
  await host.evaluate(() => sendJson({ type: "chat", text: "Hello <b>all</b>" }));
  await expect.poll(async () => (await h.gameState(watcher)).chat_lines, { timeout: 30_000 }).toBeGreaterThanOrEqual(1);
  expect((await h.gameState(watcher)).net_error).toBe("");
});
