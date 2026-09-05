const express = require("express");
const http = require("http");
const https = require("https");
const swaggerUi = require("swagger-ui-express");

const LOBBY_TELEMETRY_STALE_MS = 10000;

const { loadConfig } = require("./config");
const {
  getGauntletTestAccounts,
  hasValidGauntletTestAuth,
  isGauntletTestAccountId,
  resetGauntletRuntimeState,
} = require("./gauntlet-test-support");
const { swaggerSpec } = require("./swagger");
const {
  authenticateUser,
  createLoginToken,
  createUser,
  createDatabase,
  fetchUserByIndex,
  fetchUserByID,
  fetchUsers,
  hasActiveLoginToken,
  revokeActiveLoginTokensForUser,
  revokeLoginToken,
  testConnection,
  touchLoginToken,
  verifyLoginToken,
} = require("./db");

async function bootstrap() {
  const config = loadConfig();
  const app = express();
  const pool = createDatabase(config.database);
  const dungeonMemberStates = new Map();
  const dungeonSessions = new Map();
  const initialDungeonJoinTimers = new Map();
  const loginTelemetry = {
    totalRequests: 0,
    successfulRequests: 0,
    failedRequests: 0,
    lastRequestAt: "",
    lastSuccessAt: "",
    lastFailureAt: "",
    lastFailureMessage: "",
  };
  const lobbyTelemetry = {
    currentClientCount: 0,
    totalConnected: 0,
    totalDisconnected: 0,
    lastReportedAt: "",
    updatedAt: "",
    connectedUsers: [],
  };
  const dungeonStateKey = typeof config.serverAuth?.dungeonStateKey === "string" ? config.serverAuth.dungeonStateKey.trim() : "";
  const dungeonManagerBaseUrl = typeof config.dungeonManager?.baseUrl === "string" ? config.dungeonManager.baseUrl.trim().replace(/\/+$/, "") : "";
  const apiEvents = [];
  const gauntletTestAccounts = getGauntletTestAccounts();
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

  function guessPeerServerId(requestPath, body) {
    if (requestPath === "/api/telemetry/lobby" || requestPath === "/api/dungeon/allocate") {
      return "lobby";
    }

    if (requestPath === "/api/dungeon/session-ended") {
      return "dungeonManager";
    }

    if (requestPath === "/api/dungeon/member-state" || getDungeonSessionIdFromBody(body)) {
      return "dungeon";
    }

    if (requestPath === "/api/login" || requestPath === "/api/logout" || requestPath === "/api/register" || requestPath.startsWith("/api/session/")) {
      return "client";
    }

    return "";
  }

  function shouldRecordApiRequest(req) {
    const requestPath = getRequestPath(req);
    if (!requestPath.startsWith("/api/")) {
      return false;
    }

    return !new Set([
      "/api/health",
      "/api/health/db",
      "/api/telemetry/status",
      "/api/telemetry/events",
      "/api/telemetry/lobby",
      "/api/telemetry/lobby/users",
    ]).has(requestPath);
  }

  function addApiEvent(direction, level, message, detail = {}) {
    apiEvents.unshift({
      id: `${Date.now()}-${apiEvents.length}`,
      serverId: "gameBackend",
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

  function hasValidDungeonStateAuth(req) {
    const requestKey = typeof req.get("X-Server-Auth") === "string" ? req.get("X-Server-Auth").trim() : "";
    return !!dungeonStateKey && requestKey === dungeonStateKey;
  }

  function rejectInvalidGauntletTestAuth(req, res) {
    if (hasValidGauntletTestAuth(req)) {
      return false;
    }

    res.status(401).json({
      ok: false,
      message: "Gauntlet test auth is invalid.",
    });
    return true;
  }

  function getRequestBody(req) {
    return req.body && typeof req.body === "object" ? req.body : {};
  }

  function getInitialJoinWaitMs() {
    const initialJoinWaitMs = Number.parseInt(config.dungeonManager?.initialJoinWaitMs, 10);
    return Number.isInteger(initialJoinWaitMs) && initialJoinWaitMs >= 0 ? initialJoinWaitMs : 180000;
  }

  async function fetchGauntletTestUsers() {
    const users = [];
    for (const account of gauntletTestAccounts) {
      const user = await fetchUserByID(pool, account.id);
      if (!user) {
        return {
          ok: false,
          statusCode: 404,
          message: `Gauntlet test account was not found: ${account.id}`,
        };
      }

      if (user.status !== 1 || user.username !== account.username) {
        return {
          ok: false,
          statusCode: 409,
          message: `Gauntlet test account does not match expected data: ${account.id}`,
        };
      }

      users.push({
        ...user,
        clientIndex: account.clientIndex,
        failureCase: account.failureCase,
      });
    }

    return {
      ok: true,
      users,
    };
  }

  function normalizeUserIndexes(values) {
    if (!Array.isArray(values)) {
      return [];
    }

    return [...new Set(values
      .map((value) => Number.parseInt(value, 10))
      .filter((value) => Number.isInteger(value) && value > 0))];
  }

  function isValidCharacterId(value) {
    return value === 100 || value === 200 || value === 300;
  }

  function normalizeDungeonMembers(values) {
    if (!Array.isArray(values)) {
      return [];
    }

    const seenUserIndexes = new Set();
    const members = [];
    for (const value of values) {
      if (!value || typeof value !== "object") {
        continue;
      }

      const userIndex = Number.parseInt(value.userIndex ?? value.user_Index, 10);
      const characterId = Number.parseInt(value.characterId ?? value.selectedCharacterId, 10);
      if (!Number.isInteger(userIndex) || userIndex <= 0 || !isValidCharacterId(characterId) || seenUserIndexes.has(userIndex)) {
        continue;
      }

      seenUserIndexes.add(userIndex);
      members.push({
        userIndex,
        characterId,
      });
    }

    return members;
  }

  function postDungeonManagerJson(endpointPath, body) {
    return new Promise((resolve, reject) => {
      if (!dungeonManagerBaseUrl) {
        reject(new Error("DungeonManager baseUrl is not configured."));
        return;
      }

      const requestUrl = new URL(endpointPath, `${dungeonManagerBaseUrl}/`);
      const requestBody = JSON.stringify(body ?? {});
      const client = requestUrl.protocol === "https:" ? https : http;
      const startedAt = Date.now();
      addApiEvent("send", "info", `POST ${endpointPath} -> DungeonManager`, {
        method: "POST",
        path: endpointPath,
        targetServerId: "dungeonManager",
        target: requestUrl.origin,
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
          let responseBody = "";
          response.setEncoding("utf8");
          response.on("data", (chunk) => {
            responseBody += chunk;
          });
          response.on("end", () => {
            let json = {};
            if (responseBody) {
              try {
                json = JSON.parse(responseBody);
              } catch (error) {
                addApiEvent("receive", "error", `DungeonManager ${endpointPath} invalid JSON`, {
                  method: "POST",
                  path: endpointPath,
                  sourceServerId: "dungeonManager",
                  statusCode: response.statusCode,
                  durationMs: Date.now() - startedAt,
                });
                reject(new Error(`Invalid DungeonManager response JSON. HTTP ${response.statusCode}`));
                return;
              }
            }

            addApiEvent("receive", apiLevelFromStatus(response.statusCode), `DungeonManager ${endpointPath} -> HTTP ${response.statusCode}`, {
              method: "POST",
              path: endpointPath,
              sourceServerId: "dungeonManager",
              statusCode: response.statusCode,
              durationMs: Date.now() - startedAt,
              responseBody: json,
            });

            resolve({
              statusCode: response.statusCode,
              json,
            });
          });
        },
      );

      request.on("error", (error) => {
        addApiEvent("receive", "error", `DungeonManager ${endpointPath} request failed`, {
          method: "POST",
          path: endpointPath,
          sourceServerId: "dungeonManager",
          durationMs: Date.now() - startedAt,
          error: error.message,
        });
        reject(error);
      });
      request.write(requestBody);
      request.end();
    });
  }

  function hasDungeonSessionInGameUser(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return false;
    }

    const dungeonSession = dungeonSessions.get(normalizedSessionId);
    if (Array.isArray(dungeonSession?.inGameUserIndexes) && dungeonSession.inGameUserIndexes.length > 0) {
      return true;
    }

    for (const state of dungeonMemberStates.values()) {
      if (state.dungeonSessionId === normalizedSessionId && state.connectionState === "InGame") {
        return true;
      }
    }

    return false;
  }

  function clearInitialDungeonJoinTimeout(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return false;
    }

    const timer = initialDungeonJoinTimers.get(normalizedSessionId);
    if (!timer) {
      return false;
    }

    clearTimeout(timer);
    initialDungeonJoinTimers.delete(normalizedSessionId);
    return true;
  }

  async function shutdownDungeonSessionAfterInitialJoinTimeout(dungeonSessionId) {
    initialDungeonJoinTimers.delete(dungeonSessionId);

    const dungeonSession = dungeonSessions.get(dungeonSessionId);
    if (!dungeonSession || dungeonSession.isJoinable === false || hasDungeonSessionInGameUser(dungeonSessionId)) {
      return;
    }

    markDungeonSessionNotJoinable(dungeonSessionId);
    addApiEvent("event", "warning", "Dungeon initial join wait expired", {
      dungeonSessionId,
      waitMs: getInitialJoinWaitMs(),
    });

    try {
      const managerResponse = await postDungeonManagerJson("/api/dungeon/shutdown", {
        dungeonSessionId,
      });

      if (managerResponse.statusCode < 200 || managerResponse.statusCode >= 300 || !managerResponse.json.ok) {
        addApiEvent("event", "error", "Dungeon initial join timeout shutdown failed", {
          dungeonSessionId,
          statusCode: managerResponse.statusCode,
          responseBody: managerResponse.json,
        });
        return;
      }

      cleanupDungeonSessionStates(dungeonSessionId);
    } catch (error) {
      addApiEvent("event", "error", "Dungeon initial join timeout shutdown request failed", {
        dungeonSessionId,
        error: error.message,
      });
    }
  }

  function scheduleInitialDungeonJoinTimeout(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return;
    }

    clearInitialDungeonJoinTimeout(normalizedSessionId);

    const waitMs = getInitialJoinWaitMs();
    const deadlineAt = new Date(Date.now() + waitMs).toISOString();
    const timer = setTimeout(() => {
      shutdownDungeonSessionAfterInitialJoinTimeout(normalizedSessionId).catch((error) => {
        addApiEvent("event", "error", "Dungeon initial join timeout handler failed", {
          dungeonSessionId: normalizedSessionId,
          error: error.message,
        });
      });
    }, waitMs);
    if (typeof timer.unref === "function") {
      timer.unref();
    }

    initialDungeonJoinTimers.set(normalizedSessionId, timer);

    const dungeonSession = dungeonSessions.get(normalizedSessionId);
    if (dungeonSession) {
      dungeonSessions.set(normalizedSessionId, {
        ...dungeonSession,
        initialJoinDeadlineAt: deadlineAt,
        updatedAt: new Date().toISOString(),
      });
    }
  }

  function buildOfflineDungeonMemberState(user) {
    return {
      user_Index: user.user_Index,
      username: user.username,
      connectionState: "Offline",
      dungeonSessionId: "",
      isJoinable: false,
    };
  }

  function recordLoginRequest() {
    loginTelemetry.totalRequests += 1;
    loginTelemetry.lastRequestAt = new Date().toISOString();
  }

  function recordLoginSuccess() {
    loginTelemetry.successfulRequests += 1;
    loginTelemetry.lastSuccessAt = new Date().toISOString();
  }

  function recordLoginFailure(message) {
    loginTelemetry.failedRequests += 1;
    loginTelemetry.lastFailureAt = new Date().toISOString();
    loginTelemetry.lastFailureMessage = typeof message === "string" ? message : "";
  }

  function buildDungeonSessionMembers(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return [];
    }

    const stateOrder = {
      InGame: 0,
      OutGame: 1,
      Online: 2,
      Offline: 3,
    };

    return [...dungeonMemberStates.values()]
      .filter((state) => state.dungeonSessionId === normalizedSessionId)
      .map((state) => ({
        user_Index: state.user_Index,
        username: state.username,
        connectionState: state.connectionState,
        updatedAt: state.updatedAt,
      }))
      .sort((left, right) => {
        const leftOrder = stateOrder[left.connectionState] ?? 99;
        const rightOrder = stateOrder[right.connectionState] ?? 99;
        if (leftOrder !== rightOrder) {
          return leftOrder - rightOrder;
        }

        return String(left.username).localeCompare(String(right.username));
      });
  }

  function buildDungeonSessionTelemetry(session) {
    const members = buildDungeonSessionMembers(session.dungeonSessionId);
    const inGameUserCount = members.filter((member) => member.connectionState === "InGame").length;
    const outGameUserCount = members.filter((member) => member.connectionState === "OutGame").length;
    return {
      ...session,
      currentClientCount: inGameUserCount,
      inGameUserCount,
      outGameUserCount,
      memberCount: members.length,
      members,
    };
  }

  function parseNonNegativeInteger(value, defaultValue = 0) {
    const parsedValue = Number.parseInt(value, 10);
    return Number.isInteger(parsedValue) && parsedValue >= 0 ? parsedValue : defaultValue;
  }

  function parseSignedInteger(value, defaultValue = -1) {
    const parsedValue = Number.parseInt(value, 10);
    return Number.isInteger(parsedValue) ? parsedValue : defaultValue;
  }

  function normalizeLobbyConnectedUser(rawUser, nowMs = Date.now()) {
    if (!rawUser || typeof rawUser !== "object" || Array.isArray(rawUser)) {
      return null;
    }

    const loginAt = typeof rawUser.loginAt === "string" ? rawUser.loginAt.trim() : "";
    const loginAtMs = Date.parse(loginAt);
    const connectedSeconds = Number.isNaN(loginAtMs)
      ? parseNonNegativeInteger(rawUser.connectedSeconds)
      : Math.max(0, Math.floor((nowMs - loginAtMs) / 1000));
    const username = typeof rawUser.username === "string" ? rawUser.username.trim() : "";

    return {
      userIndex: parseSignedInteger(rawUser.userIndex ?? rawUser.user_Index),
      playerId: parseSignedInteger(rawUser.playerId, 0),
      username,
      loginAt,
      connectedSeconds,
      authVerified: rawUser.authVerified === true,
    };
  }

  function isLobbyTelemetryFresh(nowMs = Date.now()) {
    const updatedAtMs = Date.parse(lobbyTelemetry.updatedAt);
    return !Number.isNaN(updatedAtMs) && nowMs - updatedAtMs <= LOBBY_TELEMETRY_STALE_MS;
  }

  function buildLobbyTelemetrySnapshot() {
    const nowMs = Date.now();
    const bTelemetryFresh = isLobbyTelemetryFresh(nowMs);
    const currentClientCount = bTelemetryFresh ? parseNonNegativeInteger(lobbyTelemetry.currentClientCount) : 0;
    const connectedUsers = bTelemetryFresh && Array.isArray(lobbyTelemetry.connectedUsers)
      ? lobbyTelemetry.connectedUsers
        .map((user) => normalizeLobbyConnectedUser(user, nowMs))
        .filter(Boolean)
        .sort((left, right) => {
          const leftLoginAt = Date.parse(left.loginAt);
          const rightLoginAt = Date.parse(right.loginAt);
          if (!Number.isNaN(leftLoginAt) && !Number.isNaN(rightLoginAt) && leftLoginAt !== rightLoginAt) {
            return leftLoginAt - rightLoginAt;
          }

          return String(left.username).localeCompare(String(right.username));
        })
        .slice(0, currentClientCount)
      : [];

    return {
      ...lobbyTelemetry,
      currentClientCount,
      connectedUsers,
      connectedUserCount: connectedUsers.length,
    };
  }

  function buildTelemetrySnapshot() {
    const dungeonSessionViews = [...dungeonSessions.values()].map(buildDungeonSessionTelemetry);
    return {
      ok: true,
      generatedAt: new Date().toISOString(),
      login: { ...loginTelemetry },
      lobby: buildLobbyTelemetrySnapshot(),
      dungeon: {
        sessions: dungeonSessionViews,
        totalSessionCount: dungeonSessionViews.length,
        totalClientCount: dungeonSessionViews.reduce((sum, session) => sum + session.currentClientCount, 0),
        totalOutGameCount: dungeonSessionViews.reduce((sum, session) => sum + session.outGameUserCount, 0),
      },
    };
  }

  function refreshDungeonSession(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return;
    }

    const existingSession = dungeonSessions.get(normalizedSessionId);
    const inGameUserIndexes = [];
    const outGameUserIndexes = [];
    for (const [userIndex, state] of dungeonMemberStates) {
      if (state.dungeonSessionId !== normalizedSessionId) {
        continue;
      }

      if (state.connectionState === "InGame") {
        inGameUserIndexes.push(userIndex);
      } else if (state.connectionState === "OutGame") {
        outGameUserIndexes.push(userIndex);
      }
    }

    if (!existingSession && inGameUserIndexes.length === 0 && outGameUserIndexes.length === 0) {
      return;
    }

    dungeonSessions.set(normalizedSessionId, {
      ...(existingSession ?? {
        dungeonSessionId: normalizedSessionId,
        isJoinable: true,
        createdAt: new Date().toISOString(),
      }),
      dungeonSessionId: normalizedSessionId,
      isJoinable: existingSession?.isJoinable !== false,
      inGameUserIndexes,
      outGameUserIndexes,
      updatedAt: new Date().toISOString(),
    });
  }

  function markDungeonSessionAllocated(session, partyId, memberUserIndexes, members = []) {
    const dungeonSessionId = typeof session?.dungeonSessionId === "string" ? session.dungeonSessionId.trim() : "";
    if (!dungeonSessionId) {
      return;
    }

    const normalizedMembers = normalizeDungeonMembers(members);
    const normalizedMemberUserIndexes = normalizedMembers.length > 0
      ? normalizedMembers.map((member) => member.userIndex)
      : normalizeUserIndexes(memberUserIndexes);

    dungeonSessions.set(dungeonSessionId, {
      dungeonSessionId,
      address: typeof session.address === "string" ? session.address : dungeonSessionId,
      host: typeof session.host === "string" ? session.host : "",
      port: Number.isInteger(session.port) ? session.port : 0,
      pid: Number.isInteger(session.pid) ? session.pid : 0,
      partyId: Number.isInteger(partyId) ? partyId : -1,
      memberUserIndexes: normalizedMemberUserIndexes,
      members: normalizedMembers,
      isJoinable: true,
      inGameUserIndexes: [],
      outGameUserIndexes: [],
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    });
    scheduleInitialDungeonJoinTimeout(dungeonSessionId);
  }

  function isDungeonSessionJoinable(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return false;
    }

    const dungeonSession = dungeonSessions.get(normalizedSessionId);
    return !!dungeonSession && dungeonSession.isJoinable !== false;
  }

  function markDungeonSessionNotJoinable(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return false;
    }

    const dungeonSession = dungeonSessions.get(normalizedSessionId);
    if (!dungeonSession) {
      return false;
    }

    dungeonSessions.set(normalizedSessionId, {
      ...dungeonSession,
      isJoinable: false,
      updatedAt: new Date().toISOString(),
    });
    clearInitialDungeonJoinTimeout(normalizedSessionId);
    return true;
  }

  function isUserAllocatedToDungeonSession(dungeonSessionId, userIndex) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId || !Number.isInteger(userIndex) || userIndex <= 0) {
      return false;
    }

    const dungeonSession = dungeonSessions.get(normalizedSessionId);
    if (!dungeonSession || dungeonSession.isJoinable === false) {
      return false;
    }

    const memberUserIndexes = Array.isArray(dungeonSession.memberUserIndexes) ? dungeonSession.memberUserIndexes : [];
    return memberUserIndexes.includes(userIndex);
  }

  function getAllocatedDungeonCharacterId(dungeonSessionId, userIndex) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId || !Number.isInteger(userIndex) || userIndex <= 0) {
      return -1;
    }

    const dungeonSession = dungeonSessions.get(normalizedSessionId);
    const members = Array.isArray(dungeonSession?.members) ? dungeonSession.members : [];
    const member = members.find((item) => item.userIndex === userIndex);
    return Number.isInteger(member?.characterId) ? member.characterId : -1;
  }

  function cleanupDungeonSessionStates(dungeonSessionId) {
    const normalizedSessionId = typeof dungeonSessionId === "string" ? dungeonSessionId.trim() : "";
    if (!normalizedSessionId) {
      return 0;
    }

    let cleanedUserCount = 0;
    for (const [userIndex, state] of dungeonMemberStates) {
      if (state.dungeonSessionId === normalizedSessionId) {
        dungeonMemberStates.delete(userIndex);
        ++cleanedUserCount;
      }
    }

    clearInitialDungeonJoinTimeout(normalizedSessionId);
    dungeonSessions.delete(normalizedSessionId);
    return cleanedUserCount;
  }

  function removeUserIndexes(values, userIndexSet) {
    if (!Array.isArray(values)) {
      return [];
    }

    return values.filter((value) => !userIndexSet.has(Number.parseInt(value, 10)));
  }

  function removeMembersByUserIndex(values, userIndexSet) {
    if (!Array.isArray(values)) {
      return [];
    }

    return values.filter((value) => {
      const userIndex = Number.parseInt(value?.userIndex ?? value?.user_Index, 10);
      return !userIndexSet.has(userIndex);
    });
  }

  function cleanupPreviousDungeonAllocationsForUsers(memberUserIndexes, activeDungeonSessionId) {
    const normalizedActiveSessionId = typeof activeDungeonSessionId === "string" ? activeDungeonSessionId.trim() : "";
    const normalizedUserIndexes = normalizeUserIndexes(memberUserIndexes);
    if (normalizedUserIndexes.length === 0) {
      return {
        cleanedUserCount: 0,
        cleanedSessionCount: 0,
      };
    }

    const userIndexSet = new Set(normalizedUserIndexes);
    const touchedSessionIds = new Set();
    let cleanedUserCount = 0;
    let cleanedSessionCount = 0;

    for (const [userIndex, state] of dungeonMemberStates) {
      if (!userIndexSet.has(userIndex)) {
        continue;
      }

      const previousSessionId = typeof state.dungeonSessionId === "string" ? state.dungeonSessionId.trim() : "";
      if (!previousSessionId || previousSessionId === normalizedActiveSessionId) {
        continue;
      }

      dungeonMemberStates.delete(userIndex);
      touchedSessionIds.add(previousSessionId);
      ++cleanedUserCount;
    }

    for (const [sessionId, session] of dungeonSessions) {
      if (sessionId === normalizedActiveSessionId) {
        continue;
      }

      const previousMemberUserIndexes = Array.isArray(session.memberUserIndexes) ? session.memberUserIndexes : [];
      const previousMembers = Array.isArray(session.members) ? session.members : [];
      const previousInGameUserIndexes = Array.isArray(session.inGameUserIndexes) ? session.inGameUserIndexes : [];
      const previousOutGameUserIndexes = Array.isArray(session.outGameUserIndexes) ? session.outGameUserIndexes : [];

      const memberUserIndexesAfterCleanup = removeUserIndexes(previousMemberUserIndexes, userIndexSet);
      const membersAfterCleanup = removeMembersByUserIndex(previousMembers, userIndexSet);
      const inGameUserIndexesAfterCleanup = removeUserIndexes(previousInGameUserIndexes, userIndexSet);
      const outGameUserIndexesAfterCleanup = removeUserIndexes(previousOutGameUserIndexes, userIndexSet);
      const bChanged = memberUserIndexesAfterCleanup.length !== previousMemberUserIndexes.length
        || membersAfterCleanup.length !== previousMembers.length
        || inGameUserIndexesAfterCleanup.length !== previousInGameUserIndexes.length
        || outGameUserIndexesAfterCleanup.length !== previousOutGameUserIndexes.length;

      if (!bChanged) {
        continue;
      }

      if (memberUserIndexesAfterCleanup.length === 0
        && membersAfterCleanup.length === 0
        && inGameUserIndexesAfterCleanup.length === 0
        && outGameUserIndexesAfterCleanup.length === 0) {
        clearInitialDungeonJoinTimeout(sessionId);
        dungeonSessions.delete(sessionId);
        ++cleanedSessionCount;
        continue;
      }

      dungeonSessions.set(sessionId, {
        ...session,
        memberUserIndexes: memberUserIndexesAfterCleanup,
        members: membersAfterCleanup,
        inGameUserIndexes: inGameUserIndexesAfterCleanup,
        outGameUserIndexes: outGameUserIndexesAfterCleanup,
        updatedAt: new Date().toISOString(),
      });
      touchedSessionIds.add(sessionId);
    }

    for (const sessionId of touchedSessionIds) {
      refreshDungeonSession(sessionId);
    }

    return {
      cleanedUserCount,
      cleanedSessionCount,
    };
  }

  function getEffectiveDungeonMemberState(user) {
    const state = dungeonMemberStates.get(user.user_Index);
    if (!state) {
      return buildOfflineDungeonMemberState(user);
    }

    if (state.connectionState !== "InGame" && state.connectionState !== "OutGame") {
      return {
        ...state,
        isJoinable: false,
      };
    }

    if (isDungeonSessionJoinable(state.dungeonSessionId)) {
      return {
        ...state,
        isJoinable: true,
      };
    }

    if (dungeonSessions.has(state.dungeonSessionId)) {
      return {
        ...state,
        isJoinable: false,
      };
    }

    dungeonMemberStates.delete(user.user_Index);
    refreshDungeonSession(state.dungeonSessionId);
    return buildOfflineDungeonMemberState(user);
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
  app.use("/api-docs", swaggerUi.serve, swaggerUi.setup(swaggerSpec));
  app.use((req, res, next) => {
    if (!shouldRecordApiRequest(req)) {
      next();
      return;
    }

    const startedAt = Date.now();
    const requestPath = getRequestPath(req);
    const bSessionPingRequest = requestPath === "/api/session/ping";
    const requestBody = getRequestBody(req);
    const dungeonSessionId = getDungeonSessionIdFromBody(requestBody);
    const peerServerId = guessPeerServerId(requestPath, requestBody);
    let responseBody;
    const originalJson = res.json.bind(res);

    res.json = (body) => {
      responseBody = body;
      return originalJson(body);
    };

    if (!bSessionPingRequest) {
      addApiEvent("receive", "info", `${req.method} ${requestPath} received`, {
        method: req.method,
        path: requestPath,
        peerServerId,
        dungeonSessionId,
        query: req.query,
        requestBody,
        remoteAddress: req.ip,
      });
    }

    res.on("finish", () => {
      if (bSessionPingRequest && res.statusCode < 400) {
        return;
      }

      addApiEvent("send", apiLevelFromStatus(res.statusCode), `${req.method} ${requestPath} -> HTTP ${res.statusCode}`, {
        method: req.method,
        path: requestPath,
        peerServerId,
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
      message: "Login server is running.",
    });
  });

  app.get("/api/health/db", async (req, res) => {
    try {
      const result = await testConnection(pool);
      res.json({
        ok: true,
        db: result,
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Database connection failed.",
        error: error.message,
      });
    }
  });

  app.post("/api/test/reset-gauntlet", async (req, res) => {
    if (rejectInvalidGauntletTestAuth(req, res)) {
      return;
    }

    try {
      const userResult = await fetchGauntletTestUsers();
      if (!userResult.ok) {
        res.status(userResult.statusCode).json({
          ok: false,
          message: userResult.message,
        });
        return;
      }

      let revokedTokenCount = 0;
      for (const user of userResult.users) {
        revokedTokenCount += await revokeActiveLoginTokensForUser(pool, user.user_Index);
      }

      const testUserIndexes = new Set(userResult.users.map((user) => user.user_Index));
      const runtimeReset = resetGauntletRuntimeState({
        dungeonMemberStates,
        dungeonSessions,
        lobbyTelemetry,
        testUserIndexes,
      });

      const body = getRequestBody(req);
      const bPrepareDuplicateLogin = body.prepareDuplicateLogin === true;
      let duplicateLoginPrepared = false;
      let duplicateLoginUserId = "";
      if (bPrepareDuplicateLogin) {
        const duplicateUser = userResult.users.find((user) => user.clientIndex === 1);
        if (!duplicateUser) {
          res.status(500).json({
            ok: false,
            message: "Gauntlet duplicate login account was not found.",
          });
          return;
        }

        await createLoginToken(pool, duplicateUser.user_Index, req.ip);
        duplicateLoginPrepared = true;
        duplicateLoginUserId = duplicateUser.ID;
      }

      res.json({
        ok: true,
        message: "Gauntlet test data reset.",
        reset: {
          testUserCount: userResult.users.length,
          revokedTokenCount,
          duplicateLoginPrepared,
          duplicateLoginUserId,
          ...runtimeReset,
        },
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Gauntlet test data reset failed.",
        error: error.message,
      });
    }
  });

  app.post("/api/test/clear-login-token", async (req, res) => {
    if (rejectInvalidGauntletTestAuth(req, res)) {
      return;
    }

    const body = getRequestBody(req);
    const ID = typeof body.ID === "string" ? body.ID.trim() : "";
    if (!isGauntletTestAccountId(ID)) {
      res.status(400).json({
        ok: false,
        message: "A valid gauntlet test account ID is required.",
      });
      return;
    }

    try {
      const user = await fetchUserByID(pool, ID);
      if (!user) {
        res.status(404).json({
          ok: false,
          message: "Gauntlet test account was not found.",
        });
        return;
      }

      const revokedTokenCount = await revokeActiveLoginTokensForUser(pool, user.user_Index);
      res.json({
        ok: true,
        message: "Gauntlet login token cleared.",
        clear: {
          ID,
          revokedTokenCount,
        },
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Gauntlet login token clear failed.",
        error: error.message,
      });
    }
  });

  app.post("/api/login", async (req, res) => {
    recordLoginRequest();

    const ID = typeof req.body.ID === "string" ? req.body.ID.trim() : "";
    const password = typeof req.body.password === "string" ? req.body.password : "";

    if (!ID || !password) {
      recordLoginFailure("ID and password are required.");
      res.status(400).json({
        ok: false,
        message: "ID and password are required.",
      });
      return;
    }

    try {
      const user = await authenticateUser(pool, ID, password);
      if (!user) {
        recordLoginFailure("Invalid ID or password.");
        res.status(401).json({
          ok: false,
          message: "Invalid ID or password.",
        });
        return;
      }

      const dungeonMemberState = dungeonMemberStates.get(user.user_Index);
      const dungeonConnectionState = dungeonMemberState?.connectionState ?? "Offline";
      if (dungeonConnectionState === "InGame") {
        recordLoginFailure("Already logged in.");
        res.status(409).json({
          ok: false,
          message: "Already logged in.",
        });
        return;
      }

      const bAlreadyLoggedIn = await hasActiveLoginToken(pool, user.user_Index);
      if (dungeonConnectionState === "OutGame") {
        await revokeActiveLoginTokensForUser(pool, user.user_Index);
      } else if (bAlreadyLoggedIn) {
        recordLoginFailure("Already logged in.");
        res.status(409).json({
          ok: false,
          message: "Already logged in.",
        });
        return;
      }

      const token = await createLoginToken(pool, user.user_Index, req.ip);
      recordLoginSuccess();

      res.json({
        ok: true,
        message: "Login succeeded.",
        user,
        token,
      });
    } catch (error) {
      recordLoginFailure(error.message);
      res.status(500).json({
        ok: false,
        message: "Login failed.",
        error: error.message,
      });
    }
  });

  app.get("/api/telemetry/status", (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    res.json(buildTelemetrySnapshot());
  });

  app.get("/api/telemetry/events", (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    res.json({
      ok: true,
      generatedAt: new Date().toISOString(),
      serverId: "gameBackend",
      events: apiEvents,
    });
  });

  app.get("/api/telemetry/lobby/users", (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    const lobby = buildLobbyTelemetrySnapshot();
    res.json({
      ok: true,
      generatedAt: new Date().toISOString(),
      count: lobby.connectedUsers.length,
      users: lobby.connectedUsers,
    });
  });

  app.post("/api/telemetry/lobby", (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    const body = getRequestBody(req);
    const currentClientCount = Number.parseInt(body.currentClientCount, 10);
    const totalConnected = Number.parseInt(body.totalConnected, 10);
    const totalDisconnected = Number.parseInt(body.totalDisconnected, 10);
    const connectedUsers = Array.isArray(body.connectedUsers)
      ? body.connectedUsers.map((user) => normalizeLobbyConnectedUser(user)).filter(Boolean)
      : null;

    if (!Number.isInteger(currentClientCount) || currentClientCount < 0) {
      res.status(400).json({
        ok: false,
        message: "Valid currentClientCount is required.",
      });
      return;
    }

    lobbyTelemetry.currentClientCount = currentClientCount;
    if (Number.isInteger(totalConnected) && totalConnected >= 0) {
      lobbyTelemetry.totalConnected = totalConnected;
    }
    if (Number.isInteger(totalDisconnected) && totalDisconnected >= 0) {
      lobbyTelemetry.totalDisconnected = totalDisconnected;
    }
    if (connectedUsers) {
      lobbyTelemetry.connectedUsers = connectedUsers.slice(0, currentClientCount);
    } else if (currentClientCount === 0) {
      lobbyTelemetry.connectedUsers = [];
    }
    lobbyTelemetry.lastReportedAt = typeof body.reportedAt === "string" ? body.reportedAt : "";
    lobbyTelemetry.updatedAt = new Date().toISOString();

    res.json({
      ok: true,
      message: "Lobby telemetry updated.",
      lobby: buildLobbyTelemetrySnapshot(),
    });
  });

  app.post("/api/logout", async (req, res) => {
    const token = typeof req.body.token === "string" ? req.body.token.trim() : "";

    if (!token) {
      res.status(400).json({
        ok: false,
        message: "Token is required.",
      });
      return;
    }

    try {
      const bRevoked = await revokeLoginToken(pool, token);
      res.json({
        ok: true,
        message: bRevoked ? "Logout succeeded." : "Login token is already inactive.",
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Logout failed.",
        error: error.message,
      });
    }
  });

  app.post("/api/session/ping", async (req, res) => {
    const token = typeof req.body.token === "string" ? req.body.token.trim() : "";

    if (!token) {
      res.status(400).json({
        ok: false,
        message: "Token is required.",
      });
      return;
    }

    try {
      const bTouched = await touchLoginToken(pool, token);
      if (!bTouched) {
        res.status(401).json({
          ok: false,
          message: "Login token is inactive.",
        });
        return;
      }

      res.json({
        ok: true,
        message: "Session ping succeeded.",
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Session ping failed.",
        error: error.message,
      });
    }
  });

  app.post("/api/session/verify", async (req, res) => {
    const token = typeof req.body.token === "string" ? req.body.token.trim() : "";

    if (!token) {
      res.status(400).json({
        ok: false,
        message: "Token is required.",
      });
      return;
    }

    try {
      const user = await verifyLoginToken(pool, token);
      if (!user) {
        res.status(401).json({
          ok: false,
          message: "Login token is inactive.",
        });
        return;
      }

      res.json({
        ok: true,
        message: "Session verify succeeded.",
        user,
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Session verify failed.",
        error: error.message,
      });
    }
  });

  app.post("/api/dungeon/session/verify", async (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    const body = getRequestBody(req);
    const token = typeof body.token === "string" ? body.token.trim() : "";
    const dungeonSessionId = typeof body.dungeonSessionId === "string" ? body.dungeonSessionId.trim() : "";

    if (!token) {
      res.status(400).json({
        ok: false,
        message: "Token is required.",
      });
      return;
    }

    if (!dungeonSessionId) {
      res.status(400).json({
        ok: false,
        message: "Dungeon session ID is required.",
      });
      return;
    }

    try {
      const user = await verifyLoginToken(pool, token);
      if (!user) {
        res.status(401).json({
          ok: false,
          message: "Login token is inactive.",
        });
        return;
      }

      if (!isUserAllocatedToDungeonSession(dungeonSessionId, user.user_Index)) {
        res.status(403).json({
          ok: false,
          message: "User is not allocated to this dungeon session.",
        });
        return;
      }

      const characterId = getAllocatedDungeonCharacterId(dungeonSessionId, user.user_Index);
      if (!isValidCharacterId(characterId)) {
        res.status(403).json({
          ok: false,
          message: "User character is not allocated to this dungeon session.",
        });
        return;
      }

      res.json({
        ok: true,
        message: "Dungeon session verify succeeded.",
        user,
        dungeonSessionId,
        characterId,
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Dungeon session verify failed.",
        error: error.message,
      });
    }
  });

  app.post("/api/dungeon/allocate", async (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    const body = getRequestBody(req);
    const partyId = Number.parseInt(body.partyId, 10);
    const members = normalizeDungeonMembers(body.members);
    const memberUserIndexes = members.length > 0
      ? members.map((member) => member.userIndex)
      : normalizeUserIndexes(body.memberUserIndexes);

    try {
      const managerResponse = await postDungeonManagerJson("/api/dungeon/allocate", {
        partyId: Number.isInteger(partyId) ? partyId : -1,
        memberUserIndexes,
        members,
      });

      if (managerResponse.statusCode < 200 || managerResponse.statusCode >= 300 || !managerResponse.json.ok) {
        res.status(managerResponse.statusCode >= 400 ? managerResponse.statusCode : 502).json({
          ok: false,
          message: managerResponse.json.message || "DungeonManager allocation failed.",
          error: managerResponse.json.error,
        });
        return;
      }

      const cleanupResult = cleanupPreviousDungeonAllocationsForUsers(memberUserIndexes, managerResponse.json.session?.dungeonSessionId);
      markDungeonSessionAllocated(managerResponse.json.session, Number.isInteger(partyId) ? partyId : -1, memberUserIndexes, members);

      res.json({
        ok: true,
        message: "Dungeon server allocated.",
        session: managerResponse.json.session,
        cleanedPreviousUserCount: cleanupResult.cleanedUserCount,
        cleanedPreviousSessionCount: cleanupResult.cleanedSessionCount,
      });
    } catch (error) {
      res.status(502).json({
        ok: false,
        message: "Failed to request dungeon server allocation.",
        error: error.message,
      });
    }
  });

  app.post("/api/dungeon/shutdown", async (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
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

    const bMarkedNotJoinable = markDungeonSessionNotJoinable(dungeonSessionId);

    try {
      const managerResponse = await postDungeonManagerJson("/api/dungeon/shutdown", {
        dungeonSessionId,
      });

      if (managerResponse.statusCode < 200 || managerResponse.statusCode >= 300 || !managerResponse.json.ok) {
        res.status(managerResponse.statusCode >= 400 ? managerResponse.statusCode : 502).json({
          ok: false,
          message: managerResponse.json.message || "DungeonManager shutdown failed.",
          error: managerResponse.json.error,
        });
        return;
      }

      const cleanedUserCount = cleanupDungeonSessionStates(dungeonSessionId);

      res.json({
        ok: true,
        message: managerResponse.json.message || "Dungeon server shutdown requested.",
        bShutdown: !!managerResponse.json.bShutdown,
        bMarkedNotJoinable,
        cleanedUserCount,
      });
    } catch (error) {
      res.status(502).json({
        ok: false,
        message: "Failed to request dungeon server shutdown.",
        error: error.message,
      });
    }
  });

  app.post("/api/dungeon/session-ended", (req, res) => {
    if (!hasValidDungeonStateAuth(req)) {
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

    const bMarkedNotJoinable = markDungeonSessionNotJoinable(dungeonSessionId);
    const cleanedUserCount = cleanupDungeonSessionStates(dungeonSessionId);
    res.json({
      ok: true,
      message: "Dungeon session ended.",
      bMarkedNotJoinable,
      cleanedUserCount,
    });
  });

  app.post("/api/dungeon/member-state", async (req, res) => {
    const body = getRequestBody(req);
    const userIndex = Number.parseInt(body.userIndex, 10);
    const connectionState = typeof body.connectionState === "string" ? body.connectionState.trim() : "";
    const dungeonSessionId = typeof body.dungeonSessionId === "string" ? body.dungeonSessionId.trim() : "";
    const allowedStates = new Set(["Online", "InGame", "OutGame", "Offline"]);

    if (!hasValidDungeonStateAuth(req)) {
      res.status(401).json({
        ok: false,
        message: "Server auth is invalid.",
      });
      return;
    }

    if (!Number.isInteger(userIndex) || userIndex <= 0) {
      res.status(400).json({
        ok: false,
        message: "Valid userIndex is required.",
      });
      return;
    }

    if (!allowedStates.has(connectionState)) {
      res.status(400).json({
        ok: false,
        message: "Valid connectionState is required.",
      });
      return;
    }

    if ((connectionState === "InGame" || connectionState === "OutGame") && !dungeonSessionId) {
      res.status(400).json({
        ok: false,
        message: "Dungeon session ID is required for dungeon states.",
      });
      return;
    }

    try {
      const user = await fetchUserByIndex(pool, userIndex);
      if (!user) {
        res.status(404).json({
          ok: false,
          message: "User not found.",
        });
        return;
      }

      const previousState = dungeonMemberStates.get(user.user_Index);
      if (connectionState === "Offline") {
        dungeonMemberStates.delete(user.user_Index);
      } else {
        dungeonMemberStates.set(user.user_Index, {
          user_Index: user.user_Index,
          username: user.username,
          connectionState,
          dungeonSessionId,
          updatedAt: new Date().toISOString(),
        });
      }

      if (previousState) {
        refreshDungeonSession(previousState.dungeonSessionId);
      }
      refreshDungeonSession(dungeonSessionId);
      if (connectionState === "InGame") {
        clearInitialDungeonJoinTimeout(dungeonSessionId);
      }

      res.json({
        ok: true,
        message: "Dungeon member state updated.",
        state: getEffectiveDungeonMemberState(user),
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Failed to update dungeon member state.",
        error: error.message,
      });
    }
  });

  app.post("/api/dungeon/member-state/query", async (req, res) => {
    const body = getRequestBody(req);
    const token = typeof body.token === "string" ? body.token.trim() : "";

    if (!token) {
      res.status(400).json({
        ok: false,
        message: "Token is required.",
      });
      return;
    }

    try {
      const user = await verifyLoginToken(pool, token);
      if (!user) {
        res.status(401).json({
          ok: false,
          message: "Login token is inactive.",
        });
        return;
      }

      res.json({
        ok: true,
        state: getEffectiveDungeonMemberState(user),
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Failed to query dungeon member state.",
        error: error.message,
      });
    }
  });

  app.post("/api/register", async (req, res) => {
    const ID = typeof req.body.ID === "string" ? req.body.ID.trim() : "";
    const username = typeof req.body.username === "string" ? req.body.username.trim() : "";
    const password = typeof req.body.password === "string" ? req.body.password : "";

    if (!ID || !username || !password) {
      res.status(400).json({
        ok: false,
        message: "ID, username, and password are required.",
      });
      return;
    }

    try {
      const user = await createUser(pool, ID, username, password);
      res.status(201).json({
        ok: true,
        message: "Register succeeded.",
        user,
      });
    } catch (error) {
      if (error && error.code === "ER_DUP_ENTRY") {
        res.status(409).json({
          ok: false,
          message: "ID already exists.",
        });
        return;
      }

      res.status(500).json({
        ok: false,
        message: "Register failed.",
        error: error.message,
      });
    }
  });

  app.get("/api/users", async (req, res) => {
    const parsedLimit = Number.parseInt(req.query.limit, 10);
    const limit = Number.isNaN(parsedLimit) ? 20 : parsedLimit;

    try {
      const users = await fetchUsers(pool, limit);
      res.json({
        ok: true,
        count: users.length,
        users,
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Failed to query Users table.",
        error: error.message,
      });
    }
  });

  app.get("/api/users/:ID", async (req, res) => {
    try {
      const user = await fetchUserByID(pool, req.params.ID);
      if (!user) {
        res.status(404).json({
          ok: false,
          message: "User not found.",
        });
        return;
      }

      res.json({
        ok: true,
        user,
      });
    } catch (error) {
      res.status(500).json({
        ok: false,
        message: "Failed to query Users table.",
        error: error.message,
      });
    }
  });

  app.listen(config.server.port, config.server.host, () => {
    console.log(
      `Game backend listening on http://${config.server.host}:${config.server.port}`,
    );
  });
}

bootstrap().catch((error) => {
  console.error("Failed to start game backend.");
  console.error(error.message);
  process.exit(1);
});
