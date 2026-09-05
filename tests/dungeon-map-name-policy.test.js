const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const dungeonMapPath = "/Game/LeDuat/Maps/Map_Dungeon_1";
const dungeonMapAssetPath = `${dungeonMapPath}.Map_Dungeon_1`;
const legacyDungeonMapPath = /\/Game\/LeDuat\/Maps\/Map_Dungeon(?:[.'"`\s]|$)/;

function readProjectFile(...segments) {
  return fs.readFileSync(path.join(projectRoot, ...segments), "utf8");
}

function assertUsesDungeonMap(text, label) {
  assert.match(text, new RegExp(dungeonMapPath.replaceAll("/", "\\/")), `${label} should use Map_Dungeon_1`);
}

function assertDoesNotUseLegacyDungeonMap(text, label) {
  assert.doesNotMatch(text, legacyDungeonMapPath, `${label} should not launch or cook Map_Dungeon`);
}

test("backend dungeon manager config uses Map_Dungeon_1", () => {
  const config = JSON.parse(readProjectFile("GameBackend", "config.example.json"));
  assert.equal(config.dungeonManager.mapName, dungeonMapPath);
});

test("packaging scripts and editor startup map use Map_Dungeon_1", () => {
  const serverPackaging = readProjectFile("Run_ServerBuildPackaging.ps1");
  const clientPackaging = readProjectFile("Run_ClientBuildPackaging.ps1");
  const defaultEngine = readProjectFile("Config", "DefaultEngine.ini");

  assertUsesDungeonMap(serverPackaging, "server packaging");
  assertUsesDungeonMap(clientPackaging, "client packaging");
  assert.match(defaultEngine, new RegExp(dungeonMapAssetPath.replaceAll("/", "\\/").replace(".", "\\.")));

  assertDoesNotUseLegacyDungeonMap(serverPackaging, "server packaging");
  assertDoesNotUseLegacyDungeonMap(clientPackaging, "client packaging");
  assertDoesNotUseLegacyDungeonMap(defaultEngine, "DefaultEngine.ini");
});

test("dungeon runtime fallback and reconnect map resolver know Map_Dungeon_1", () => {
  const dungeonManager = readProjectFile("GameBackend", "dungeon-manager.js");
  const dungeonGameMode = readProjectFile("Source", "ProjectP", "Dungeon", "CPP_DungeonGM.cpp");

  assert.match(dungeonManager, /"Map_Dungeon_1"/);
  assert.match(dungeonGameMode, /TEXT\("Map_Dungeon_1"\)/);
});

test("Jenkins runtime config forces Map_Dungeon_1 after copying secret backend config", () => {
  const jenkinsfile = readProjectFile("Jenkinsfile");
  const gauntletOnlyJenkinsfile = readProjectFile("Jenkinsfile_GauntletOnly");
  const forcedMapPattern = /dungeonManager\s*\|\s*Add-Member[\s\S]*-Name mapName[\s\S]*\/Game\/LeDuat\/Maps\/Map_Dungeon_1[\s\S]*-Force/;

  assert.match(jenkinsfile, forcedMapPattern);
  assert.match(gauntletOnlyJenkinsfile, forcedMapPattern);
});
