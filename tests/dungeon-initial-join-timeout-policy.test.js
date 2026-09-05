const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const dungeonGMSourcePath = path.join(projectRoot, "Source", "ProjectP", "Dungeon", "CPP_DungeonGM.cpp");
const gameBackendSourcePath = path.join(projectRoot, "GameBackend", "server.js");
const gameBackendConfigPath = path.join(projectRoot, "GameBackend", "config.json");
const gameBackendExampleConfigPath = path.join(projectRoot, "GameBackend", "config.example.json");

function readFile(filePath) {
  return fs.readFileSync(filePath, "utf8");
}

function getFunctionBody(source, signature) {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `${signature} should exist`);

  const bodyStart = source.indexOf("{", start);
  assert.notEqual(bodyStart, -1, `${signature} should have a body`);

  let depth = 0;
  for (let index = bodyStart; index < source.length; ++index) {
    if (source[index] === "{") {
      ++depth;
    } else if (source[index] === "}") {
      --depth;
      if (depth === 0) {
        return source.slice(bodyStart + 1, index);
      }
    }
  }

  assert.fail(`${signature} body should be closed`);
}

test("dungeon BeginPlay does not schedule empty shutdown before the session is joinable", () => {
  const source = readFile(dungeonGMSourcePath);
  const beginPlayBody = getFunctionBody(source, "void ACPP_DungeonGM::BeginPlay()");

  assert.doesNotMatch(beginPlayBody, /ScheduleEmptyDungeonShutdown\(\);/);
});

test("GameBackend config keeps initial dungeon join wait at three minutes", () => {
  const config = JSON.parse(readFile(gameBackendConfigPath));
  const exampleConfig = JSON.parse(readFile(gameBackendExampleConfigPath));

  assert.equal(config.dungeonManager.initialJoinWaitMs, 180000);
  assert.equal(exampleConfig.dungeonManager.initialJoinWaitMs, 180000);
});

test("GameBackend schedules initial join shutdown when a dungeon session becomes joinable", () => {
  const source = readFile(gameBackendSourcePath);

  assert.match(source, /function\s+scheduleInitialDungeonJoinTimeout\(dungeonSessionId\)/);
  assert.match(source, /const\s+initialDungeonJoinTimers\s*=\s*new\s+Map\(\);/);
  assert.match(source, /getInitialJoinWaitMs\(\)[\s\S]*initialJoinWaitMs[\s\S]*180000/);
  assert.match(source, /markDungeonSessionAllocated[\s\S]*scheduleInitialDungeonJoinTimeout\(dungeonSessionId\);/);
  assert.match(source, /connectionState\s*===\s*"InGame"[\s\S]*clearInitialDungeonJoinTimeout\(dungeonSessionId\);/);
  assert.match(source, /postDungeonManagerJson\("\/api\/dungeon\/shutdown",\s*\{\s*dungeonSessionId,\s*\}\)/);
});
