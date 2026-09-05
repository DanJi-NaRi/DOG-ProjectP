const assert = require("node:assert/strict");
const dgram = require("node:dgram");
const test = require("node:test");

const {
  findAvailableDungeonPort,
  isUdpPortAvailable,
  waitForUdpPortBound,
} = require("../GameBackend/dungeon-manager-port");

function bindUdpPort(port = 0) {
  return new Promise((resolve, reject) => {
    const socket = dgram.createSocket("udp4");
    socket.once("error", reject);
    socket.once("listening", () => resolve(socket));
    socket.bind(port, "0.0.0.0");
  });
}

function closeUdpPort(socket) {
  return new Promise((resolve) => socket.close(resolve));
}

test("isUdpPortAvailable returns false when UDP port is already bound", async (t) => {
  const socket = await bindUdpPort();
  t.after(() => socket.close());

  const port = socket.address().port;

  assert.equal(await isUdpPortAvailable(port), false);
});

test("findAvailableDungeonPort returns 0 when every candidate UDP port is bound", async (t) => {
  const socket = await bindUdpPort();
  t.after(() => socket.close());

  const port = socket.address().port;

  const result = await findAvailableDungeonPort({
    portStart: port,
    portEnd: port,
    sessions: new Map(),
  });

  assert.equal(result, 0);
});

test("waitForUdpPortBound waits until UDP port is bound", async (t) => {
  const probeSocket = await bindUdpPort();
  const port = probeSocket.address().port;
  await closeUdpPort(probeSocket);

  let heldSocket = null;
  let bindTimer = null;
  const bindPromise = new Promise((resolve, reject) => {
    bindTimer = setTimeout(() => {
      bindUdpPort(port)
        .then((socket) => {
          heldSocket = socket;
          resolve();
        })
        .catch(reject);
    }, 50);
  });

  t.after(() => {
    clearTimeout(bindTimer);
    if (heldSocket) {
      heldSocket.close();
    }
  });

  const result = await waitForUdpPortBound({
    port,
    timeoutMs: 1000,
    pollIntervalMs: 20,
  });
  await bindPromise;

  assert.equal(result, true);
});
