const express = require("express");
const { execFile } = require("child_process");
const http = require("http");
const https = require("https");
const path = require("path");

const { loadConfig } = require("./config");
const { createDatabase, testConnection } = require("./db");

const SENSITIVE_DB_FIELD_NAMES = new Set([
  "auth",
  "authkey",
  "key",
  "password",
  "passwordhash",
  "password_hash",
  "secret",
  "serverauthkey",
  "token",
  "tokenhash",
  "token_hash",
]);

function buildLocalBaseUrl(host, port) {
  const normalizedHost = !host || host === "0.0.0.0" || host === "::" ? "127.0.0.1" : host;
  return `http://${normalizedHost}:${port}`;
}

function trimBaseUrl(value) {
  return typeof value === "string" ? value.trim().replace(/\/+$/, "") : "";
}

function requestJson(requestUrl, headers = {}, timeoutMs = 1500) {
  return new Promise((resolve) => {
    const startedAt = Date.now();
    const url = new URL(requestUrl);
    const client = url.protocol === "https:" ? https : http;
    const request = client.request(
      url,
      {
        method: "GET",
        headers,
        timeout: timeoutMs,
      },
      (response) => {
        let responseBody = "";
        response.setEncoding("utf8");
        response.on("data", (chunk) => {
          responseBody += chunk;
        });
        response.on("end", () => {
          let json = null;
          try {
            json = responseBody ? JSON.parse(responseBody) : null;
          } catch (error) {
            json = null;
          }

          resolve({
            ok: response.statusCode >= 200 && response.statusCode < 300,
            statusCode: response.statusCode,
            latencyMs: Date.now() - startedAt,
            json,
          });
        });
      },
    );

    request.on("timeout", () => {
      request.destroy(new Error("Request timed out."));
    });
    request.on("error", (error) => {
      resolve({
        ok: false,
        statusCode: 0,
        latencyMs: Date.now() - startedAt,
        error: error.message,
      });
    });
    request.end();
  });
}

function execFileAsync(fileName, args, timeoutMs = 1500) {
  return new Promise((resolve) => {
    const startedAt = Date.now();
    execFile(
      fileName,
      args,
      {
        timeout: timeoutMs,
        windowsHide: true,
        maxBuffer: 1024 * 1024,
      },
      (error, stdout, stderr) => {
        const errorMessage = error ? error.message : String(stderr || "").trim();
        resolve({
          ok: !error,
          stdout: String(stdout || ""),
          stderr: String(stderr || ""),
          latencyMs: Date.now() - startedAt,
          error: errorMessage,
        });
      },
    );
  });
}

async function collectUdpEndpoints(startPort, endPort, timeoutMs = 1500) {
  const minPort = Math.min(startPort, endPort);
  const maxPort = Math.max(startPort, endPort);
  const command = [
    "$ErrorActionPreference = 'Stop';",
    `$startPort = ${minPort};`,
    `$endPort = ${maxPort};`,
    "Get-NetUDPEndpoint -ErrorAction Stop |",
    "Where-Object { $_.LocalPort -ge $startPort -and $_.LocalPort -le $endPort } |",
    "ForEach-Object {",
    "  $processName = '';",
    "  try { $processName = (Get-Process -Id $_.OwningProcess -ErrorAction Stop).ProcessName } catch {}",
    "  [pscustomobject]@{",
    "    LocalAddress = $_.LocalAddress;",
    "    LocalPort = $_.LocalPort;",
    "    OwningProcess = $_.OwningProcess;",
    "    ProcessName = $processName",
    "  }",
    "} | ConvertTo-Json -Compress",
  ].join(" ");

  const result = await execFileAsync(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command],
    timeoutMs,
  );

  if (!result.ok) {
    return {
      ok: false,
      endpoints: [],
      latencyMs: result.latencyMs,
      error: result.error || "UDP endpoint query failed.",
    };
  }

  const output = result.stdout.trim();
  if (!output) {
    return {
      ok: true,
      endpoints: [],
      latencyMs: result.latencyMs,
      error: "",
    };
  }

  try {
    const parsed = JSON.parse(output);
    return {
      ok: true,
      endpoints: Array.isArray(parsed) ? parsed : [parsed],
      latencyMs: result.latencyMs,
      error: "",
    };
  } catch (error) {
    return {
      ok: false,
      endpoints: [],
      latencyMs: result.latencyMs,
      error: error.message,
    };
  }
}

