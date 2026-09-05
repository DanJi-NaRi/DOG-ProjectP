const express = require("express");
const fs = require("fs");
const http = require("http");
const https = require("https");
const path = require("path");
const { execFile, spawn } = require("child_process");

const { loadConfig } = require("./config");
const {
  findAvailableDungeonPort,
  isUdpPortAvailable,
  waitForUdpPortBound,
} = require("./dungeon-manager-port");

async function bootstrap() {
  const config = loadConfig();
  const app = express();
  const projectRoot = path.resolve(__dirname, "..");
  const managerConfig = config.dungeonManager ?? {};
  const dungeonStateKey = typeof config.serverAuth?.dungeonStateKey === "string" ? config.serverAuth.dungeonStateKey.trim() : "";
  const sessions = new Map();
  const apiEvents = [];
  const maxApiEvents = Number.isInteger(config.monitor?.maxServerEvents)
    ? Math.max(20, config.monitor.maxServerEvents)
    : 300;
  const sensitiveFieldNames = new Set([
    "authorization",
    "auth",
    "authkey",
    "key",
    "password",
    "secret",
    "serverauthkey",
    "token",
    "x-server-auth",
  ]);

  function normalizeSensitiveKey(key) {
    return String(key || "").replace(/[^a-z0-9]/gi, "").toLowerCase();
  }

  function isSensitiveField(key) {
    const normalizedKey = normalizeSensitiveKey(key);
    if (sensitiveFieldNames.has(normalizedKey)) {
      return true;
    }

    return normalizedKey.includes("password")
      || normalizedKey.includes("token")
      || normalizedKey.includes("secret")
      || normalizedKey.includes("auth")
      || normalizedKey.endsWith("key");
  }

  function sanitizeForLog(value, depth = 0) {
    if (value === undefined) {
      return undefined;
    }

    if (value === null || typeof value === "number" || typeof value === "boolean") {
      return value;
    }

    if (typeof value === "string") {
      return value.length > 500 ? `${value.slice(0, 500)}...` : value;
    }

    if (depth >= 4) {
      return "[depth-limit]";
    }

    if (Array.isArray(value)) {
      const sanitizedItems = value.slice(0, 20).map((item) => sanitizeForLog(item, depth + 1));
      if (value.length > sanitizedItems.length) {
        sanitizedItems.push(`... ${value.length - sanitizedItems.length} more`);
      }
      return sanitizedItems;
    }

    if (typeof value === "object") {
      const sanitizedObject = {};
      const entries = Object.entries(value).slice(0, 40);
      for (const [key, item] of entries) {
        sanitizedObject[key] = isSensitiveField(key) ? "[masked]" : sanitizeForLog(item, depth + 1);
      }
      const hiddenCount = Object.keys(value).length - entries.length;
      if (hiddenCount > 0) {
        sanitizedObject.__truncated = `${hiddenCount} more fields`;
      }
      return sanitizedObject;
    }

    return String(value);
  }

  function getRequestPath(req) {
    return typeof req.path === "string" ? req.path : String(req.originalUrl || "").split("?")[0];
  }

  function getDungeonSessionIdFromBody(body) {
    return typeof body?.dungeonSessionId === "string" ? body.dungeonSessionId.trim() : "";
  }

  function shouldRecordApiRequest(req) {
    const requestPath = getRequestPath(req);
    if (!requestPath.startsWith("/api/")) {
      return false;
    }

    return !new Set([
      "/api/health",
      "/api/dungeon/sessions",
      "/api/telemetry/events",
    ]).has(requestPath);
  }

  function addApiEvent(direction, level, message, detail = {}) {
    apiEvents.unshift({
      id: `${Date.now()}-${apiEvents.length}`,
      serverId: "dungeonManager",
      time: new Date().toISOString(),
      direction,
      level,
      message,
      detail: sanitizeForLog(detail),
    });

    if (apiEvents.length > maxApiEvents) {
      apiEvents.length = maxApiEvents;
    }
  }

  function apiLevelFromStatus(statusCode) {
    if (statusCode >= 500 || statusCode === 0) {
      return "error";
    }

    if (statusCode >= 400) {
      return "warning";
    }

    return "info";
  }

  function hasValidServerAuth(req) {
    const requestKey = typeof req.get("X-Server-Auth") === "string" ? req.get("X-Server-Auth").trim() : "";
    return !!dungeonStateKey && requestKey === dungeonStateKey;
  }

  function getRequestBody(req) {
    return req.body && typeof req.body === "object" ? req.body : {};
  }

  function getGameBackendBaseUrl() {
    const configuredBaseUrl = typeof managerConfig.gameBackendBaseUrl === "string" ? managerConfig.gameBackendBaseUrl.trim().replace(/\/+$/, "") : "";
    if (configuredBaseUrl) {
      return configuredBaseUrl;
    }

    const serverConfig = config.server ?? {};
    const serverPort = Number.isInteger(serverConfig.port) ? serverConfig.port : 8080;
    const configuredHost = typeof serverConfig.host === "string" ? serverConfig.host.trim() : "";
    const serverHost = !configuredHost || configuredHost === "0.0.0.0" || configuredHost === "::" ? "127.0.0.1" : configuredHost;
    return `http://${serverHost}:${serverPort}`;
  }

  function postGameBackendJson(endpointPath, body) {
    return new Promise((resolve, reject) => {
      const requestUrl = new URL(endpointPath, `${getGameBackendBaseUrl()}/`);
      const requestBody = JSON.stringify(body ?? {});
      const client = requestUrl.protocol === "https:" ? https : http;
      const startedAt = Date.now();
      addApiEvent("send", "info", `POST ${endpointPath} -> GameBackend`, {
        method: "POST",
        path: endpointPath,
        targetServerId: "gameBackend",
        target: requestUrl.origin,
        dungeonSessionId: getDungeonSessionIdFromBody(body),
        requestBody: body ?? {},
      });
      const request = client.request(
        requestUrl,
        {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            "Content-Length": Buffer.byteLength(requestBody),
            "X-Server-Auth": dungeonStateKey,
          },
        },
        (response) => {
          response.resume();
          response.on("end", () => {
            addApiEvent("receive", apiLevelFromStatus(response.statusCode), `GameBackend ${endpointPath} -> HTTP ${response.statusCode}`, {
              method: "POST",
              path: endpointPath,
              sourceServerId: "gameBackend",
              statusCode: response.statusCode,
              durationMs: Date.now() - startedAt,
              dungeonSessionId: getDungeonSessionIdFromBody(body),
            });
            resolve(response.statusCode);
          });
        },
      );

      request.on("error", (error) => {
        addApiEvent("receive", "error", `GameBackend ${endpointPath} request failed`, {
          method: "POST",
          path: endpointPath,
          sourceServerId: "gameBackend",
          durationMs: Date.now() - startedAt,
          dungeonSessionId: getDungeonSessionIdFromBody(body),
          error: error.message,
        });
        reject(error);
      });
      request.write(requestBody);
      request.end();
    });
  }

  function getManagerPort() {
    return Number.isInteger(managerConfig.port) ? managerConfig.port : 8090;
  }

  function getPortStart() {
    return Number.isInteger(managerConfig.portStart) ? managerConfig.portStart : 7780;
  }

  function getPortEnd() {
    return Number.isInteger(managerConfig.portEnd) ? managerConfig.portEnd : 7799;
  }

  function getShutdownGraceMs() {
    return Number.isInteger(managerConfig.shutdownGraceMs) ? managerConfig.shutdownGraceMs : 5000;
  }

  function getShutdownForceWaitMs() {
    return Number.isInteger(managerConfig.shutdownForceWaitMs) ? managerConfig.shutdownForceWaitMs : 3000;
  }

  function getShutdownPortWaitMs() {
    return Number.isInteger(managerConfig.shutdownPortWaitMs) ? managerConfig.shutdownPortWaitMs : 2000;
  }

  function getShutdownPollIntervalMs() {
    return Number.isInteger(managerConfig.shutdownPollIntervalMs) ? managerConfig.shutdownPollIntervalMs : 250;
  }

  function getShutdownRetryIntervalMs() {
    return Number.isInteger(managerConfig.shutdownRetryIntervalMs) ? managerConfig.shutdownRetryIntervalMs : 10000;
  }

  function getShutdownRetryMaxCount() {
    return Number.isInteger(managerConfig.shutdownRetryMaxCount) ? managerConfig.shutdownRetryMaxCount : 5;
  }

  function getPublicHost() {
    const publicHost = typeof managerConfig.publicHost === "string" ? managerConfig.publicHost.trim() : "";
    return publicHost || "127.0.0.1";
  }

  function getServerExecutablePath() {
    const configuredPath = typeof managerConfig.serverExecutable === "string" ? managerConfig.serverExecutable.trim() : "";
    if (!configuredPath) {
      return "";
    }

    return path.isAbsolute(configuredPath) ? configuredPath : path.join(projectRoot, configuredPath);
  }

  function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, Math.max(0, ms)));
  }

  function waitForProcessExit(session, timeoutMs) {
    return new Promise((resolve) => {
      if (session.process.exitCode !== null || session.process.signalCode !== null) {
        resolve(true);
        return;
      }

      let bResolved = false;
      const finish = (bExited) => {
        if (bResolved) {
          return;
        }

        bResolved = true;
        clearTimeout(timer);
        session.process.off("exit", onExit);
        resolve(bExited);
      };
      const onExit = () => finish(true);
      const timer = setTimeout(() => finish(false), Math.max(0, timeoutMs));
      session.process.once("exit", onExit);
    });
  }

  async function waitForUdpPortClosed(port, timeoutMs) {
    const startedAt = Date.now();
    const pollIntervalMs = Math.max(50, getShutdownPollIntervalMs());

    while (Date.now() - startedAt <= Math.max(0, timeoutMs)) {
      if (await isUdpPortAvailable(port)) {
        return true;
      }

      await sleep(pollIntervalMs);
    }

    return false;
  }

  function forceKillProcessTree(pid) {
    return new Promise((resolve) => {
      if (!Number.isInteger(pid) || pid <= 0) {
        resolve({
          ok: false,
          error: "Invalid process ID.",
        });
        return;
      }

      execFile(
        "taskkill.exe",
        ["/PID", String(pid), "/T", "/F"],
        {
          timeout: getShutdownForceWaitMs(),
          windowsHide: true,
          maxBuffer: 1024 * 1024,
        },
        (error, stdout, stderr) => {
          resolve({
            ok: !error,
            stdout: String(stdout || ""),
            stderr: String(stderr || ""),
            error: error ? error.message : "",
          });
        },
      );
    });
  }

  function getUdpPortOwningProcesses(port) {
    return new Promise((resolve) => {
      if (!Number.isInteger(port) || port <= 0) {
        resolve([]);
        return;
      }

      const command = [
        "$ErrorActionPreference = 'Stop';",
        `$port = ${port};`,
        "$owners = @(Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue | ForEach-Object {",
        "  $processName = '';",
        "  $executablePath = '';",
        "  try {",
        "    $process = Get-Process -Id $_.OwningProcess -ErrorAction Stop;",
        "    $processName = $process.ProcessName;",
        "    $executablePath = $process.Path;",
        "  } catch {}",
        "  [PSCustomObject]@{ ProcessId = $_.OwningProcess; ProcessName = $processName; ExecutablePath = $executablePath }",
        "} | Where-Object { $_.ProcessId -gt 0 } | Sort-Object -Property ProcessId -Unique);",
        "if ($owners.Count -eq 0) { '[]' } else { $owners | ConvertTo-Json -Compress }",
      ].join(" ");

      execFile(
        "powershell.exe",
        ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command],
        {
          timeout: getShutdownForceWaitMs(),
          windowsHide: true,
          maxBuffer: 1024 * 1024,
        },
        (error, stdout) => {
          if (error) {
            resolve([]);
            return;
          }

          try {
            const output = String(stdout || "").trim();
            if (!output) {
              resolve([]);
              return;
            }

            const parsedOwners = JSON.parse(output);
            const owners = (Array.isArray(parsedOwners) ? parsedOwners : [parsedOwners])
              .map((owner) => ({
                processId: Number(owner.ProcessId),
                processName: String(owner.ProcessName || ""),
                executablePath: String(owner.ExecutablePath || ""),
              }))
              .filter((owner) => Number.isInteger(owner.processId) && owner.processId > 0);
            resolve(owners);
          } catch {
            resolve([]);
          }
        },
      );
    });
  }

  function isProjectPServerProcess(owner) {
    const processName = String(owner?.processName || "").toLowerCase();
    const executablePath = String(owner?.executablePath || "").toLowerCase();
    return processName === "projectpserver" || executablePath.endsWith("\\projectpserver.exe") || executablePath.endsWith("/projectpserver.exe");
  }

  async function forceKillUdpPortOwners(port) {
    const owners = await getUdpPortOwningProcesses(port);
    const killableOwners = owners.filter(isProjectPServerProcess);
    const result = {
      ok: killableOwners.length > 0,
      killedPids: [],
      skippedPids: owners
        .filter((owner) => !isProjectPServerProcess(owner))
        .map((owner) => owner.processId),
      errors: [],
    };

    for (const owner of killableOwners) {
      const killResult = await forceKillProcessTree(owner.processId);
      if (killResult.ok) {
        result.killedPids.push(owner.processId);
      } else {
        result.errors.push(`PID ${owner.processId}: ${killResult.error || "taskkill failed"}`);
      }
    }

    result.ok = result.killedPids.length > 0 && result.errors.length === 0;
    return result;
  }

  function buildSessionView(session) {
    return {
      dungeonSessionId: session.dungeonSessionId,
      address: session.address,
      host: session.host,
      port: session.port,
      pid: session.process.pid,
      partyId: session.partyId,
      status: session.status || "running",
      startedAt: session.startedAt,
      stoppingAt: session.stoppingAt || "",
      shutdownRetryCount: Number.isInteger(session.shutdownRetryCount) ? session.shutdownRetryCount : 0,
      nextShutdownRetryAt: session.nextShutdownRetryAt || "",
      lastShutdownError: session.lastShutdownError || "",
    };
  }

  async function findAvailablePort() {
    return findAvailableDungeonPort({
      portStart: getPortStart(),
      portEnd: getPortEnd(),
      sessions,
    });
  }

  function waitForStartup(session) {
    const startupWaitMs = Number.isInteger(managerConfig.startupWaitMs) ? managerConfig.startupWaitMs : 60000;
    const startupPollIntervalMs = Number.isInteger(managerConfig.startupPollIntervalMs) ? managerConfig.startupPollIntervalMs : 250;
    return new Promise((resolve, reject) => {
      let bResolved = false;
      const finish = (callback, value) => {
        if (bResolved) {
          return;
        }

        bResolved = true;
        session.process.off("exit", onExit);
        callback(value);
      };
      const onExit = (code, signal) => {
        sessions.delete(session.dungeonSessionId);
        finish(reject, new Error(`Dungeon server exited during startup. code=${code}, signal=${signal}`));
      };

      session.process.once("exit", onExit);
      waitForUdpPortBound({
        port: session.port,
        timeoutMs: startupWaitMs,
        pollIntervalMs: startupPollIntervalMs,
      }).then((bPortBound) => {
        if (!bPortBound) {
          finish(reject, new Error(`Dungeon server did not open UDP port ${session.port} within ${startupWaitMs}ms.`));
          return;
        }

        finish(resolve);
      }).catch((error) => {
        finish(reject, error);
      });
    });
  }

  function notifyDungeonSessionEnded(session, code, signal) {
    if (!session || session.bEndNotified) {
      return;
    }

    session.bEndNotified = true;
    postGameBackendJson("/api/dungeon/session-ended", {
      dungeonSessionId: session.dungeonSessionId,
      exitCode: Number.isInteger(code) ? code : null,
      signal: typeof signal === "string" ? signal : "",
    }).catch((error) => {
      console.error(`Failed to notify dungeon session ended. SessionId=${session.dungeonSessionId}, Error=${error.message}`);
    });
  }

  async function allocateDungeonServer(partyId) {
    const serverExecutable = getServerExecutablePath();
    if (!serverExecutable || !fs.existsSync(serverExecutable)) {
      throw new Error(`Dungeon server executable was not found: ${serverExecutable || "(empty)"}`);
    }

    const port = await findAvailablePort();
    if (port <= 0) {
      throw new Error("No available dungeon server port.");
    }

    const host = getPublicHost();
    const address = `${host}:${port}`;
    const dungeonSessionId = address;
    const mapName = typeof managerConfig.mapName === "string" && managerConfig.mapName.trim() ? managerConfig.mapName.trim() : "Map_Dungeon_1";
    const extraArgs = Array.isArray(managerConfig.extraArgs) ? managerConfig.extraArgs.filter((arg) => typeof arg === "string" && arg.trim()) : [];
    const args = [
      mapName,
      `-port=${port}`,
      `-DungeonSessionId=${dungeonSessionId}`,
      `-DungeonServerAddress=${address}`,
      ...extraArgs,
    ];

    const childProcess = spawn(serverExecutable, args, {
      cwd: projectRoot,
      detached: false,
      stdio: "ignore",
      windowsHide: true,
    });

    const session = {
      dungeonSessionId,
      address,
      host,
      port,
      partyId,
      process: childProcess,
      startedAt: new Date().toISOString(),
      status: "running",
      stoppingAt: "",
      shutdownRetryCount: 0,
      nextShutdownRetryAt: "",
      lastShutdownError: "",
      shutdownPromise: null,
      bEndNotified: false,
    };

    sessions.set(dungeonSessionId, session);
    addApiEvent("event", "info", `Dungeon server spawned ${port}`, {
      dungeonSessionId,
      port,
      partyId,
      pid: childProcess.pid,
      address,
    });
    childProcess.once("exit", (code, signal) => {
      const bWasStopping = session.status === "stopping";
      session.status = "exited";
      session.exitCode = Number.isInteger(code) ? code : null;
      session.signal = typeof signal === "string" ? signal : "";
      if (!bWasStopping) {
        sessions.delete(dungeonSessionId);
        notifyDungeonSessionEnded(session, code, signal);
      }
      addApiEvent("event", "warning", `Dungeon server exited ${port}`, {
        dungeonSessionId,
        port,
        partyId,
        pid: childProcess.pid,
        exitCode: Number.isInteger(code) ? code : null,
        signal: typeof signal === "string" ? signal : "",
      });
    });

    await waitForStartup(session);
    return session;
  }

  async function performShutdownDungeonServer(session) {
    const result = {
      bShutdown: true,
      bStopped: false,
      bExited: false,
      bForceKilled: false,
      bPortClosed: false,
      forceKillError: "",
      bPortOwnerKilled: false,
      portOwnerKilledPids: [],
      portOwnerSkippedPids: [],
      portOwnerKillError: "",
      message: "Dungeon server failed to stop.",
    };

    try {
      if (session.process.exitCode !== null || session.process.signalCode !== null) {
        result.bExited = true;
      } else {
        try {
          session.process.kill();
        } catch (error) {
          addApiEvent("event", "warning", `Dungeon server kill signal failed ${session.port}`, {
            dungeonSessionId: session.dungeonSessionId,
            port: session.port,
            partyId: session.partyId,
            pid: session.process.pid,
            error: error.message,
          });
        }

        result.bExited = await waitForProcessExit(session, getShutdownGraceMs());
        if (!result.bExited) {
          const forceKillResult = await forceKillProcessTree(session.process.pid);
          result.bForceKilled = forceKillResult.ok;
          result.forceKillError = forceKillResult.ok ? "" : forceKillResult.error;
          result.bExited = await waitForProcessExit(session, getShutdownForceWaitMs());
        }
      }

      result.bPortClosed = await waitForUdpPortClosed(session.port, getShutdownPortWaitMs());
      if (!result.bPortClosed) {
        const portKillResult = await forceKillUdpPortOwners(session.port);
        result.bPortOwnerKilled = portKillResult.killedPids.length > 0;
        result.portOwnerKilledPids = portKillResult.killedPids;
        result.portOwnerSkippedPids = portKillResult.skippedPids;
        result.portOwnerKillError = portKillResult.errors.join("; ");

        if (result.bPortOwnerKilled) {
          result.bForceKilled = true;
          addApiEvent("event", "warning", `Dungeon server UDP port owner killed ${session.port}`, {
            dungeonSessionId: session.dungeonSessionId,
            port: session.port,
            partyId: session.partyId,
            pid: session.process.pid,
            portOwnerKilledPids: result.portOwnerKilledPids,
            portOwnerSkippedPids: result.portOwnerSkippedPids,
          });
          result.bPortClosed = await waitForUdpPortClosed(session.port, getShutdownPortWaitMs());
        }
      }

      result.bStopped = result.bPortClosed && (result.bExited || result.bForceKilled || result.bPortOwnerKilled);

      if (result.bStopped) {
        session.status = "stopped";
        session.nextShutdownRetryAt = "";
        session.lastShutdownError = "";
        sessions.delete(session.dungeonSessionId);
        notifyDungeonSessionEnded(session, session.process.exitCode, session.process.signalCode);
        result.message = "Dungeon server stopped.";
        addApiEvent("event", "info", `Dungeon server stopped ${session.port}`, {
          dungeonSessionId: session.dungeonSessionId,
          port: session.port,
          partyId: session.partyId,
          pid: session.process.pid,
          bForceKilled: result.bForceKilled,
        });
      } else {
        session.status = "failed-to-stop";
        session.lastShutdownError = result.forceKillError || result.portOwnerKillError || `bExited=${result.bExited}, bPortClosed=${result.bPortClosed}`;
        if (getShutdownRetryMaxCount() > 0 && session.shutdownRetryCount < getShutdownRetryMaxCount()) {
          session.nextShutdownRetryAt = new Date(Date.now() + getShutdownRetryIntervalMs()).toISOString();
        } else {
          session.nextShutdownRetryAt = "";
        }
        addApiEvent("event", "error", `Dungeon server failed to stop ${session.port}`, {
          dungeonSessionId: session.dungeonSessionId,
          port: session.port,
          partyId: session.partyId,
          pid: session.process.pid,
          bExited: result.bExited,
          bPortClosed: result.bPortClosed,
          bForceKilled: result.bForceKilled,
          forceKillError: result.forceKillError,
          bPortOwnerKilled: result.bPortOwnerKilled,
          portOwnerKilledPids: result.portOwnerKilledPids,
          portOwnerSkippedPids: result.portOwnerSkippedPids,
          portOwnerKillError: result.portOwnerKillError,
        });
      }
    } finally {
      session.shutdownPromise = null;
    }

    return result;
  }

  async function shutdownDungeonServer(dungeonSessionId) {
    const session = sessions.get(dungeonSessionId);
    if (!session) {
      return {
        bShutdown: false,
        bStopped: false,
        bExited: false,
        bForceKilled: false,
        bPortClosed: false,
        forceKillError: "",
        message: "Dungeon server session was not found.",
      };
    }

    if (session.shutdownPromise) {
      return session.shutdownPromise;
    }

    session.status = "stopping";
    session.stoppingAt = new Date().toISOString();
    session.lastShutdownError = "";
    addApiEvent("event", "info", `Dungeon server shutdown requested ${session.port}`, {
      dungeonSessionId,
      port: session.port,
      partyId: session.partyId,
      pid: session.process.pid,
    });

    session.shutdownPromise = performShutdownDungeonServer(session);
    return session.shutdownPromise;
  }

  let bShutdownRetryInFlight = false;

  async function retryFailedShutdownSessions() {
    if (bShutdownRetryInFlight || getShutdownRetryMaxCount() <= 0) {
      return;
    }

    bShutdownRetryInFlight = true;
    try {
      const nowMs = Date.now();
      for (const session of sessions.values()) {
        if (session.status !== "failed-to-stop" || session.shutdownPromise) {
          continue;
        }

        const retryCount = Number.isInteger(session.shutdownRetryCount) ? session.shutdownRetryCount : 0;
        if (retryCount >= getShutdownRetryMaxCount()) {
          continue;
        }

        const nextRetryMs = Date.parse(session.nextShutdownRetryAt || "");
        if (!Number.isNaN(nextRetryMs) && nextRetryMs > nowMs) {
          continue;
        }

        session.status = "stopping";
        session.stoppingAt = new Date().toISOString();
        session.shutdownRetryCount = retryCount + 1;
        session.nextShutdownRetryAt = "";
        session.lastShutdownError = "";
        addApiEvent("event", "warning", `Retrying dungeon server shutdown ${session.port}`, {
          dungeonSessionId: session.dungeonSessionId,
          port: session.port,
          partyId: session.partyId,
          pid: session.process.pid,
          shutdownRetryCount: session.shutdownRetryCount,
        });

        session.shutdownPromise = performShutdownDungeonServer(session);
        await session.shutdownPromise;
      }
    } finally {
      bShutdownRetryInFlight = false;
    }
  }

  app.disable("x-powered-by");
  app.use(express.json({ limit: "16kb" }));
  app.use((error, req, res, next) => {
    if (error instanceof SyntaxError && error.status === 400 && "body" in error) {
      res.status(400).json({
        ok: false,
        message: "Invalid JSON body.",
      });
      return;
    }

    next(error);
  });
  app.use((req, res, next) => {
    if (!shouldRecordApiRequest(req)) {
      next();
      return;
    }

    const startedAt = Date.now();
    const requestPath = getRequestPath(req);
    const requestBody = getRequestBody(req);
    const dungeonSessionId = getDungeonSessionIdFromBody(requestBody);
    let responseBody;
    const originalJson = res.json.bind(res);

    res.json = (body) => {
      responseBody = body;
      return originalJson(body);
    };

    addApiEvent("receive", "info", `${req.method} ${requestPath} received`, {
      method: req.method,
      path: requestPath,
      peerServerId: "gameBackend",
      dungeonSessionId,
      query: req.query,
      requestBody,
      remoteAddress: req.ip,
    });

    res.on("finish", () => {
      addApiEvent("send", apiLevelFromStatus(res.statusCode), `${req.method} ${requestPath} -> HTTP ${res.statusCode}`, {
        method: req.method,
        path: requestPath,
        peerServerId: "gameBackend",
        dungeonSessionId,
        statusCode: res.statusCode,
        durationMs: Date.now() - startedAt,
        responseBody,
      });
    });

    next();
  });

  app.get("/api/health", (req, res) => {
    res.json({
      ok: true,
      message: "Dungeon manager is running.",
    });
  });

  app.get("/api/dungeon/sessions", (req, res) => {
    if (!hasValidServerAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    res.json({
      ok: true,
      sessions: [...sessions.values()].map(buildSessionView),
    });
  });

  app.get("/api/telemetry/events", (req, res) => {
    if (!hasValidServerAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    res.json({
      ok: true,
      generatedAt: new Date().toISOString(),
      serverId: "dungeonManager",
      events: apiEvents,
    });
  });

  app.post("/api/dungeon/allocate", async (req, res) => {
    if (!hasValidServerAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    const body = getRequestBody(req);
    const partyId = Number.parseInt(body.partyId, 10);
    if (!Number.isInteger(partyId) || partyId < 0) {
      res.status(400).json({
        ok: false,
        message: "Valid partyId is required.",
      });
      return;
    }

    try {
      const session = await allocateDungeonServer(partyId);
      res.json({
        ok: true,
        message: "Dungeon server allocated.",
        session: buildSessionView(session),
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Failed to allocate dungeon server.",
        error: error.message,
      });
    }
  });

  app.post("/api/dungeon/shutdown", async (req, res) => {
    if (!hasValidServerAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    const body = getRequestBody(req);
    const dungeonSessionId = typeof body.dungeonSessionId === "string" ? body.dungeonSessionId.trim() : "";
    if (!dungeonSessionId) {
      res.status(400).json({
        ok: false,
        message: "Dungeon session ID is required.",
      });
      return;
    }

    const shutdownResult = await shutdownDungeonServer(dungeonSessionId);
    const bOk = !shutdownResult.bShutdown || shutdownResult.bStopped;
    res.status(bOk ? 200 : 500).json({
      ok: bOk,
      ...shutdownResult,
    });
  });

  app.use((error, req, res, next) => {
    console.error(error);
    if (res.headersSent) {
      next(error);
      return;
    }

    res.status(500).json({
      ok: false,
      message: "Internal server error.",
    });
  });

  const shutdownRetryTimer = setInterval(() => {
    retryFailedShutdownSessions().catch((error) => {
      addApiEvent("event", "error", "Failed to retry dungeon server shutdown", {
        error: error.message,
      });
    });
  }, Math.max(1000, getShutdownRetryIntervalMs()));
  if (typeof shutdownRetryTimer.unref === "function") {
    shutdownRetryTimer.unref();
  }

  const host = typeof managerConfig.host === "string" && managerConfig.host.trim() ? managerConfig.host.trim() : "127.0.0.1";
  app.listen(getManagerPort(), host, () => {
    console.log(`Dungeon manager listening on ${host}:${getManagerPort()}`);
  });
}

bootstrap().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
