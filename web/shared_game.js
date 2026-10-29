// The script of a shared game in the page (docs/MULTIPLAYER.md). web/shell.html loads it
// before the game. It uses siteAuth, hexdomServer, element() and postJson() of the page.

// MP5: the list "My games". "Your turn" marks each game where the faction of the member acts.
async function showMyGames() {
  var list = await fetch("api/games").then(function (r) { return r.ok ? r.json() : null; }).catch(function () { return null; });
  if (!list || !list.games.length) return;
  var panel = element("div", undefined, "games");
  panel.appendChild(element("h2", "My games"));
  var items = element("ul");
  list.games.forEach(function (game) {
    var item = element("li");
    var link = element("a", game.name + " (" + game.state + ")", "game-" + game.gid);
    link.href = "?game=" + game.gid;
    item.appendChild(link);
    if (game.your_turn) item.appendChild(element("strong", " Your turn", "turn-" + game.gid));
    items.appendChild(item);
  });
  panel.appendChild(items);
  var close = element("button", "Close", "games-close");
  close.type = "button";
  panel.appendChild(close);
  document.body.appendChild(panel);
  // The list lies over the game. It goes away at the first click on the game.
  function hide() { panel.remove(); Module.canvas.removeEventListener("pointerdown", hide); }
  close.addEventListener("click", hide);
  Module.canvas.addEventListener("pointerdown", hide);
}

/* ---- A shared game (docs/MULTIPLAYER.md, MP3; web/rooms.py) -----------------------
   The page owns the lobby and the socket of the room. The game and the page talk
   through Module.hexdomNetSend (the game sends) and Module.hexdomNetPush (the page
   pushes an event; code/wasm_hexdom.cpp). The kinds are platform_net_kind of
   code/hexdom.h.
   - "Play online" in the game: the page makes the room, sends the start image and
     shows the lobby with the invite link.
   - The lobby: each member takes a seat. The host starts the game.
   - The start: the page opens the socket. After the welcome, it gives the start
     image and the seats to the game (join), then each step of the socket.
   The page shows names with textContent, never as HTML. */
var NET = { HOST: 1, STEP: 2, CHECKSUM: 3, JOIN: 4, SEATS: 5, DESYNC: 6, ERROR: 7, TIMER: 8, PACK: 9, SNAPSHOT: 10,
            OPEN_ROOM: 11, STATE_IMAGE: 12, CHAT: 13 };
var NET_POLL_MS = 2000;
var RECONNECT_SECONDS = [1, 2, 4, 8, 15];   // after a lost connection; then each 15 seconds (MP4)
var TURN_CHOICES = { live: [120, 180, 300, 600], correspondence: [12 * 3600, 24 * 3600, 48 * 3600, 72 * 3600] };
var net = { build: null, ready: false, early: [], gid: null, room: null, socket: null, joined: false,
            online: [], poll: null, lastStep: -1, outbox: [], retries: 0 };

// The events wait until the game runs and the join went first, in their order.
function flushEarly() {
  var early = net.early;
  net.early = [];
  early.forEach(function (event) { Module.hexdomNetPush.apply(null, event); });
}
Module.hexdomNetReady = function () {
  net.ready = true;
  if (net.joined) flushEarly();
};
function netPush(kind, step, seat, host, bytes, text) {
  var event = [kind, step, seat, host, bytes || null, text || ""];
  if (net.ready && net.joined && !net.early.length) Module.hexdomNetPush.apply(null, event);
  else net.early.push(event);
}

Module.hexdomNetSend = function (kind, step, low, high, acting, bytes, text, pack) {
  if (kind === NET.HOST) hostRoom(bytes, text, pack).catch(function (error) { roomStatus("The room was not made: " + error); });
  else if (kind === NET.STEP) sendStep(step, bytes);
  else if (kind === NET.SNAPSHOT) putSnapshot(step, hexChecksum(low, high), bytes, 0);
  else if (kind === NET.OPEN_ROOM && /^[A-Z]{6}$/.test(text)) location.href = "/r/" + text;
  else if (kind === NET.CHAT) sendJson({ type: "chat", text: text });   // MP7: the server cleans the line
  else if (kind === NET.STATE_IMAGE) {
    // MP6: the image of the state after the step of a desync, for the developer.
    fetch("api/games/" + net.gid + "/desync/" + step, { method: "PUT", body: bytes })
      .catch(function (error) { console.log("platform: room: the state image did not upload: " + error); });
  }
  else if (kind === NET.CHECKSUM) sendJson({ type: "checksum", step: step, checksum: hexChecksum(low, high), acting: acting });
};