function buildUdpPortState(port, udpEndpoints) {
  const endpoints = udpEndpoints.endpoints.filter((endpoint) => Number(endpoint.LocalPort) === port);
  const endpointLabels = endpoints.map((endpoint) => {
    const address = endpoint.LocalAddress || "0.0.0.0";
    const processName = endpoint.ProcessName ? ` ${endpoint.ProcessName}` : "";
    return `${address}:${port}${processName}`;
  });

  if (endpoints.length > 0) {
    return {
      port,
      protocol: "udp",
      bOpen: true,
      latencyMs: udpEndpoints.latencyMs,
      endpoints: endpointLabels,
      error: "",
    };
  }

  return {
    port,
    protocol: "udp",
    bOpen: false,
    latencyMs: udpEndpoints.latencyMs,
    endpoints: [],
    error: udpEndpoints.error || "not listening",
  };
}

function normalizeInteger(value, defaultValue = 0) {
  const parsedValue = Number.parseInt(value, 10);
  return Number.isInteger(parsedValue) ? parsedValue : defaultValue;
}

function quoteIdentifier(value) {
  return `\`${String(value).replace(/`/g, "``")}\``;
}

function normalizeSensitiveDbKey(value) {
  return String(value || "").replace(/[^a-z0-9_]/gi, "").toLowerCase();
}

function isSensitiveDbField(value) {
  const normalizedKey = normalizeSensitiveDbKey(value);
  if (normalizedKey === "id" || normalizedKey.endsWith("_id") || normalizedKey.endsWith("index")) {
    return false;
  }

  if (SENSITIVE_DB_FIELD_NAMES.has(normalizedKey)) {
    return true;
  }

  return normalizedKey.includes("password")
    || normalizedKey.includes("token_hash")
    || normalizedKey.includes("tokenhash")
    || normalizedKey === "token"
    || normalizedKey.endsWith("_token")
    || normalizedKey.includes("secret")
    || normalizedKey.includes("auth")
    || normalizedKey.endsWith("key");
}

function normalizeDbValue(value) {
  if (value === null || value === undefined) {
    return value;
  }

  if (value instanceof Date) {
    return value.toISOString();
  }

  if (typeof value === "bigint") {
    return value.toString();
  }

  if (Buffer.isBuffer(value)) {
    return value.toString("hex");
  }

  return value;
}

function sanitizeDbRow(row) {
  const sanitizedRow = {};
  for (const [key, value] of Object.entries(row)) {
    sanitizedRow[key] = isSensitiveDbField(key) ? "[masked]" : normalizeDbValue(value);
  }

  return sanitizedRow;
}

function getEventTimeValue(event) {
  const timeValue = Date.parse(event?.time);
  return Number.isNaN(timeValue) ? 0 : timeValue;
}

function getEventDetail(event) {
  return event && event.detail && typeof event.detail === "object" ? event.detail : {};
}

function getEventPath(event) {
  const detail = getEventDetail(event);
  return typeof detail.path === "string" ? detail.path : "";
}

function eventDetailIncludes(event, value) {
  if (!value) {
    return false;
  }

  const detail = getEventDetail(event);
  if (detail.dungeonSessionId === value) {
    return true;
  }

  try {
    return JSON.stringify(detail).includes(value);
  } catch (error) {
    return false;
  }
}

function isClientApiEvent(event) {
  const eventPath = getEventPath(event);
  return eventPath === "/api/login"
    || eventPath === "/api/logout"
    || eventPath === "/api/register"
    || eventPath.startsWith("/api/session/");
}

function isLobbyApiEvent(event) {
  const detail = getEventDetail(event);
  const eventPath = getEventPath(event);
  return detail.peerServerId === "lobby"
    || eventPath === "/api/telemetry/lobby"
    || eventPath === "/api/dungeon/allocate";
}

