const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const lobbyPlayerStateHeaderPath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyPS.h");
const lobbyPlayerStateSourcePath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyPS.cpp");
const myPlayerStateHeaderPath = path.join(projectRoot, "Source", "ProjectP", "GAS", "MyPlayerState.h");
const myPlayerStateSourcePath = path.join(projectRoot, "Source", "ProjectP", "GAS", "MyPlayerState.cpp");

test("lobby player state inherits shared my player state", () => {
  const header = fs.readFileSync(lobbyPlayerStateHeaderPath, "utf8");

  assert.match(header, /#include\s+"\.\.\/GAS\/MyPlayerState\.h"/);
  assert.match(header, /class\s+PROJECTP_API\s+ACPP_LobbyPS\s*:\s*public\s+AMyPlayerState/);
  assert.doesNotMatch(header, /#include\s+"GameFramework\/PlayerState\.h"/);
});

test("lobby player state reuses shared auth fields instead of duplicating them", () => {
  const header = fs.readFileSync(lobbyPlayerStateHeaderPath, "utf8");
  const source = fs.readFileSync(lobbyPlayerStateSourcePath, "utf8");

  assert.doesNotMatch(header, /UPROPERTY\([\s\S]*?\)\s*FString\s+Username\s*;/);
  assert.doesNotMatch(header, /UPROPERTY\([\s\S]*?\)\s*int32\s+UserIndex\s*=/);
  assert.doesNotMatch(header, /UPROPERTY\([\s\S]*?\)\s*bool\s+bLobbyAuthVerified\s*=/);
  assert.doesNotMatch(source, /DOREPLIFETIME\(ACPP_LobbyPS,\s*Username\)/);
  assert.doesNotMatch(source, /DOREPLIFETIME\(ACPP_LobbyPS,\s*UserIndex\)/);
  assert.doesNotMatch(source, /DOREPLIFETIME\(ACPP_LobbyPS,\s*bLobbyAuthVerified\)/);
  assert.match(source, /bool\s+ACPP_LobbyPS::IsLobbyAuthVerified\(\)\s+const\s*\{[\s\S]*?return\s+IsAuthVerified\(\);[\s\S]*?\}/);
});

test("shared player state exposes username setter for lobby compatibility", () => {
  const header = fs.readFileSync(myPlayerStateHeaderPath, "utf8");
  const source = fs.readFileSync(myPlayerStateSourcePath, "utf8");

  assert.match(header, /void\s+SetUsername\(const\s+FString&\s+NewUsername\);/);
  assert.match(source, /void\s+AMyPlayerState::SetUsername\(const\s+FString&\s+NewUsername\)\s*\{[\s\S]*?Username\s*=\s*NewUsername;[\s\S]*?\}/);
});