function hexChecksum(low, high) {
  return ("00000000" + high.toString(16)).slice(-8) + ("00000000" + low.toString(16)).slice(-8);
}

// MP5: the snapshot of the end of a game turn. The report of the step goes through the
// socket, and the server can get the snapshot first: then the page tries again.
var SNAPSHOT_TRIES = 4;
function putSnapshot(step, checksum, bytes, tries) {
  fetch("api/games/" + net.gid + "/snap/" + step + "?checksum=" + checksum, { method: "PUT", body: bytes })
    .then(function (response) {
      if (response.ok) { console.log("platform: room: the snapshot of step " + step + " is on the server"); return; }
      if (tries + 1 < SNAPSHOT_TRIES) setTimeout(function () { putSnapshot(step, checksum, bytes, tries + 1); }, 1000 * (tries + 1));
      else console.log("platform: room: the server refused the snapshot of step " + step + " (" + response.status + ")");
    })
    .catch(function (error) { console.log("platform: room: the snapshot did not upload: " + error); });
}


async function hostRoom(image, text, pack) {
  var factions = text.split("\n").filter(Boolean).map(function (line) {
    var parts = line.split("\t");
    return { force: parseInt(parts[0], 10), name: parts[1] || "" };
  });
  var name = (hexdomServer.me ? hexdomServer.me.name + "'s game" : "Online game");
  // The room records the pack: each player starts the engine of that pack (MP4).
  var body = { name: name, scenario: "", pack: pack, mode: "live", turn_seconds: 300, factions: factions };
  if (siteAuth.channel) body.channel = siteAuth.channel;   // MP8: the room of the voice channel of the Activity
  var room = await postJson("api/games", body);
  var put = await fetch("api/games/" + room.gid + "/start-image", { method: "PUT", body: image });
  if (!put.ok) throw new Error("the start image did not upload (" + put.status + ")");
  history.replaceState(null, "", location.pathname + "?game=" + room.gid);
  openRoom(room.gid);
}

function roomPanel() {
  var panel = document.getElementById("room");
  if (!panel) {
    panel = document.createElement("div");
    panel.id = "room";
    document.body.appendChild(panel);
  }
  return panel;
}
function roomStatus(text) {
  console.log("platform: room: " + text);
  var line = document.getElementById("room-status");
  if (line) line.textContent = text;
}

function openRoom(gid) {
  net.gid = gid;
  refreshRoom();
  if (!net.poll) net.poll = setInterval(refreshRoom, NET_POLL_MS);
}

async function refreshRoom() {
  var response = await fetch("api/games/" + net.gid).catch(function () { return null; });
  if (!response || !response.ok) { roomStatus("The room does not load."); return; }
  net.room = await response.json();
  drawRoom();
  // MP7: a member of the house without a seat watches the game after a click on "Watch".
  if (net.room.state !== "open" && !net.socket && !net.preparing && net.room.you_viewer) {
    clearInterval(net.poll);
    net.poll = null;
    net.preparing = true;
    await loadJoinImage();
    connectRoom();
  }
}

// MP5: the latest snapshot and the log after it, or the start image and the whole log.
// The hello then asks only for the steps after the snapshot.
async function loadJoinImage() {
  var response = await fetch("api/games/" + net.gid + "/snap/latest").catch(function () { return null; });
  var step = response && response.ok ? parseInt(response.headers.get("X-Snapshot-Step"), 10) : NaN;
  if (!isNaN(step)) {
    net.image = new Uint8Array(await response.arrayBuffer());
    net.joinStep = step + 1;
    net.lastStep = step;
    console.log("platform: room: the game goes on from the snapshot of step " + step);
    return;
  }
  response = await fetch("api/games/" + net.gid + "/start-image");
  if (!response.ok) { roomStatus("The start image does not load."); return; }
  net.image = new Uint8Array(await response.arrayBuffer());
  net.joinStep = 0;
}

