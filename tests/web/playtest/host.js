// The host page of a play test (docs/PLAYTEST.md). In a game of the AI alone, the frames of the
// host page end each game turn (ui_net_end_game_turn_of_the_ai). A hidden tab gets no frames, and the
// game stops. This script opens the game again as the host user in a page that stays visible, and
// records it. Usage: node tests/web/playtest/host.js <game url> <user name> [out folder]
// Stop: make the file STOP in the out folder.
const fs = require("fs");
const path = require("path");
const { chromium } = require("@playwright/test");

const gameUrl = new URL(process.argv[2]);
const name = process.argv[3];
const out = path.resolve(process.argv[4] || "build/playtest");

(async () => {
  const browser = await chromium.launch({ channel: "chromium", args: ["--use-angle=d3d11", "--enable-gpu", "--ignore-gpu-blocklist"] });
  const context = await browser.newContext({ viewport: { width: 1280, height: 800 },
                                             recordVideo: { dir: path.join(out, "video-" + name), size: { width: 1280, height: 800 } } });
  const page = await context.newPage();
  page.on("console", (message) => {
    if (/^platform|desync|error/i.test(message.text())) fs.appendFileSync(path.join(out, "console.jsonl"), JSON.stringify({ t: Date.now(), who: name, text: message.text() }) + "\n");
  });
  await page.goto(`${gameUrl.origin}/auth/dev/start?name=${name}`);
  await page.waitForURL((url) => url.pathname === "/");
  await page.goto(gameUrl.href);
  console.log("the host page is open");
  while (!fs.existsSync(path.join(out, "STOP")) && !page.isClosed()) await new Promise((resolve) => setTimeout(resolve, 2000));
  await context.close();
  await browser.close();
  console.log("the host video is saved");
})().catch((error) => { console.error(error); process.exit(1); });
