const assert = require("node:assert/strict");
const test = require("node:test");

const {
  findGauntletDungeonSessionIds,
  getGauntletTestAccounts,
  hasValidGauntletTestAuth,
  isGauntletTestAccountId,
  removeGauntletLobbyTelemetryUsers,
  resetGauntletRuntimeState,
} = require("../GameBackend/gauntlet-test-support");

test("getGauntletTestAccounts returns the fixed login lobby client mapping", () => {
  assert.deepEqual(getGauntletTestAccounts(), [
    {
      clientIndex: 1,
      id: "Gauntlet-test-1",
      username: "TestBot1",
      failureCase: "DuplicateLogin",
    },
    {
      clientIndex: 2,
      id: "Gauntlet-test-2",
      username: "TestBot2",
      failureCase: "WrongPassword",
    },
    {
      clientIndex: 3,
      id: "Gauntlet-test-3",
      username: "TestBot3",
      failureCase: "WrongID",
    },
  ]);
});

test("isGauntletTestAccountId only accepts fixed gauntlet account IDs", () => {
  assert.equal(isGauntletTestAccountId("Gauntlet-test-1"), true);
  assert.equal(isGauntletTestAccountId("Gauntlet-test-2"), true);
  assert.equal(isGauntletTestAccountId("Gauntlet-test-3"), true);
  assert.equal(isGauntletTestAccountId("normal-user"), false);
  assert.equal(isGauntletTestAccountId(""), false);
});

test("removeGauntletLobbyTelemetryUsers removes only matching test users", () => {
  const lobbyTelemetry = {
    currentClientCount: 3,
    connectedUsers: [
      { userIndex: 1, username: "TestBot1" },
      { userIndex: 99, username: "NormalUser" },
      { userIndex: 2, username: "TestBot2" },
    ],
    updatedAt: "",
  };

  const removedCount = removeGauntletLobbyTelemetryUsers(lobbyTelemetry, new Set([1, 2, 3]));

  assert.equal(removedCount, 2);
  assert.deepEqual(lobbyTelemetry.connectedUsers, [
    { userIndex: 99, username: "NormalUser" },
  ]);
  assert.equal(lobbyTelemetry.currentClientCount, 1);
  assert.match(lobbyTelemetry.updatedAt, /^\d{4}-\d{2}-\d{2}T/);
});

test("findGauntletDungeonSessionIds finds sessions that contain any test user", () => {
  const dungeonSessions = new Map([
    ["session-a", { memberUserIndexes: [1, 99] }],
    ["session-b", { members: [{ userIndex: 2 }] }],
    ["session-c", { inGameUserIndexes: [42], outGameUserIndexes: [3] }],
    ["session-d", { memberUserIndexes: [99], members: [{ userIndex: 100 }] }],
  ]);

  assert.deepEqual(
    findGauntletDungeonSessionIds(dungeonSessions, new Set([1, 2, 3])),
    ["session-a", "session-b", "session-c"],
  );
});

test("hasValidGauntletTestAuth requires a matching non-empty secret header", () => {
  const request = {
    get(name) {
      return name === "X-Gauntlet-Test-Auth" ? "secret-value" : "";
    },
  };

  assert.equal(hasValidGauntletTestAuth(request, { GAUNTLET_TEST_AUTH: "secret-value" }), true);
  assert.equal(hasValidGauntletTestAuth(request, { GAUNTLET_TEST_AUTH: "other-value" }), false);
  assert.equal(hasValidGauntletTestAuth(request, { GAUNTLET_TEST_AUTH: "" }), false);
});

test("resetGauntletRuntimeState clears only runtime state related to test users", () => {
  const dungeonMemberStates = new Map([
    [1, { user_Index: 1, username: "TestBot1" }],
    [9, { user_Index: 9, username: "NormalUser" }],
  ]);
  const dungeonSessions = new Map([
    ["session-a", { memberUserIndexes: [1, 9] }],
    ["session-b", { memberUserIndexes: [9] }],
  ]);
  const lobbyTelemetry = {
    currentClientCount: 2,
    connectedUsers: [
      { userIndex: 1, username: "TestBot1" },
      { userIndex: 9, username: "NormalUser" },
    ],
    updatedAt: "",
  };

  const reset = resetGauntletRuntimeState({
    dungeonMemberStates,
    dungeonSessions,
    lobbyTelemetry,
    testUserIndexes: new Set([1, 2, 3]),
  });

  assert.deepEqual(reset, {
    clearedDungeonMemberStateCount: 1,
    clearedDungeonSessionCount: 1,
    clearedLobbyTelemetryUserCount: 1,
  });
  assert.deepEqual([...dungeonMemberStates.keys()], [9]);
  assert.deepEqual([...dungeonSessions.keys()], ["session-b"]);
  assert.deepEqual(lobbyTelemetry.connectedUsers, [
    { userIndex: 9, username: "NormalUser" },
  ]);
});
