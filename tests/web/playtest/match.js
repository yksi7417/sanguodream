// The driver of a play test (docs/PLAYTEST.md). It is not a test of "npm test".
// A host makes the room in another browser and gives the invite link. This driver then:
// - signs in a guest, takes the second seat, and plays its first turns by hand;
// - then gives the faction of the guest to the AI ("AI plays for me");
// - signs in a spectator that records the full game as video;
// - writes the state of each faction at each turn, and each caption of "Follow the action".
// Usage: node tests/web/playtest/match.js <invite link> [out folder]
// Stop: make the file STOP in the out folder, or wait for MAX_TURNS.
const fs = require("fs");
const path = require("path");
const { chromium } = require("@playwright/test");

const link = process.argv[2];
const out = path.resolve(process.argv[3] || "build/playtest");
const seatForce = Number(process.env.GUEST_FORCE || -1);   // -1: the first free seat
const handTurns = Number(process.env.GUEST_HAND_TURNS || 3);
const maxTurns = Number(process.env.MAX_TURNS || 150);
const base = new URL(link).origin;
fs.mkdirSync(path.join(out, "shots"), { recursive: true });

const log = (name, row) => fs.appendFileSync(path.join(out, name), JSON.stringify({ t: Date.now(), ...row }) + "\n");
const say = (text) => { console.log(new Date().toISOString().slice(11, 19) + " " + text); log("driver.jsonl", { text }); };
const call = (page, name, args = []) => page.evaluate(([n, a]) => Module.ccall(n, "string", a.map(() => "string"), a), [name, args]);
const state = async (page) => JSON.parse(await call(page, "wasm_test_state"));
const factions = async (page) => JSON.parse(await call(page, "wasm_test_factions"));
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function frames(page, count) {
  await page.evaluate(async (n) => { for (let i = 0; i < n; i++) await new Promise((r) => requestAnimationFrame(r)); }, count);
}

async function clickItem(page, id) {
  for (let tries = 0; tries < 50; tries++) {
    const rect = JSON.parse(await page.evaluate((i) => Module.ccall("wasm_test_item", "string", ["string"], [i]), id));
    if (rect) {
      await page.mouse.move((rect.x0 + rect.x1) / 2, (rect.y0 + rect.y1) / 2);
      await frames(page, 3); await page.mouse.down(); await frames(page, 3); await page.mouse.up(); await frames(page, 3);
      return true;
    }
    await sleep(200);
  }
  return false;
}

async function waitForGame(page) {
  await page.waitForFunction(() => typeof Module !== "undefined" && typeof Module._wasm_test_item === "function", null, { timeout: 180_000 });
  for (let tries = 0; tries < 600; tries++) {
    const ids = JSON.parse(await page.evaluate(() => Module.ccall("wasm_test_item_ids", "string", ["string"], [""])));
    if (ids.length) return;
    await sleep(200);
  }
}

async function member(browser, name) {
  const context = await browser.newContext({ viewport: { width: 1280, height: 800 },
                                             recordVideo: { dir: path.join(out, "video-" + name), size: { width: 1280, height: 800 } } });
  const page = await context.newPage();
  page.on("console", (message) => { if (/^platform|desync|error/i.test(message.text())) log("console.jsonl", { who: name, text: message.text() }); });
  await page.goto(`${base}/auth/dev/start?name=${name}`);
  await page.waitForURL((url) => url.pathname === "/");
  return { context, page };
}

(async () => {
  const browser = await chromium.launch({ channel: "chromium", args: ["--use-angle=d3d11", "--enable-gpu", "--ignore-gpu-blocklist"] });
  const guest = await member(browser, "Rival");
  await guest.page.goto(link);
  await guest.page.waitForURL((url) => url.searchParams.has("game"));
  const gid = new URL(guest.page.url()).searchParams.get("game");
  await waitForGame(guest.page);
  const room = await guest.page.evaluate(async (id) => (await fetch(`api/games/${id}`)).json(), gid);
  const free = room.seats.filter((seat) => !seat.user);
  const force = seatForce >= 0 ? seatForce : free[0].force;
  await guest.page.click(`#seat-${force}`);
  say(`the guest Rival took the seat of force ${force} in game ${gid}; the host can start now`);
  fs.writeFileSync(path.join(out, "game.json"), JSON.stringify({ gid, link, force, seats: room.seats }, null, 1));

  // Wait for the start, then the spectator joins and records.
  for (;;) {
    const s = await state(guest.page).catch(() => null);
    if (s && s.shared) break;
    await sleep(1000);
  }
  say("the game started");
  const watcher = await member(browser, "Camera");
  await watcher.page.goto(`${base}/?game=${gid}`);
  await waitForGame(watcher.page);
  await watcher.page.click("#room-watch", { timeout: 60_000 });
  for (;;) { const s = await state(watcher.page).catch(() => null); if (s && s.shared) break; await sleep(1000); }
  await clickItem(watcher.page, "net_follow");   // the spectator follows each action with its caption
  say("the spectator Camera watches and follows the action");

  let handLeft = handTurns, autoOn = false, lastKey = "", lastCaption = "", actsSince = 0;
  for (;;) {
    if (fs.existsSync(path.join(out, "STOP"))) { say("STOP file"); break; }
    let w, f, g;
    try { [w, f, g] = [await state(watcher.page), await factions(watcher.page), await state(guest.page)]; }
    catch (error) { say("read error: " + error.message); await sleep(2000); continue; }
    const key = `${f.turn}/${w.whose_turn}`;
    if (key !== lastKey) {
      lastKey = key;
      log("turns.jsonl", { turn: f.turn, date: `${f.year}-${f.month}-${f.day}`, whose: w.whose_turn, steps: w.steps,
                           checksum: w.checksum, guest_checksum: g.checksum, guest_steps: g.steps, net_error: w.net_error || g.net_error,
                           humans: w.humans, factions: f.factions });
      await watcher.page.screenshot({ path: path.join(out, "shots", `t${String(f.turn).padStart(3, "0")}-f${w.whose_turn}.jpg`), quality: 70, type: "jpeg" });
    }
    const caption = `${f.who}|${f.what}|${f.where}`;
    if (f.who && caption !== lastCaption) { lastCaption = caption; log("captions.jsonl", { turn: f.turn, who: f.who, what: f.what, where: f.where }); }
    if (w.net_ended || f.turn >= maxTurns) { say(`end: turn ${f.turn}, ended ${w.net_ended}`); break; }

    // The guest plays its first turns by hand: it looks at the map, then ends the turn.
    if (g.local_acts && !autoOn) {
      if (!actsSince) actsSince = Date.now();
      if (handLeft > 0 && Date.now() - actsSince > 8000) {
        const id = await guest.page.evaluate(() => Module.ccall("wasm_test_end_turn", "number", [], []));
        say(`the guest ended its turn ${f.turn} by hand (command ${id})`);
        handLeft--; actsSince = 0;
      } else if (handLeft === 0) {
        autoOn = await clickItem(guest.page, "net_auto");
        say(`the guest chose "AI plays for me": ${autoOn}`);
      }
    }
    await sleep(1500);
  }
  await guest.context.close();
  await watcher.context.close();
  await browser.close();
  say("the videos are saved");
})().catch((error) => { console.error(error); process.exit(1); });
