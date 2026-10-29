// The Playwright smoke tests of the web game (docs/WEB.md, "The browser tests").
// Build the test game first: build_web.bat test-game. The config starts web/server.py
// with the test login, a storage folder and the house password "house-test".
const path = require("path");
const { defineConfig } = require("@playwright/test");

const repo = path.resolve(__dirname, "..", "..");
const port = 8095;
const python = process.env.HEXDOM_PYTHON ||
  (process.platform === "win32" ? "C:\\tools\\hexdom-venv\\Scripts\\python.exe" : "python3");

function glArguments() {
  if (process.platform === "win32" && process.env.HEXDOM_SOFTWARE_GL !== "1") {
    return ["--use-angle=d3d11", "--enable-gpu", "--ignore-gpu-blocklist"];
  }
  return ["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"];
}

module.exports = defineConfig({
  testDir: __dirname,
  timeout: 240_000,
  expect: { timeout: 30_000 },
  workers: 1,
  reporter: "list",
  globalSetup: require.resolve("./global-setup.js"),
  use: {
    baseURL: `http://127.0.0.1:${port}`,
    viewport: { width: 1280, height: 800 },
    deviceScaleFactor: 1,
    // The new headless mode of Chromium can use the GPU. On Windows the tests draw with
    // D3D11. Elsewhere, and with HEXDOM_SOFTWARE_GL=1, they draw with SwiftShader, at
    // approximately 5 frames each second.
    channel: "chromium",
    launchOptions: { args: glArguments() },
  },
  webServer: {
    command: `"${python}" -u web/server.py build/webtest`,
    cwd: repo,
    url: `http://127.0.0.1:${port}/healthz`,
    timeout: 120_000,
    reuseExistingServer: false,
    env: {
      PORT: String(port),
      SESSION_SECRET: "playwright-session-secret",
      HOUSE_PASSWORD: "house-test",
      AUTH_DEV_LOGIN: "1",
      // The test login gives the email <name>@dev.test (web/auth.py). Tester is an admin.
      ADMIN_EMAILS: "tester@dev.test",
      STORAGE_DIR: path.join(repo, "build", "webtest-storage"),
      FLY_APP_NAME: "",
      BUCKET_NAME: "",
      YOUTUBE_STREAM_KEY: "",
      // MP4: the test of an absent player uses a faction turn of a few seconds (web/rooms.py).
      HEXDOM_TEST_MIN_TURN_SECONDS: "4",
    },
  },
});
