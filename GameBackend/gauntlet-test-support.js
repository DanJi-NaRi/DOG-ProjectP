const GAUNTLET_TEST_ACCOUNTS = Object.freeze([
  Object.freeze({
    clientIndex: 1,
    id: "Gauntlet-test-1",
    username: "TestBot1",
    failureCase: "DuplicateLogin",
  }),
  Object.freeze({
    clientIndex: 2,
    id: "Gauntlet-test-2",
    username: "TestBot2",
    failureCase: "WrongPassword",
  }),
  Object.freeze({
    clientIndex: 3,
    id: "Gauntlet-test-3",
    username: "TestBot3",
    failureCase: "WrongID",
  }),
]);

const GAUNTLET_TEST_ACCOUNT_IDS = new Set(GAUNTLET_TEST_ACCOUNTS.map((account) => account.id));

function getGauntletTestAccounts() {
  return GAUNTLET_TEST_ACCOUNTS.map((account) => ({ ...account }));
}

function isGauntletTestAccountId(id) {
  return typeof id === "string" && GAUNTLET_TEST_ACCOUNT_IDS.has(id.trim());
}

function parseUserIndex(value) {
  const parsedValue = Number.parseInt(value, 10);
  return Number.isInteger(parsedValue) && parsedValue > 0 ? parsedValue : -1;
}

function removeGauntletLobbyTelemetryUsers(lobbyTelemetry, testUserIndexes) {
  if (!lobbyTelemetry || !Array.isArray(lobbyTelemetry.connectedUsers)) {
    return 0;
  }

  const originalUsers = lobbyTelemetry.connectedUsers;
  const remainingUsers = originalUsers.filter((user) => {
    const userIndex = parseUserIndex(user?.userIndex ?? user?.user_Index);
    return !testUserIndexes.has(userIndex);
  });

  lobbyTelemetry.connectedUsers = remainingUsers;
  lobbyTelemetry.currentClientCount = remainingUsers.length;
  lobbyTelemetry.updatedAt = new Date().toISOString();

  return originalUsers.length - remainingUsers.length;
}

function sessionContainsAnyUser(session, testUserIndexes) {
  const userIndexGroups = [
    session?.memberUserIndexes,
    session?.inGameUserIndexes,
    session?.outGameUserIndexes,
  ];

  for (const group of userIndexGroups) {
    if (!Array.isArray(group)) {
      continue;
    }

    if (group.some((userIndex) => testUserIndexes.has(parseUserIndex(userIndex)))) {
      return true;
    }
  }

  if (Array.isArray(session?.members)) {
    return session.members.some((member) => {
      const userIndex = parseUserIndex(member?.userIndex ?? member?.user_Index);
      return testUserIndexes.has(userIndex);
    });
  }

  return false;
}

function findGauntletDungeonSessionIds(dungeonSessions, testUserIndexes) {
  if (!(dungeonSessions instanceof Map)) {
    return [];
  }

  return [...dungeonSessions.entries()]
    .filter(([, session]) => sessionContainsAnyUser(session, testUserIndexes))
    .map(([sessionId]) => sessionId);
}

function hasValidGauntletTestAuth(req, env = process.env) {
  const expectedAuth = typeof env.GAUNTLET_TEST_AUTH === "string" ? env.GAUNTLET_TEST_AUTH.trim() : "";
  if (!expectedAuth) {
    return false;
  }

  const requestAuth = typeof req?.get === "function" ? String(req.get("X-Gauntlet-Test-Auth") ?? "").trim() : "";
  return requestAuth === expectedAuth;
}

function resetGauntletRuntimeState({
  dungeonMemberStates,
  dungeonSessions,
  lobbyTelemetry,
  testUserIndexes,
}) {
  let clearedDungeonMemberStateCount = 0;
  if (dungeonMemberStates instanceof Map) {
    for (const userIndex of testUserIndexes) {
      if (dungeonMemberStates.delete(userIndex)) {
        clearedDungeonMemberStateCount += 1;
      }
    }
  }

  const sessionIdsToDelete = findGauntletDungeonSessionIds(dungeonSessions, testUserIndexes);
  for (const sessionId of sessionIdsToDelete) {
    dungeonSessions.delete(sessionId);
  }

  return {
    clearedDungeonMemberStateCount,
    clearedDungeonSessionCount: sessionIdsToDelete.length,
    clearedLobbyTelemetryUserCount: removeGauntletLobbyTelemetryUsers(lobbyTelemetry, testUserIndexes),
  };
}

module.exports = {
  findGauntletDungeonSessionIds,
  getGauntletTestAccounts,
  hasValidGauntletTestAuth,
  isGauntletTestAccountId,
  removeGauntletLobbyTelemetryUsers,
  resetGauntletRuntimeState,
};
