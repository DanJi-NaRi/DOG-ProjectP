const state = {
  activeTab: "monitoring",
  pollTimer: null,
  selectedNodeId: "",
  selectedNodeLabel: "",
  selectedDbTable: "",
  serverLogViewMode: "brief",
  serverLogPayload: null,
  lobbyUsersPayload: null,
};

function text(value) {
  return value === undefined || value === null || value === "" ? "-" : String(value);
}

function escapeHtml(value) {
  return text(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function formatTime(value) {
  if (!value) {
    return "-";
  }

  const date = new Date(value);
  if (Number.isNaN(date.getTime())) {
    return value;
  }

  return date.toLocaleString();
}

function formatDuration(totalSeconds) {
  const safeSeconds = Math.max(0, Number.parseInt(totalSeconds, 10) || 0);
  const hours = Math.floor(safeSeconds / 3600);
  const minutes = Math.floor((safeSeconds % 3600) / 60);
  return `${hours}h ${minutes}m`;
}

function serviceBadge(status) {
  return `<span class="badge ${status}">${status === "up" ? "OK" : "DOWN"}</span>`;
}

function stateBadge(connectionState) {
  const state = text(connectionState);
  return `<span class="state-pill ${state.toLowerCase()}">${escapeHtml(state)}</span>`;
}

function processStateBadge(status) {
  const state = text(status === "unknown" ? "untracked" : status);
  return `<span class="state-pill ${state.toLowerCase()}">${escapeHtml(state)}</span>`;
}

function renderSummary(snapshot) {
  const summary = snapshot.summary ?? {};
  document.getElementById("summary").innerHTML = `
    <div class="summary-item"><strong>${text(summary.healthyServices)}/${text(summary.totalServices)}</strong><span>Services</span></div>
    <div class="summary-item"><strong>${text(summary.lobbyClients)}</strong><span>Lobby Clients</span></div>
    <div class="summary-item"><strong>${text(summary.dungeonClients)}</strong><span>Dungeon InGame</span></div>
    <div class="summary-item"><strong>${text(summary.dungeonOutGameUsers)}</strong><span>Dungeon OutGame</span></div>
    <div class="summary-item"><strong>${text(summary.loginRequests)}</strong><span>Login Requests</span></div>
  `;
}

function renderServices(snapshot) {
  const services = snapshot.services ?? [];
  document.getElementById("services").innerHTML = services.map((service) => `
    <article class="status-card">
      <div class="service-head">
        <div class="service-name">${escapeHtml(service.name)}</div>
        ${serviceBadge(service.status)}
      </div>
      <div class="service-target">${escapeHtml(service.target)}</div>
      <div class="service-detail">${escapeHtml(service.detail)} / ${escapeHtml(service.latencyMs)}ms</div>
    </article>
  `).join("");
}

function nodePosition(node, index, totalDungeons) {
  const fixed = {
    client: { left: 11, top: 45 },
    gameBackend: { left: 36, top: 18 },
    dungeonManager: { left: 65, top: 18 },
    lobby: { left: 36, top: 52 },
  };

  if (fixed[node.id]) {
    return fixed[node.id];
  }

  const dungeonIndex = Math.max(0, index - 4);
  const row = dungeonIndex % Math.max(1, Math.min(4, totalDungeons));
  const column = Math.floor(dungeonIndex / 4);
  return {
    left: 65 + column * 18,
    top: 52 + row * 14,
  };
}

function renderTopology(snapshot) {
  const nodes = snapshot.nodes ?? [];
  const edges = snapshot.edges ?? [];
  const dungeonCount = nodes.filter((node) => node.kind === "dungeon").length;
  const positions = new Map();
  const nodeHtml = nodes.map((node, index) => {
    const position = nodePosition(node, index, dungeonCount);
    positions.set(node.id, position);
    const statusClass = node.status === "up" || node.status === "down" ? node.status : "neutral";
    return `
      <button type="button" class="node ${statusClass}" data-node-id="${escapeHtml(node.id)}" data-node-label="${escapeHtml(node.label)}" style="left:${position.left}%;top:${position.top}%">
        <div class="node-title"><span>${escapeHtml(node.label)}</span><span class="dot ${statusClass}"></span></div>
        <div class="node-kind">${escapeHtml(node.kind)}</div>
      </button>
    `;
  }).join("");

  const linkHtml = `
    <svg class="link-layer" viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden="true">
      ${edges.map((edge) => {
        const from = positions.get(edge.from);
        const to = positions.get(edge.to);
        if (!from || !to) {
          return "";
        }

        return `<line x1="${from.left}" y1="${from.top}" x2="${to.left}" y2="${to.top}"></line>`;
      }).join("")}
    </svg>
  `;

  const edgeHtml = `
    <div class="edge-list">
      ${edges.map((edge) => `<div class="edge">${escapeHtml(edge.from)} -> ${escapeHtml(edge.to)}: ${escapeHtml(edge.label)}</div>`).join("")}
    </div>
  `;

  const topology = document.getElementById("topology");
  topology.innerHTML = linkHtml + nodeHtml + edgeHtml;
  topology.querySelectorAll(".node").forEach((nodeElement) => {
    nodeElement.addEventListener("click", () => {
      openServerLogModal(nodeElement.dataset.nodeId, nodeElement.dataset.nodeLabel);
    });
  });
}

function renderPorts(snapshot) {
  const ports = snapshot.ports ?? [];
  document.getElementById("ports").innerHTML = ports.map((port) => `
    <div class="port ${port.bOpen ? "open" : "closed"}">
      <span>${escapeHtml(port.port)}</span>
      <span class="dot ${port.bOpen ? "open" : ""}"></span>
    </div>
  `).join("");
}

function renderDungeonMembers(members) {
  const memberList = Array.isArray(members) ? members : [];
  if (memberList.length === 0) {
    return `<span class="empty-inline">No members</span>`;
  }

  return `
    <div class="member-list">
      ${memberList.map((member) => `
        <div class="member-row">
          <span class="member-name">${escapeHtml(member.username)}</span>
          ${stateBadge(member.connectionState)}
        </div>
      `).join("")}
    </div>
  `;
}

function renderDungeons(snapshot) {
  const dungeons = snapshot.dungeons ?? [];
  const body = document.getElementById("dungeons");
  if (dungeons.length === 0) {
    body.innerHTML = `<tr><td colspan="9" class="empty">No dungeon sessions</td></tr>`;
    return;
  }

  body.innerHTML = dungeons.map((session) => `
    <tr>
      <td>${escapeHtml(session.dungeonSessionId)}</td>
      <td>${escapeHtml(session.partyId)}</td>
      <td>${escapeHtml(session.address)}</td>
      <td>${escapeHtml(session.pid)}</td>
      <td>
        <div class="state-stack">
          ${processStateBadge(session.status ?? session.processStatus)}
          <span>Retry ${escapeHtml(session.shutdownRetryCount ?? 0)}</span>
          <span>${escapeHtml(session.nextShutdownRetryAt ? formatTime(session.nextShutdownRetryAt) : session.lastShutdownError || "")}</span>
        </div>
      </td>
      <td>${escapeHtml(session.currentClientCount)} / ${escapeHtml(session.memberCount ?? session.members?.length ?? 0)}</td>
      <td>
        <div class="state-stack">
          <span>InGame ${escapeHtml(session.inGameUserCount)}</span>
          <span>OutGame ${escapeHtml(session.outGameUserCount)}</span>
        </div>
      </td>
      <td>${renderDungeonMembers(session.members)}</td>
      <td>${formatTime(session.updatedAt || session.startedAt)}</td>
    </tr>
  `).join("");
}

function renderEvents(events) {
  const eventList = events ?? [];
  const target = document.getElementById("events");
  if (eventList.length === 0) {
    target.innerHTML = `<div class="empty">No events</div>`;
    return;
  }

  target.innerHTML = eventList.slice(0, 40).map((event) => `
    <div class="event">
      <span class="dot ${event.level}"></span>
      <div>
        <div class="event-message">${escapeHtml(event.message)}</div>
        <div class="event-detail">${formatTime(event.time)} / ${escapeHtml(event.type)}</div>
      </div>
    </div>
  `).join("");
}

function renderDatabaseSummary(payload) {
  const summary = payload.summary ?? {};
  const database = payload.database ?? {};
  document.getElementById("dbSummary").innerHTML = `
    <div class="summary-item"><strong>${escapeHtml(database.name)}</strong><span>Database</span></div>
    <div class="summary-item"><strong>${escapeHtml(summary.tableCount)}</strong><span>Tables</span></div>
    <div class="summary-item"><strong>${escapeHtml(summary.totalRows)}</strong><span>Total Rows</span></div>
    <div class="summary-item"><strong>${escapeHtml(summary.selectedTable)}</strong><span>Selected Table</span></div>
    <div class="summary-item"><strong>${escapeHtml(summary.selectedRowCount)}</strong><span>Selected Rows</span></div>
  `;
}

function renderDatabaseTables(payload) {
  const tables = payload.tables ?? [];
  const selectedTable = payload.selectedTable || "";
  const target = document.getElementById("dbTables");
  if (tables.length === 0) {
    target.innerHTML = `<div class="empty">No tables</div>`;
    return;
  }

  target.innerHTML = tables.map((table) => `
    <button type="button" class="db-table-button ${table.tableName === selectedTable ? "active" : ""}" data-table-name="${escapeHtml(table.tableName)}">
      <span class="db-table-name">${escapeHtml(table.tableName)}</span>
      <span class="db-table-meta">${escapeHtml(table.rowCount)} rows / ${escapeHtml(table.columnCount)} columns</span>
    </button>
  `).join("");

  target.querySelectorAll(".db-table-button").forEach((button) => {
    button.addEventListener("click", () => {
      state.selectedDbTable = button.dataset.tableName || "";
      refreshDatabase().catch(console.error);
    });
  });
}

function renderDatabaseRows(payload) {
  const columns = payload.columns ?? [];
  const rows = payload.rows ?? [];
  const table = document.getElementById("dbRowsTable");
  document.getElementById("dbRowsTitle").textContent = payload.selectedTable ? `${payload.selectedTable} Rows` : "Table Rows";
  document.getElementById("dbRowsSubtitle").textContent = `Updated ${formatTime(payload.generatedAt)} / limit ${text(payload.rowLimit)}`;

  if (!payload.selectedTable) {
    table.innerHTML = `<tbody><tr><td class="empty">No table selected</td></tr></tbody>`;
    return;
  }

  if (columns.length === 0) {
    table.innerHTML = `<tbody><tr><td class="empty">No columns</td></tr></tbody>`;
    return;
  }

  const headerHtml = `
    <thead>
      <tr>
        ${columns.map((column) => `<th>${escapeHtml(column.columnName)}</th>`).join("")}
      </tr>
    </thead>
  `;
  const bodyHtml = rows.length === 0
    ? `<tbody><tr><td colspan="${columns.length}" class="empty">No rows</td></tr></tbody>`
    : `
      <tbody>
        ${rows.map((row) => `
          <tr>
            ${columns.map((column) => `<td class="${column.bMasked ? "masked-cell" : ""}">${escapeHtml(row[column.columnName])}</td>`).join("")}
          </tr>
        `).join("")}
      </tbody>
    `;

  table.innerHTML = headerHtml + bodyHtml;
}

function renderDatabaseColumns(payload) {
  const columns = payload.columns ?? [];
  const target = document.getElementById("dbColumns");
  if (columns.length === 0) {
    target.innerHTML = `<tr><td colspan="5" class="empty">No columns</td></tr>`;
    return;
  }

  target.innerHTML = columns.map((column) => `
    <tr>
      <td>${escapeHtml(column.columnName)}</td>
      <td>${escapeHtml(column.dataType)}</td>
      <td>${escapeHtml(column.columnKey)}</td>
      <td>${escapeHtml(column.isNullable)}</td>
      <td>${column.bMasked ? "Yes" : "No"}</td>
    </tr>
  `).join("");
}

function renderDatabaseError(message) {
  const safeMessage = escapeHtml(message || "Database status query failed.");
  document.getElementById("dbSummary").innerHTML = `
    <div class="summary-item db-error-summary"><strong>DOWN</strong><span>${safeMessage}</span></div>
  `;
  document.getElementById("dbTables").innerHTML = `<div class="empty">${safeMessage}</div>`;
  document.getElementById("dbRowsTable").innerHTML = `<tbody><tr><td class="empty">${safeMessage}</td></tr></tbody>`;
  document.getElementById("dbColumns").innerHTML = `<tr><td colspan="5" class="empty">${safeMessage}</td></tr>`;
}

function renderDatabase(payload) {
  if (!payload.ok) {
    renderDatabaseError(payload.message || payload.error);
    return;
  }

  state.selectedDbTable = payload.selectedTable || state.selectedDbTable;
  renderDatabaseSummary(payload);
  renderDatabaseTables(payload);
  renderDatabaseRows(payload);
  renderDatabaseColumns(payload);
}

async function refreshDatabase() {
  const params = new URLSearchParams({ limit: "30" });
  if (state.selectedDbTable) {
    params.set("table", state.selectedDbTable);
  }

  const response = await fetch(`/api/monitor/db?${params.toString()}`);
  const payload = await response.json();
  renderDatabase(payload);
}

function switchTab(tabName) {
  state.activeTab = tabName === "db" ? "db" : "monitoring";
  document.querySelectorAll(".tab-button").forEach((button) => {
    button.classList.toggle("active", button.dataset.tab === state.activeTab);
  });
  document.getElementById("monitoringTabPanel").classList.toggle("active", state.activeTab === "monitoring");
  document.getElementById("dbTabPanel").classList.toggle("active", state.activeTab === "db");

  if (state.activeTab === "db") {
    refreshDatabase().catch(console.error);
  }
}

function isSensitiveSummaryKey(key) {
  const normalizedKey = String(key || "").replace(/[^a-z0-9]/gi, "").toLowerCase();
  return normalizedKey.includes("password")
    || normalizedKey.includes("token")
    || normalizedKey.includes("secret")
    || normalizedKey.includes("auth")
    || normalizedKey.endsWith("key");
}

function getLogDetail(log) {
  return log && log.detail && typeof log.detail === "object" ? log.detail : {};
}

function getObjectValue(value) {
  return value && typeof value === "object" && !Array.isArray(value) ? value : {};
}

function firstValue(...values) {
  return values.find((value) => value !== undefined && value !== null && value !== "");
}

function compactValue(value) {
  if (Array.isArray(value)) {
    return value.length === 0 ? "[]" : `${value.length} items`;
  }

  if (value && typeof value === "object") {
    return "object";
  }

  return text(value);
}

function summaryChip(label, value) {
  if (value === undefined || value === null || value === "") {
    return "";
  }

  return `
    <span class="summary-chip">
      <span>${escapeHtml(label)}</span>
      <strong>${escapeHtml(value)}</strong>
    </span>
  `;
}

function getLogSummaryText(detail) {
  const requestBody = getObjectValue(detail.requestBody);
  const responseBody = getObjectValue(detail.responseBody);
  const stateBody = getObjectValue(responseBody.state);
  const sessionBody = getObjectValue(responseBody.session);
  const userBody = getObjectValue(responseBody.user);

  if (detail.error) {
    return detail.error;
  }

  if (responseBody.message || responseBody.error) {
    return responseBody.message || responseBody.error;
  }

  if (requestBody.connectionState) {
    return `상태 ${requestBody.connectionState}`;
  }

  if (requestBody.partyId || requestBody.memberUserIndexes) {
    return `파티 ${text(requestBody.partyId)} / 인원 ${compactValue(requestBody.memberUserIndexes)}`;
  }

  if (requestBody.ID || requestBody.username) {
    return `유저 ${text(requestBody.username || requestBody.ID)}`;
  }

  if (stateBody.connectionState || stateBody.username) {
    return `상태 ${text(stateBody.username)} ${text(stateBody.connectionState)}`;
  }

  if (sessionBody.address || sessionBody.port) {
    return `${text(sessionBody.address)}:${text(sessionBody.port)}`;
  }

  if (userBody.username || userBody.ID) {
    return `유저 ${text(userBody.username || userBody.ID)}`;
  }

  return "";
}

function getLogSummaryChips(log) {
  const detail = getLogDetail(log);
  const requestBody = getObjectValue(detail.requestBody);
  const responseBody = getObjectValue(detail.responseBody);
  const sessionBody = getObjectValue(responseBody.session);
  const stateBody = getObjectValue(responseBody.state);
  const userBody = getObjectValue(responseBody.user);
  const methodPath = detail.method || detail.path
    ? `${text(detail.method)} ${text(detail.path)}`.trim()
    : "";
  const statusCode = Number.isInteger(detail.statusCode) ? `HTTP ${detail.statusCode}` : "";
  const duration = Number.isInteger(detail.durationMs) ? `${detail.durationMs}ms` : "";
  const peer = firstValue(detail.peerServerId, detail.targetServerId, detail.sourceServerId);
  const session = firstValue(
    detail.dungeonSessionId,
    requestBody.dungeonSessionId,
    responseBody.dungeonSessionId,
    sessionBody.dungeonSessionId,
    stateBody.dungeonSessionId,
  );
  const user = firstValue(
    requestBody.userIndex,
    requestBody.ID,
    requestBody.username,
    userBody.username,
    userBody.ID,
    stateBody.username,
  );
  const party = firstValue(requestBody.partyId, sessionBody.partyId);

  return [
    summaryChip("경로", methodPath),
    summaryChip("상태", statusCode),
    summaryChip("시간", duration),
    summaryChip("상대", peer),
    summaryChip("세션", session),
    summaryChip("유저", user),
    summaryChip("파티", party),
  ].join("");
}

function renderBriefServerLog(log) {
  const detail = getLogDetail(log);
  const summaryText = getLogSummaryText(detail);
  return `
    <article class="server-log compact">
      <div class="server-log-head">
        <span class="dot ${escapeHtml(log.level)}"></span>
        <span class="log-direction ${escapeHtml(log.direction)}">${escapeHtml(log.direction)}</span>
        <span class="server-log-time">${formatTime(log.time)}</span>
      </div>
      <div class="server-log-message">${escapeHtml(log.message)}</div>
      ${summaryText ? `<div class="server-log-summary-text">${escapeHtml(summaryText)}</div>` : ""}
      <div class="server-log-summary-grid">${getLogSummaryChips(log)}</div>
    </article>
  `;
}

function renderDetailedServerLog(log) {
  const detail = getLogDetail(log);
  const detailText = JSON.stringify(detail, (key, value) => {
    if (isSensitiveSummaryKey(key)) {
      return "[masked]";
    }

    return value;
  }, 2);
  return `
    <article class="server-log">
      <div class="server-log-head">
        <span class="dot ${escapeHtml(log.level)}"></span>
        <span class="log-direction ${escapeHtml(log.direction)}">${escapeHtml(log.direction)}</span>
        <span class="server-log-time">${formatTime(log.time)}</span>
      </div>
      <div class="server-log-message">${escapeHtml(log.message)}</div>
      <pre class="server-log-detail">${escapeHtml(detailText)}</pre>
    </article>
  `;
}

function isLobbyNodeSelected() {
  return state.selectedNodeId === "lobby";
}

function updateServerLogModeButtons() {
  const bShowLobbyUsersTab = isLobbyNodeSelected();
  document.querySelectorAll(".lobby-users-tab").forEach((button) => {
    button.classList.toggle("hidden", !bShowLobbyUsersTab);
  });
  if (!bShowLobbyUsersTab && state.serverLogViewMode === "users") {
    state.serverLogViewMode = "brief";
  }
  document.querySelectorAll(".modal-tab-button").forEach((button) => {
    button.classList.toggle("active", button.dataset.logView === state.serverLogViewMode);
  });
}

function switchServerLogViewMode(viewMode) {
  if (viewMode === "users" && !isLobbyNodeSelected()) {
    state.serverLogViewMode = "brief";
  } else if (viewMode === "users") {
    state.serverLogViewMode = "users";
  } else {
    state.serverLogViewMode = viewMode === "detail" ? "detail" : "brief";
  }
  updateServerLogModeButtons();
  if (state.serverLogViewMode === "users") {
    if (state.lobbyUsersPayload) {
      renderLobbyUsers(state.lobbyUsersPayload);
    } else {
      document.getElementById("serverLogList").innerHTML = `<div class="empty">Loading lobby users</div>`;
      refreshServerLogModal();
    }
    return;
  }

  if (state.serverLogPayload) {
    renderServerLogs(state.serverLogPayload);
  }
}

function renderLobbyUsers(payload) {
  const users = Array.isArray(payload?.users) ? payload.users : [];
  const logList = document.getElementById("serverLogList");
  state.lobbyUsersPayload = payload;
  updateServerLogModeButtons();
  document.getElementById("serverLogSubtitle").textContent = `Updated ${formatTime(payload?.generatedAt)} / ${users.length} users`;

  if (payload?.error) {
    logList.innerHTML = `<div class="empty">${escapeHtml(payload.error)}</div>`;
    return;
  }

  if (users.length === 0) {
    logList.innerHTML = `<div class="empty">현재 접속 유저 없음</div>`;
    return;
  }

  logList.innerHTML = `
    <div class="table-wrap lobby-user-table-wrap">
      <table class="lobby-user-table">
        <thead>
          <tr>
            <th>로그인 시간</th>
            <th>유저네임</th>
            <th>접속시간</th>
            <th>User Index</th>
            <th>인증</th>
          </tr>
        </thead>
        <tbody>
          ${users.map((user) => `
            <tr>
              <td>${formatTime(user.loginAt)}</td>
              <td>${escapeHtml(user.username || `Player ${text(user.playerId)}`)}</td>
              <td>${escapeHtml(formatDuration(user.connectedSeconds))}</td>
              <td>${escapeHtml(user.userIndex > 0 ? user.userIndex : "-")}</td>
              <td><span class="state-pill ${user.authVerified ? "verified" : "pending"}">${user.authVerified ? "인증" : "대기"}</span></td>
            </tr>
          `).join("")}
        </tbody>
      </table>
    </div>
  `;
}

function renderServerLogs(payload) {
  const logs = payload.logs ?? [];
  const logList = document.getElementById("serverLogList");
  state.serverLogPayload = payload;
  updateServerLogModeButtons();
  if (state.serverLogViewMode === "users") {
    renderLobbyUsers(state.lobbyUsersPayload);
    return;
  }
  document.getElementById("serverLogSubtitle").textContent = `Updated ${formatTime(payload.generatedAt)} / ${logs.length} logs`;

  if (logs.length === 0) {
    logList.innerHTML = `<div class="empty">No API events</div>`;
    return;
  }

  logList.innerHTML = logs
    .map((log) => state.serverLogViewMode === "detail" ? renderDetailedServerLog(log) : renderBriefServerLog(log))
    .join("");
}

async function refreshServerLogModal() {
  if (!state.selectedNodeId) {
    return;
  }

  try {
    const requests = [
      fetch(`/api/monitor/server-logs/${encodeURIComponent(state.selectedNodeId)}`).then((response) => response.json()),
    ];
    if (isLobbyNodeSelected()) {
      requests.push(fetch("/api/monitor/lobby/users").then((response) => response.json()));
    }

    const [logPayload, lobbyUsersPayload] = await Promise.all(requests);
    if (isLobbyNodeSelected()) {
      state.lobbyUsersPayload = lobbyUsersPayload;
    } else {
      state.lobbyUsersPayload = null;
    }
    renderServerLogs(logPayload);
  } catch (error) {
    if (state.serverLogViewMode === "users") {
      renderLobbyUsers({
        generatedAt: new Date().toISOString(),
        users: [],
        error: "Failed to load lobby users",
      });
      return;
    }

    document.getElementById("serverLogList").innerHTML = `<div class="empty">Failed to load API events</div>`;
  }
}

function openServerLogModal(nodeId, nodeLabel) {
  state.selectedNodeId = nodeId || "";
  state.selectedNodeLabel = nodeLabel || state.selectedNodeId;
  state.serverLogViewMode = "brief";
  state.serverLogPayload = null;
  state.lobbyUsersPayload = null;
  updateServerLogModeButtons();
  document.getElementById("serverLogTitle").textContent = state.selectedNodeLabel;
  document.getElementById("serverLogModal").classList.remove("hidden");
  refreshServerLogModal();
}

function closeServerLogModal() {
  state.selectedNodeId = "";
  state.selectedNodeLabel = "";
  state.serverLogPayload = null;
  state.lobbyUsersPayload = null;
  document.getElementById("serverLogModal").classList.add("hidden");
}

async function refresh() {
  const [statusResponse, eventsResponse] = await Promise.all([
    fetch("/api/monitor/status"),
    fetch("/api/monitor/events"),
  ]);
  const snapshot = await statusResponse.json();
  const events = await eventsResponse.json();

  document.getElementById("updatedAt").textContent = `Updated ${formatTime(snapshot.generatedAt)}`;
  renderSummary(snapshot);
  renderServices(snapshot);
  renderTopology(snapshot);
  renderPorts(snapshot);
  renderDungeons(snapshot);
  renderEvents(events.events);
  refreshServerLogModal();
  if (state.activeTab === "db") {
    refreshDatabase().catch(console.error);
  }
}

function start() {
  document.querySelectorAll(".tab-button").forEach((button) => {
    button.addEventListener("click", () => {
      switchTab(button.dataset.tab);
    });
  });
  document.getElementById("dbRefresh").addEventListener("click", () => {
    refreshDatabase().catch(console.error);
  });
  document.querySelectorAll(".modal-tab-button").forEach((button) => {
    button.addEventListener("click", () => {
      switchServerLogViewMode(button.dataset.logView);
    });
  });
  document.getElementById("serverLogClose").addEventListener("click", closeServerLogModal);
  document.getElementById("serverLogModal").addEventListener("click", (event) => {
    if (event.target.id === "serverLogModal") {
      closeServerLogModal();
    }
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      closeServerLogModal();
    }
  });
  refresh().catch(console.error);
  state.pollTimer = setInterval(() => {
    refresh().catch(console.error);
  }, 3000);
}

start();