function drawRoom() {
  var room = net.room, panel = roomPanel();
  panel.replaceChildren();
  panel.appendChild(element("h2", room.name));
  if (room.state === "open") {
    var link = location.origin + "/r/" + room.code;
    panel.appendChild(element("p", "Invite link: " + link, "room-link"));
    panel.appendChild(element("p", "Room code: " + room.code, "room-code"));
  }
  if (room.state === "open" && room.you_host) panel.appendChild(roomOptions(room));
  var list = element("ul");
  room.seats.forEach(function (seat) {
    var who = seat.user ? (seat.ai ? "the computer, while " + seat.user + " is away" : seat.user) : "the computer";
    var item = element("li", seat.name + ": " + who);
    item.className = "seat";
    if (room.state === "open") {
      var take = element("button", seat.user ? "Leave" : "Take", "seat-" + seat.force);
      take.type = "button";
      take.addEventListener("click", function () {
        postJson("api/games/" + net.gid + "/seat", { force: seat.user ? null : seat.force })
          .then(refreshRoom).catch(function (error) { roomStatus(String(error.message || error)); });
      });
      item.appendChild(take);
    }
    list.appendChild(item);
  });
  panel.appendChild(list);
  if (room.state !== "open" && !room.you_viewer) {
    if (room.spectators === false) panel.appendChild(element("p", "The host allows no spectators in this game."));
    else {
      var watch = element("button", "Watch", "room-watch");
      watch.type = "button";
      watch.addEventListener("click", function () {
        postJson("api/games/" + net.gid + "/watch", {}).then(refreshRoom)
          .catch(function (error) { roomStatus(String(error.message || error)); });
      });
      panel.appendChild(watch);
    }
  }
  if (room.state === "open") {
    var start = element("button", "Start the game", "room-start");
    start.type = "button";
    start.addEventListener("click", function () {
      postJson("api/games/" + net.gid + "/start", {}).then(refreshRoom).catch(function (error) { roomStatus(String(error.message || error)); });
    });
    panel.appendChild(start);
  }
  panel.appendChild(element("p", "", "room-status"));
  panel.style.display = net.joined ? "none" : "block";
}

// The options of the room before the start (MP4). Only the host can change them:
// the server refuses the request of a guest.
function roomOptions(room) {
  var box = element("p", undefined, "room-options");
  var mode = element("select", undefined, "room-mode");
  ["live", "correspondence"].forEach(function (value) {
    var option = element("option", value === "live" ? "Live" : "Correspondence");
    option.value = value;
    mode.appendChild(option);
  });
  mode.value = room.mode;
  var turn = element("select", undefined, "room-turn");
  var choices = TURN_CHOICES[room.mode].slice();
  if (choices.indexOf(room.turn_seconds) < 0) choices.unshift(room.turn_seconds);
  choices.forEach(function (seconds) {
    var option = element("option", seconds >= 3600 ? seconds / 3600 + " h" : seconds >= 120 ? seconds / 60 + " min" : seconds + " s");
    option.value = String(seconds);
    turn.appendChild(option);
  });
  turn.value = String(room.turn_seconds);
  var pause = element("input", undefined, "room-pause");
  pause.type = "checkbox";
  pause.checked = !!room.pause_absent;
  var label = element("label", " Pause when a player is absent");
  label.prepend(pause);
  var watch = element("input", undefined, "room-spectators");   // MP7: the host decides (Q-11)
  watch.type = "checkbox";
  watch.checked = room.spectators !== false;
  var watchLabel = element("label", " Spectators can watch");
  watchLabel.prepend(watch);
  function send(changed) {
    var body = { mode: mode.value, turn_seconds: parseInt(turn.value, 10), pause_absent: pause.checked, spectators: watch.checked };
    if (changed === "mode") body.turn_seconds = TURN_CHOICES[mode.value][2];
    setRoomOptions(body);
  }
  mode.addEventListener("change", function () { send("mode"); });
  turn.addEventListener("change", function () { send("turn"); });
  pause.addEventListener("change", function () { send("pause"); });
  watch.addEventListener("change", function () { send("spectators"); });
  box.append("A faction turn: ", mode, " ", turn, " ", label, " ", watchLabel);
  return box;
}

function setRoomOptions(body) {
  return postJson("api/games/" + net.gid + "/options", body).then(refreshRoom)
    .catch(function (error) { roomStatus(String(error.message || error)); });
}

function seatsText(withOnline) {
  return net.room.seats.map(function (seat) {
    var line = seat.force + "\t" + (seat.user || "");
    if (withOnline) line += "\t" + (net.online.indexOf(seat.force) >= 0 ? 1 : 0);
    return line;
  }).join("\n");
}