function mergeDungeonSessions(managerSessions, telemetrySessions) {
  const sessionsById = new Map();

  for (const telemetrySession of telemetrySessions) {
    const dungeonSessionId = typeof telemetrySession.dungeonSessionId === "string" ? telemetrySession.dungeonSessionId.trim() : "";
    if (!dungeonSessionId) {
      continue;
    }

    sessionsById.set(dungeonSessionId, {
      ...telemetrySession,
      processStatus: "unknown",
    });
  }

  for (const managerSession of managerSessions) {
    const dungeonSessionId = typeof managerSession.dungeonSessionId === "string" ? managerSession.dungeonSessionId.trim() : "";
    if (!dungeonSessionId) {
      continue;
    }

    const telemetrySession = sessionsById.get(dungeonSessionId) ?? {};
    sessionsById.set(dungeonSessionId, {
      ...managerSession,
      ...telemetrySession,
      address: telemetrySession.address || managerSession.address,
      host: telemetrySession.host || managerSession.host,
      port: normalizeInteger(telemetrySession.port, normalizeInteger(managerSession.port)),
      pid: normalizeInteger(telemetrySession.pid, normalizeInteger(managerSession.pid)),
      startedAt: telemetrySession.startedAt || managerSession.startedAt,
      processStatus: "running",
    });
  }

  return [...sessionsById.values()]
    .map((session) => {
      const members = Array.isArray(session.members) ? session.members : [];
      const inGameUserCount = normalizeInteger(
        session.inGameUserCount,
        members.filter((member) => member.connectionState === "InGame").length,
      );
      const outGameUserCount = normalizeInteger(
        session.outGameUserCount,
        members.filter((member) => member.connectionState === "OutGame").length,
      );

      return {
        ...session,
        members,
        currentClientCount: normalizeInteger(session.currentClientCount, inGameUserCount),
        inGameUserCount,
        outGameUserCount,
      };
    })
    .sort((left, right) => normalizeInteger(left.port) - normalizeInteger(right.port));
}

