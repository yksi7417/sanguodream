// Before the tests: an empty storage folder with one save of the test user on the
// server. The save is tests/fixtures/saves/slot_01.hxs, the start of the sample
// (tests/feature/save_compat_tests.cpp).
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const repo = path.resolve(__dirname, "..", "..");
const TEST_USER = "Tester";

module.exports = async function globalSetup() {
  const storage = path.join(repo, "build", "webtest-storage");
  fs.rmSync(storage, { recursive: true, force: true });
  // The uid of a user is the first 24 hex digits of sha256("<provider>:<subject>") (web/auth.py).
  const uid = crypto.createHash("sha256").update(`dev:${TEST_USER}`).digest("hex").slice(0, 24);
  const saves = path.join(storage, "saves", uid);
  fs.mkdirSync(saves, { recursive: true });
  fs.copyFileSync(path.join(repo, "tests", "fixtures", "saves", "slot_01.hxs"), path.join(saves, "slot_01.hxs"));
  if (!fs.existsSync(path.join(repo, "build", "webtest", "index.wasm"))) {
    throw new Error("no test game: run build_web.bat test-game");
  }
};
