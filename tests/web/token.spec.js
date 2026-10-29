// The test of a page without the cookie (docs/MULTIPLAYER.md, MP8). The page of a Discord
// Activity has no cookie of the site: it sends a session token in the header of each request
// and in the hello of the room. A test cannot run inside Discord, so this test gives the page
// the token of the guest in the address. The page joins the room and gets the steps.
const { test, expect } = require("@playwright/test");
const h = require("./hexdom");
const { bothAgree, startTogether } = require("./together");

test("a page with a token in place of the cookie joins a room and gets the steps", async ({ browser }) => {
  test.setTimeout(420_000);
  const { host, guest, forces, gid } = await startTogether(browser);
  const token = (await guest.evaluate(async () => (await fetch("api/session-token")).json())).token;
  expect(token).toMatch(/^[\w-]+\.[\w-]+$/);
  await guest.close();

  // A new context has no cookie. Without the token, the page of the game sends it to the sign-in.
  const context = await browser.newContext();
  const page = await context.newPage();
  await page.goto(`/?game=${gid}`);
  await expect(page).toHaveURL(/\/login/);
  await page.goto(`/?activity=1&game=${gid}#token=${token}`);
  expect((await context.cookies()).length).toBe(0);
  await page.waitForFunction(() => typeof Module !== "undefined" && typeof Module._wasm_test_state === "function",
                             null, { timeout: 120_000 });
  // The page joins the game at once, without the title menu. Before its first frame, the hook has no engine.
  await expect.poll(async () => { try { return (await h.gameState(page)).shared; } catch (error) { return -1; } },
                    { timeout: 120_000 }).toBe(1);
  expect((await h.gameState(page)).local_force).toBe(forces[1]);
  let state = await bothAgree(host, page);

  // The players end their faction turns. The page of the token sends and gets the steps.
  for (let round = 0; round < 2; round++) {
    const actor = (await h.gameState(host)).local_acts ? host : page;
    expect(await h.call(actor, "wasm_test_end_turn", "number", [], [])).toBeGreaterThan(0);
    await expect.poll(async () => (await h.gameState(host)).steps).toBeGreaterThan(state.steps);
    state = await bothAgree(host, page);
  }
  expect((await h.gameState(page)).net_error).toBe("");
  expect((await context.cookies()).length).toBe(0);
});
