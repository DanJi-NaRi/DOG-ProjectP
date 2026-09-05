const { execFile } = require("child_process");
const dgram = require("dgram");

function isUdpPortAvailable(port, host = "0.0.0.0") {
  return new Promise((resolve) => {
    const socket = dgram.createSocket("udp4");
    let bResolved = false;

    function finish(bAvailable) {
      if (bResolved) {
        return;
      }

      bResolved = true;
      try {
        socket.close();
      } catch {
      }
      resolve(bAvailable);
    }

    socket.once("error", () => finish(false));
    socket.once("listening", () => finish(true));
    socket.bind(port, host);
  });
}

async function findAvailableDungeonPort({ portStart, portEnd, sessions, host = "0.0.0.0" }) {
  for (let port = portStart; port <= portEnd; ++port) {
    if ([...(sessions?.values?.() ?? [])].some((session) => session.port === port)) {
      continue;
    }

    if (await isUdpPortAvailable(port, host)) {
      return port;
    }
  }

  return 0;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function isUdpPortBound(port) {
  return new Promise((resolve) => {
    if (!Number.isInteger(port) || port <= 0) {
      resolve(false);
      return;
    }

    const command = [
      "$ErrorActionPreference = 'SilentlyContinue';",
      `$port = ${port};`,
      "$endpoint = Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue | Select-Object -First 1;",
      "if ($null -eq $endpoint) { '0' } else { '1' }",
    ].join(" ");

    execFile(
      "powershell.exe",
      ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command],
      {
        timeout: 3000,
        windowsHide: true,
        maxBuffer: 1024 * 1024,
      },
      (error, stdout) => {
        if (error) {
          resolve(false);
          return;
        }

        resolve(String(stdout || "").trim() === "1");
      },
    );
  });
}

async function waitForUdpPortBound({ port, timeoutMs = 30000, pollIntervalMs = 250 } = {}) {
  const startedAt = Date.now();
  const maxWaitMs = Math.max(0, Number.isInteger(timeoutMs) ? timeoutMs : 30000);
  const intervalMs = Math.max(50, Number.isInteger(pollIntervalMs) ? pollIntervalMs : 250);

  while (Date.now() - startedAt <= maxWaitMs) {
    if (await isUdpPortBound(port)) {
      return true;
    }

    await sleep(intervalMs);
  }

  return false;
}

module.exports = {
  findAvailableDungeonPort,
  isUdpPortBound,
  isUdpPortAvailable,
  waitForUdpPortBound,
};