// The hello gives the last step of the page: after a lost connection, the server sends
// only the steps after it (MP4). The page tries again after 1, 2, 4 and 8 seconds,
// then each 15 seconds.
function connectRoom() {
  var scheme = location.protocol === "https:" ? "wss://" : "ws://";
  var address = scheme + location.host + location.pathname.replace(/[^/]*$/, "") + "api/games/" + net.gid + "/socket";
  var socket = new WebSocket(address + (siteAuth.token ? "?token=1" : ""));   // MP8: the hello carries the token
  socket.binaryType = "arraybuffer";
  net.socket = socket;
  socket.onopen = function () {
    var hello = { type: "hello", gid: net.gid, build: net.build || net.room.build, last_step: net.lastStep };
    if (siteAuth.token) hello.token = siteAuth.token;
    socket.send(JSON.stringify(hello));
  };
  socket.onmessage = function (event) {
    if (typeof event.data !== "string") {
      var view = new DataView(event.data);
      var number = view.getUint32(0, true);
      if (number <= net.lastStep) return;   // the page has this step
      net.lastStep = number;
      netPush(NET.STEP, number, 0, false, new Uint8Array(event.data, 8), "");
      return;
    }
    var message = JSON.parse(event.data);
    if (message.type === "welcome") welcomeRoom(message);
    else if (message.type === "presence") { net.online = message.seats; netPush(NET.SEATS, 0, 0, false, null, seatsText(true)); }
    else if (message.type === "timer") { netPush(NET.TIMER, message.seconds === null ? -1 : message.seconds, 0, false, null, ""); markYourTurn(message.acting); }
    else if (message.type === "chat") netPush(NET.CHAT, 0, 0, false, null, message.name + "\t" + message.text);
    else if (message.type === "desync") netPush(NET.DESYNC, message.step, 0, false, null, "");
    else if (message.type === "desync_end") netPush(NET.DESYNC, -1, 0, false, null, "");
    else if (message.type === "reload") loadGoodState(message.step);
    else if (message.type === "error") { roomStatus(message.reason); netPush(NET.ERROR, 0, 0, false, null, message.reason); }
  };
  socket.onclose = function () {
    if (net.socket !== socket) return;
    net.socket = null;
    var wait = RECONNECT_SECONDS[Math.min(net.retries, RECONNECT_SECONDS.length - 1)];
    net.retries++;
    roomStatus("The connection to the room closed. The page tries again in " + wait + " s.");
    setTimeout(connectRoom, wait * 1000);
  };
}

// MP6: the room decided a desync against this page. The page loads the latest good
// snapshot (or the start image) and the log after it: a new socket asks for the steps
// after the snapshot, and the join then loads the good state into the game.
async function loadGoodState(step) {
  console.log("platform: room: the state of this page differs at step " + step + "; the page loads the good state");
  var socket = net.socket;
  net.socket = null;
  net.joined = false;
  net.early = [];
  net.outbox = [];
  if (socket) { socket.onclose = null; socket.close(); }
  await loadJoinImage();
  if (!net.joinStep) net.lastStep = -1;
  connectRoom();
}

// MP7 (Q-6): the notice "Your turn" is a badge of the site. The title of the tab shows it.
function markYourTurn(acting) {
  var yours = net.seat !== null && net.seat !== undefined && acting === net.seat;
  document.title = yours ? "● Your turn - hexdom" : "hexdom";
}

function welcomeRoom(welcome) {
  net.retries = 0;
  net.room = welcome.room;
  net.seat = welcome.seat;
  if (net.joined) {
    // A new connection: the messages that waited go to the room now.
    var outbox = net.outbox;
    net.outbox = [];
    outbox.forEach(function (message) { net.socket.send(message); });
    roomStatus("");
    return;
  }
  joinRoom(welcome);
}

function joinRoom(welcome) {
  if (!net.image) { roomStatus("The start image does not load."); return; }
  var seat = welcome.seat === null ? 0xFF : welcome.seat;
  // The pack, then the join, go before the steps of the socket. The pack can start the
  // engine of another pack first (MP4). The step of the join is the first step after a
  // snapshot, or 0 for the start image (MP5).
  // MP7: the last lines of the chat of the room follow the join.
  var history = (net.room.chat || []).map(function (line) { return [NET.CHAT, 0, 0, false, null, line.name + "\t" + line.text]; });
  net.early.unshift.apply(net.early, [[NET.JOIN, net.joinStep, seat, welcome.host, net.image, seatsText(false)]].concat(history));
  net.image = null;
  if (net.room.pack) net.early.unshift([NET.PACK, 0, 0, false, null, net.room.pack]);
  net.joined = true;
  if (net.ready) flushEarly();
  drawRoom();
}

// A message for the room. Without a connection, it waits in the outbox.
function sendToRoom(message) {
  if (net.socket && net.socket.readyState === WebSocket.OPEN) net.socket.send(message);
  else net.outbox.push(message);
}
function sendJson(message) {
  sendToRoom(JSON.stringify(message));
}
function sendStep(step, bytes) {
  var message = new Uint8Array(8 + bytes.length);
  new DataView(message.buffer).setUint32(0, step, true);
  message.set(bytes, 8);
  net.lastStep = Math.max(net.lastStep, step);
  sendToRoom(message);
}
