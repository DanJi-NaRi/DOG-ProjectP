const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const automationProjectPath = path.join(projectRoot, "Build", "Scripts", "ProjectP.Automation.csproj");
const testNodePath = path.join(projectRoot, "Build", "Scripts", "Gauntlet", "ProjectPLoginLobbyTest.cs");
const partyDungeonTestNodePath = path.join(projectRoot, "Build", "Scripts", "Gauntlet", "ProjectPPartyDungeonFlowTest.cs");

test("project automation module is discoverable by RunUAT project script scanning", () => {
  assert.equal(fs.existsSync(automationProjectPath), true, "ProjectP.Automation.csproj should exist under Build/Scripts");

  const projectFile = fs.readFileSync(automationProjectPath, "utf8");

  assert.match(projectFile, /<TargetFramework>net8\.0<\/TargetFramework>/);
  assert.match(projectFile, /<AssemblyName>ProjectP\.Automation<\/AssemblyName>/);
  assert.match(projectFile, /<ProjectReference Include=.*Gauntlet\.Automation\.csproj/);
});

test("login lobby test node configures three clients with isolated failure cases", () => {
  assert.equal(fs.existsSync(testNodePath), true, "ProjectPLoginLobbyTest.cs should exist");

  const source = fs.readFileSync(testNodePath, "utf8");

  assert.match(source, /class\s+ProjectPLoginLobbyTest\s*:\s*UnrealTestNode<UnrealTestConfig>/);
  assert.match(source, /private const int ClientCount = 3;/);
  assert.match(source, /RequireRoles\(UnrealTargetRole\.Client,\s*ClientCount\)/);
  assert.match(source, /ConfigureClient\(clients\[0\],\s*1,\s*"DuplicateLogin"/);
  assert.match(source, /ConfigureClient\(clients\[1\],\s*2,\s*"WrongPassword"/);
  assert.match(source, /ConfigureClient\(clients\[2\],\s*3,\s*"WrongID"/);
  assert.match(source, /SuccessMarkerFormat = "GAUNTLET_LOGIN_LOBBY_CLIENT_\{0\}_SUCCESS"/);
  assert.match(source, /FailureMarkerFormat = "GAUNTLET_LOGIN_LOBBY_CLIENT_\{0\}_FAILURE"/);
  assert.match(source, /GetLogLinesContaining\(string\.Format\(SuccessMarkerFormat,\s*clientIndex\)\)/);
  assert.match(source, /GetLogLinesContaining\(string\.Format\(FailureMarkerFormat,\s*clientIndex\)\)/);
});

test("party dungeon flow test node configures three clients with role-specific markers", () => {
  assert.equal(fs.existsSync(partyDungeonTestNodePath), true, "ProjectPPartyDungeonFlowTest.cs should exist");

  const source = fs.readFileSync(partyDungeonTestNodePath, "utf8");

  assert.match(source, /class\s+ProjectPPartyDungeonFlowTest\s*:\s*UnrealTestNode<UnrealTestConfig>/);
  assert.match(source, /private const int ClientCount = 3;/);
  assert.match(source, /RequireRoles\(UnrealTargetRole\.Client,\s*ClientCount\)/);
  assert.match(source, /ConfigureClient\(clients\[0\],\s*1,\s*credentialsFile\)/);
  assert.match(source, /ConfigureClient\(clients\[1\],\s*2,\s*credentialsFile\)/);
  assert.match(source, /ConfigureClient\(clients\[2\],\s*3,\s*credentialsFile\)/);
  assert.match(source, /SuccessMarkerFormat = "GAUNTLET_PARTY_DUNGEON_CLIENT_\{0\}_SUCCESS"/);
  assert.match(source, /FailureMarkerFormat = "GAUNTLET_PARTY_DUNGEON_CLIENT_\{0\}_FAILURE"/);
  assert.match(source, /client\.CommandLineParams\.Add\("GauntletPartyDungeonFlowTest"\)/);
  assert.match(source, /GetLogLinesContaining\(string\.Format\(SuccessMarkerFormat,\s*clientIndex\)\)/);
  assert.match(source, /GetLogLinesContaining\(string\.Format\(FailureMarkerFormat,\s*clientIndex\)\)/);
});