async function bootstrap() {
  const config = loadConfig();
  const monitorConfig = config.monitor ?? {};
  const serverConfig = config.server ?? {};
  const managerConfig = config.dungeonManager ?? {};
  const dbPool = createDatabase(config.database);
  const app = express();
  const publicRoot = path.join(__dirname, "Monitor", "public");
  const dungeonStateKey = typeof config.serverAuth?.dungeonStateKey === "string" ? config.serverAuth.dungeonStateKey.trim() : "";
  const gameBackendBaseUrl = trimBaseUrl(monitorConfig.gameBackendBaseUrl)
    || trimBaseUrl(managerConfig.gameBackendBaseUrl)
    || buildLocalBaseUrl(serverConfig.host, Number.isInteger(serverConfig.port) ? serverConfig.port : 8080);
  const dungeonManagerBaseUrl = trimBaseUrl(monitorConfig.dungeonManagerBaseUrl)
    || trimBaseUrl(managerConfig.baseUrl)
    || buildLocalBaseUrl(managerConfig.host, Number.isInteger(managerConfig.port) ? managerConfig.port : 8090);
  const lobbyHost = typeof monitorConfig.lobbyHost === "string" && monitorConfig.lobbyHost.trim()
    ? monitorConfig.lobbyHost.trim()
    : "127.0.0.1";
  const lobbyPort = Number.isInteger(monitorConfig.lobbyPort) ? monitorConfig.lobbyPort : 7777;
  const dungeonPortStart = Number.isInteger(managerConfig.portStart) ? managerConfig.portStart : 7780;
  const dungeonPortEnd = Number.isInteger(managerConfig.portEnd) ? managerConfig.portEnd : 7799;
  const pollIntervalMs = Number.isInteger(monitorConfig.pollIntervalMs) ? Math.max(1000, monitorConfig.pollIntervalMs) : 3000;
  const events = [];
  const maxEvents = Number.isInteger(monitorConfig.maxEvents) ? Math.max(20, monitorConfig.maxEvents) : 200;
  const maxServerEvents = Number.isInteger(monitorConfig.maxServerEvents) ? Math.max(20, monitorConfig.maxServerEvents) : 300;
  const sourceEventCaches = new Map();
  let serverEventLogs = new Map();
  let snapshot = buildEmptySnapshot();
  let previousStateKeys = new Map();

  function addEvent(type, level, message, detail = {}) {
    events.unshift({
      id: `${Date.now()}-${events.length}`,
      time: new Date().toISOString(),
      type,
      level,
      message,
      detail,
    });

    if (events.length > maxEvents) {
      events.length = maxEvents;
    }
  }

  function updateStateEvent(key, bHealthy, message, detail = {}) {
    const previous = previousStateKeys.get(key);
    if (previous === bHealthy) {
      return;
    }

    previousStateKeys.set(key, bHealthy);
    addEvent(key, bHealthy ? "info" : "warning", message, detail);
  }

  function sortAndLimitServerEvents(sourceEvents) {
    return [...sourceEvents]
      .sort((left, right) => getEventTimeValue(right) - getEventTimeValue(left))
      .slice(0, maxServerEvents);
  }

  function readSourceEvents(response, serverId) {
    if (response.ok && Array.isArray(response.json?.events)) {
      sourceEventCaches.set(serverId, response.json.events);
    }

    return sourceEventCaches.get(serverId) ?? [];
  }

  function rebuildServerEventLogs(gameBackendEvents, dungeonManagerEvents, dungeonSessions) {
    const logs = new Map();
    logs.set("gameBackend", sortAndLimitServerEvents(gameBackendEvents));
    logs.set("dungeonManager", sortAndLimitServerEvents(dungeonManagerEvents));
    logs.set("client", sortAndLimitServerEvents(gameBackendEvents.filter(isClientApiEvent)));
    logs.set("lobby", sortAndLimitServerEvents(gameBackendEvents.filter(isLobbyApiEvent)));

    for (const session of dungeonSessions) {
      const port = normalizeInteger(session.port);
      const dungeonSessionId = typeof session.dungeonSessionId === "string" ? session.dungeonSessionId.trim() : "";
      if (port <= 0 || !dungeonSessionId) {
        continue;
      }

      logs.set(
        `dungeon-${port}`,
        sortAndLimitServerEvents([
          ...gameBackendEvents.filter((event) => eventDetailIncludes(event, dungeonSessionId)),
          ...dungeonManagerEvents.filter((event) => eventDetailIncludes(event, dungeonSessionId)),
        ]),
      );
    }

    serverEventLogs = logs;
  }

  function buildEmptySnapshot() {
    return {
      generatedAt: new Date().toISOString(),
      config: {
        gameBackendBaseUrl,
        dungeonManagerBaseUrl,
        lobbyHost,
        lobbyPort,
        dungeonPortStart,
        dungeonPortEnd,
        pollIntervalMs,
      },
      nodes: [],
      edges: [],
      services: [],
      ports: [],
      dungeons: [],
      login: {},
      lobby: {},
      summary: {
        healthyServices: 0,
        totalServices: 3,
        activeDungeonSessions: 0,
        openDungeonPorts: 0,
        lobbyClients: 0,
        dungeonClients: 0,
        dungeonOutGameUsers: 0,
        loginRequests: 0,
      },
    };
  }

  async function collectDatabaseStatus(selectedTableName, limit) {
    const safeLimit = Number.isInteger(limit) ? Math.max(1, Math.min(limit, 100)) : 30;
    const databaseName = typeof config.database?.database === "string" ? config.database.database : "";
    await testConnection(dbPool);

    const [tableRows] = await dbPool.query(
      `
        SELECT
          TABLE_NAME AS tableName,
          TABLE_COMMENT AS tableComment,
          CREATE_TIME AS createdAt,
          UPDATE_TIME AS updatedAt
        FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = ?
        ORDER BY TABLE_NAME ASC
      `,
      [databaseName],
    );
    const tableNames = tableRows.map((table) => table.tableName);
    const requestedTableName = typeof selectedTableName === "string" ? selectedTableName.trim() : "";
    const selectedTable = tableNames.includes(requestedTableName)
      ? requestedTableName
      : (tableNames.includes("users") ? "users" : tableNames[0] ?? "");

    const [columnRows] = await dbPool.query(
      `
        SELECT
          TABLE_NAME AS tableName,
          COLUMN_NAME AS columnName,
          DATA_TYPE AS dataType,
          COLUMN_KEY AS columnKey,
          IS_NULLABLE AS isNullable
        FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = ?
        ORDER BY TABLE_NAME ASC, ORDINAL_POSITION ASC
      `,
      [databaseName],
    );
    const columnsByTable = new Map();
    for (const column of columnRows) {
      const tableColumns = columnsByTable.get(column.tableName) ?? [];
      tableColumns.push({
        tableName: column.tableName,
        columnName: column.columnName,
        dataType: column.dataType,
        columnKey: column.columnKey,
        isNullable: column.isNullable,
        bMasked: isSensitiveDbField(column.columnName),
      });
      columnsByTable.set(column.tableName, tableColumns);
    }

    const tables = [];
    for (const table of tableRows) {
      const [countRows] = await dbPool.query(`SELECT COUNT(*) AS rowCount FROM ${quoteIdentifier(table.tableName)}`);
      tables.push({
        tableName: table.tableName,
        rowCount: normalizeInteger(countRows[0]?.rowCount),
        columnCount: (columnsByTable.get(table.tableName) ?? []).length,
        createdAt: normalizeDbValue(table.createdAt),
        updatedAt: normalizeDbValue(table.updatedAt),
        tableComment: table.tableComment,
      });
    }

    const selectedColumns = columnsByTable.get(selectedTable) ?? [];
    let rows = [];
    if (selectedTable) {
      const primaryColumn = selectedColumns.find((column) => column.columnKey === "PRI")?.columnName;
      const orderClause = primaryColumn ? ` ORDER BY ${quoteIdentifier(primaryColumn)} DESC` : "";
      const [selectedRows] = await dbPool.query(
        `SELECT * FROM ${quoteIdentifier(selectedTable)}${orderClause} LIMIT ?`,
        [safeLimit],
      );
      rows = selectedRows.map(sanitizeDbRow);
    }

    return {
      ok: true,
      generatedAt: new Date().toISOString(),
      database: {
        name: databaseName,
        host: config.database?.host ?? "",
        port: config.database?.port ?? "",
      },
      summary: {
        tableCount: tables.length,
        totalRows: tables.reduce((sum, table) => sum + normalizeInteger(table.rowCount), 0),
        selectedTable,
        selectedRowCount: tables.find((table) => table.tableName === selectedTable)?.rowCount ?? 0,
      },
      tables,
      selectedTable,
      columns: selectedColumns,
      rows,
      rowLimit: safeLimit,
    };
  }

  function buildTopology(services, dungeonSessions, lobbyTelemetry) {
    const lobbyClientCount = normalizeInteger(lobbyTelemetry?.currentClientCount);
    const nodes = [
      { id: "client", label: "Client", kind: "external", status: "neutral" },
      { id: "gameBackend", label: "GameBackend", kind: "service", status: services[0]?.status ?? "down" },
      { id: "dungeonManager", label: "DungeonManager", kind: "service", status: services[1]?.status ?? "down" },
      { id: "lobby", label: `Lobby (${lobbyClientCount})`, kind: "service", status: services[2]?.status ?? "down" },
    ];

    for (const session of dungeonSessions) {
      nodes.push({
        id: `dungeon-${session.port}`,
        label: `Dungeon:${session.port} (${session.currentClientCount})`,
        kind: "dungeon",
        status: "up",
        sessionId: session.dungeonSessionId,
      });
    }

    const edges = [
      { from: "client", to: "gameBackend", label: "login / session verify" },
      { from: "client", to: "lobby", label: "Unreal connection" },
      { from: "lobby", to: "gameBackend", label: "allocate / shutdown" },
      { from: "gameBackend", to: "dungeonManager", label: "spawn / sessions" },
      { from: "dungeonManager", to: "gameBackend", label: "session-ended" },
    ];

    for (const session of dungeonSessions) {
      edges.push({ from: "dungeonManager", to: `dungeon-${session.port}`, label: `pid ${session.pid}` });
      edges.push({ from: `dungeon-${session.port}`, to: "gameBackend", label: "member-state" });
      edges.push({ from: "client", to: `dungeon-${session.port}`, label: "ClientTravel" });
    }

    return { nodes, edges };
  }

  function buildCurrentLobbyTelemetry(rawLobbyTelemetry, lobbyPortState) {
    const bLobbyOpen = lobbyPortState?.bOpen === true;
    const currentClientCount = bLobbyOpen ? normalizeInteger(rawLobbyTelemetry?.currentClientCount) : 0;
    const connectedUsers = bLobbyOpen && currentClientCount > 0 && Array.isArray(rawLobbyTelemetry?.connectedUsers)
      ? rawLobbyTelemetry.connectedUsers.slice(0, currentClientCount)
      : [];

    return {
      ...(rawLobbyTelemetry ?? {}),
      currentClientCount,
      connectedUserCount: connectedUsers.length,
      connectedUsers,
    };
  }

  async function collectSnapshot() {
    const [
      gameBackendHealth,
      dungeonManagerHealth,
      udpEndpoints,
      dungeonSessionsResponse,
      gameBackendTelemetryResponse,
      gameBackendEventsResponse,
      dungeonManagerEventsResponse,
    ] = await Promise.all([
      requestJson(`${gameBackendBaseUrl}/api/health`),
      requestJson(`${dungeonManagerBaseUrl}/api/health`),
      collectUdpEndpoints(Math.min(lobbyPort, dungeonPortStart), Math.max(lobbyPort, dungeonPortEnd)),
      requestJson(`${dungeonManagerBaseUrl}/api/dungeon/sessions`, { "X-Server-Auth": dungeonStateKey }),
      requestJson(`${gameBackendBaseUrl}/api/telemetry/status`, { "X-Server-Auth": dungeonStateKey }),
      requestJson(`${gameBackendBaseUrl}/api/telemetry/events`, { "X-Server-Auth": dungeonStateKey }),
      requestJson(`${dungeonManagerBaseUrl}/api/telemetry/events`, { "X-Server-Auth": dungeonStateKey }),
    ]);

    const lobbyPortState = buildUdpPortState(lobbyPort, udpEndpoints);
    const managerDungeonSessions = Array.isArray(dungeonSessionsResponse.json?.sessions)
      ? dungeonSessionsResponse.json.sessions
      : [];
    const gameBackendTelemetry = gameBackendTelemetryResponse.ok ? gameBackendTelemetryResponse.json : null;
    const telemetryDungeonSessions = Array.isArray(gameBackendTelemetry?.dungeon?.sessions)
      ? gameBackendTelemetry.dungeon.sessions
      : [];
    const dungeonSessions = mergeDungeonSessions(managerDungeonSessions, telemetryDungeonSessions);
    rebuildServerEventLogs(
      readSourceEvents(gameBackendEventsResponse, "gameBackend"),
      readSourceEvents(dungeonManagerEventsResponse, "dungeonManager"),
      dungeonSessions,
    );
    const loginTelemetry = gameBackendTelemetry?.login ?? {};
    const lobbyTelemetry = buildCurrentLobbyTelemetry(gameBackendTelemetry?.lobby ?? {}, lobbyPortState);
    const dungeonPorts = [];
    for (let port = dungeonPortStart; port <= dungeonPortEnd; ++port) {
      dungeonPorts.push(buildUdpPortState(port, udpEndpoints));
    }
    const portStates = dungeonPorts;

    const services = [
      {
        id: "gameBackend",
        name: "GameBackend",
        target: gameBackendBaseUrl,
        status: gameBackendHealth.ok ? "up" : "down",
        latencyMs: gameBackendHealth.latencyMs,
        detail: gameBackendTelemetryResponse.ok
          ? `Login ${normalizeInteger(loginTelemetry.totalRequests)} req / ${normalizeInteger(loginTelemetry.successfulRequests)} ok / ${normalizeInteger(loginTelemetry.failedRequests)} fail`
          : gameBackendHealth.error || gameBackendHealth.json?.message || `HTTP ${gameBackendHealth.statusCode}`,
      },
      {
        id: "dungeonManager",
        name: "DungeonManager",
        target: dungeonManagerBaseUrl,
        status: dungeonManagerHealth.ok ? "up" : "down",
        latencyMs: dungeonManagerHealth.latencyMs,
        detail: dungeonManagerHealth.error || dungeonManagerHealth.json?.message || `HTTP ${dungeonManagerHealth.statusCode}`,
      },
      {
        id: "lobby",
        name: "LobbyServer",
        target: `${lobbyHost}:${lobbyPort}`,
        status: lobbyPortState.bOpen ? "up" : "down",
        latencyMs: lobbyPortState.latencyMs,
        detail: lobbyPortState.bOpen
          ? `${normalizeInteger(lobbyTelemetry.currentClientCount)} clients / UDP endpoint open`
          : lobbyPortState.error,
      },
    ];

    const { nodes, edges } = buildTopology(services, dungeonSessions, lobbyTelemetry);
    const openDungeonPorts = portStates.filter((portState) => portState.bOpen).length;
    const dungeonClientCount = dungeonSessions.reduce((sum, session) => sum + normalizeInteger(session.currentClientCount), 0);
    const dungeonOutGameUserCount = dungeonSessions.reduce((sum, session) => sum + normalizeInteger(session.outGameUserCount), 0);
    snapshot = {
      generatedAt: new Date().toISOString(),
      config: {
        gameBackendBaseUrl,
        dungeonManagerBaseUrl,
        lobbyHost,
        lobbyPort,
        dungeonPortStart,
        dungeonPortEnd,
        pollIntervalMs,
      },
      nodes,
      edges,
      services,
      ports: portStates,
      dungeons: dungeonSessions,
      login: loginTelemetry,
      lobby: lobbyTelemetry,
      summary: {
        healthyServices: services.filter((service) => service.status === "up").length,
        totalServices: services.length,
        activeDungeonSessions: dungeonSessions.length,
        openDungeonPorts,
        lobbyClients: normalizeInteger(lobbyTelemetry.currentClientCount),
        dungeonClients: dungeonClientCount,
        dungeonOutGameUsers: dungeonOutGameUserCount,
        loginRequests: normalizeInteger(loginTelemetry.totalRequests),
      },
    };

    updateStateEvent("gameBackend", gameBackendHealth.ok, `GameBackend ${gameBackendHealth.ok ? "OK" : "DOWN"}`, services[0]);
    updateStateEvent("dungeonManager", dungeonManagerHealth.ok, `DungeonManager ${dungeonManagerHealth.ok ? "OK" : "DOWN"}`, services[1]);
    updateStateEvent("lobby", lobbyPortState.bOpen, `LobbyServer ${lobbyPortState.bOpen ? "OK" : "DOWN"}`, services[2]);

    const activeSessionKeys = new Set(dungeonSessions.map((session) => session.dungeonSessionId));
    for (const session of dungeonSessions) {
      updateStateEvent(`session:${session.dungeonSessionId}`, true, `Dungeon session active ${session.port}`, session);
    }

    for (const key of [...previousStateKeys.keys()]) {
      if (key.startsWith("session:") && !activeSessionKeys.has(key.slice("session:".length))) {
        previousStateKeys.delete(key);
        addEvent("session", "info", `Dungeon session ended ${key.slice("session:".length)}`);
      }
    }
  }

  app.disable("x-powered-by");
  app.use(express.static(publicRoot));

  app.get("/api/monitor/status", (req, res) => {
    res.json(snapshot);
  });

  app.get("/api/monitor/dungeons", (req, res) => {
    res.json({
      generatedAt: snapshot.generatedAt,
      dungeons: snapshot.dungeons,
    });
  });

  app.get("/api/monitor/ports", (req, res) => {
    res.json({
      generatedAt: snapshot.generatedAt,
      ports: snapshot.ports,
    });
  });

  app.get("/api/monitor/events", (req, res) => {
    res.json({
      generatedAt: snapshot.generatedAt,
      events,
    });
  });

  app.get("/api/monitor/lobby/users", (req, res) => {
    const lobby = snapshot.lobby && typeof snapshot.lobby === "object" ? snapshot.lobby : {};
    const connectedUsers = Array.isArray(lobby.connectedUsers) ? lobby.connectedUsers : [];
    res.json({
      generatedAt: snapshot.generatedAt,
      count: connectedUsers.length,
      users: connectedUsers,
    });
  });

  app.get("/api/monitor/db", async (req, res) => {
    const parsedLimit = Number.parseInt(req.query.limit, 10);
    const limit = Number.isNaN(parsedLimit) ? 30 : parsedLimit;
    const tableName = typeof req.query.table === "string" ? req.query.table : "";
    try {
      res.json(await collectDatabaseStatus(tableName, limit));
    } catch (error) {
      res.status(500).json({
        ok: false,
        generatedAt: new Date().toISOString(),
        message: "Database status query failed.",
        error: error.message,
      });
    }
  });

  app.get("/api/monitor/server-logs/:serverId", (req, res) => {
    const serverId = typeof req.params.serverId === "string" ? req.params.serverId.trim() : "";
    res.json({
      generatedAt: snapshot.generatedAt,
      serverId,
      logs: serverEventLogs.get(serverId) ?? [],
    });
  });

  await collectSnapshot();
  setInterval(() => {
    collectSnapshot().catch((error) => {
      addEvent("monitor", "error", "Monitor collection failed.", { error: error.message });
    });
  }, pollIntervalMs);

  const monitorHost = typeof monitorConfig.host === "string" && monitorConfig.host.trim() ? monitorConfig.host.trim() : "127.0.0.1";
  const monitorPort = Number.isInteger(monitorConfig.port) ? monitorConfig.port : 9000;
  app.listen(monitorPort, monitorHost, () => {
    console.log(`Server monitor listening on http://${monitorHost}:${monitorPort}`);
  });
}

bootstrap().catch((error) => {
  console.error("Failed to start server monitor.");
  console.error(error);
  process.exit(1);
});
