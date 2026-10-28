// The tests of the room server through the real site (web/rooms.py; docs/MULTIPLAYER.md, MP2).
// The page needs no game for these tests: they use fetch and a WebSocket of the page.
const { test, expect } = require("@playwright/test");

const HOUSE_PASSWORD = "house-test";

// Sign in with the test provider and join the house. The test does not wait for the game.
async function signInOnly(page) {
  await page.goto("/login");
  await page.click("#login-dev");
  await page.waitForURL((url) => url.pathname === "/house" || url.pathname === "/");
  if (new URL(page.url()).pathname === "/house") {
    await page.fill("#house-password", HOUSE_PASSWORD);
    await page.click("#house-submit");
    await page.waitForURL((url) => url.pathname === "/");
  }
}

test("the socket of a room refuses a person without a member session", async ({ request }) => {
  const response = await request.get("/api/games/0123456789abcdef/socket");
  expect(response.status()).toBe(401);
  expect((await request.get("/api/games")).status()).toBe(401);
});

test("the socket of a room refuses a page of another origin", async ({ page }) => {
  await signInOnly(page);
  const response = await page.request.get("/api/games/0123456789abcdef/socket", { headers: { Origin: "http://evil.test" } });
  expect(response.status()).toBe(403);
});

test("a member makes a room, takes a seat, starts it and gets the welcome on the socket", async ({ page }) => {
  await signInOnly(page);
  const result = await page.evaluate(async () => {
    const post = (url, body) => fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
    const made = await post("/api/games", { name: "Test", scenario: "SCEN002", pack: "placeholder", mode: "live", turn_seconds: 300,
                                            factions: [{ force: 2, name: "Cao" }, { force: 5, name: "Liu" }] });
    const room = await made.json();
    await post(`/api/games/${room.gid}/seat`, { force: 2 });
    await fetch(`/api/games/${room.gid}/start-image`, { method: "PUT", body: new Uint8Array([1, 2, 3]) });
    const started = await (await post(`/api/games/${room.gid}/start`, {})).json();
    const games = await (await fetch("/api/games")).json();
    const welcome = await new Promise((resolve, reject) => {
      const socket = new WebSocket(`${location.origin.replace("http", "ws")}/api/games/${room.gid}/socket`);
      socket.onopen = () => socket.send(JSON.stringify({ type: "hello", build: room.build, last_step: -1 }));
      socket.onmessage = (event) => {
        const message = JSON.parse(event.data);
        if (message.type === "welcome") { socket.close(); resolve(message); }
      };
      socket.onerror = () => reject(new Error("the socket failed"));
      setTimeout(() => reject(new Error("no welcome")), 10000);
    });
    return { status: made.status, room, started, games, welcome };
  });
  expect(result.status).toBe(201);
  expect(result.room.gid).toMatch(/^[0-9a-f]{16}$/);
  expect(result.room.code).toMatch(/^[A-HJKMNP-Z]{6}$/);
  expect(result.started.state).toBe("running");
  expect(result.games.games.map((game) => game.gid)).toContain(result.room.gid);
  expect(result.welcome.seat).toBe(2);
  expect(result.welcome.host).toBe(true);
  expect(result.welcome.room.seats[0].user).toBe("Tester");
});
