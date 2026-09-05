const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const cheatHeaderPath = path.join(projectRoot, "Source", "ProjectP", "Dungeon", "CPP_DungeonCheatManager.h");
const cheatSourcePath = path.join(projectRoot, "Source", "ProjectP", "Dungeon", "CPP_DungeonCheatManager.cpp");
const dungeonPCHeaderPath = path.join(projectRoot, "Source", "ProjectP", "Dungeon", "DungeonPC.h");
const dungeonPCSourcePath = path.join(projectRoot, "Source", "ProjectP", "Dungeon", "DungeonPC.cpp");
const dungeonGMHeaderPath = path.join(projectRoot, "Source", "ProjectP", "Dungeon", "CPP_DungeonGM.h");
const dungeonGMSourcePath = path.join(projectRoot, "Source", "ProjectP", "Dungeon", "CPP_DungeonGM.cpp");

test("dungeon cheat manager exposes a console command entry point", () => {
  assert.equal(fs.existsSync(cheatHeaderPath), true, "CPP_DungeonCheatManager.h should exist");
  assert.equal(fs.existsSync(cheatSourcePath), true, "CPP_DungeonCheatManager.cpp should exist");

  const header = fs.readFileSync(cheatHeaderPath, "utf8");
  const source = fs.readFileSync(cheatSourcePath, "utf8");

  assert.match(header, /#include\s+"GameFramework\/CheatManager\.h"/);
  assert.match(header, /class\s+PROJECTP_API\s+UCPP_DungeonCheatManager\s*:\s*public\s+UCheatManager/);
  assert.match(header, /UFUNCTION\(Exec\)\s*void\s+SpawnTestEnemies\(int32\s+Count\);/);
  assert.match(source, /ADungeonPC\*\s+DungeonPC\s*=\s*GetDungeonPlayerController\(\);/);
  assert.match(source, /DungeonPC->RequestSpawnTestEnemies\(Count\);/);
});

test("dungeon player controller routes cheat requests to the server", () => {
  const header = fs.readFileSync(dungeonPCHeaderPath, "utf8");
  const source = fs.readFileSync(dungeonPCSourcePath, "utf8");

  assert.match(header, /ADungeonPC\(\);/);
  assert.match(header, /void\s+RequestSpawnTestEnemies\(int32\s+Count\);/);
  assert.match(header, /UFUNCTION\(Server,\s*Reliable\)\s*void\s+ServerSpawnTestEnemies\(int32\s+Count\);/);
  assert.match(source, /#include\s+"CPP_DungeonCheatManager\.h"/);
  assert.match(source, /CheatClass\s*=\s*UCPP_DungeonCheatManager::StaticClass\(\);/);
  assert.match(source, /void\s+ADungeonPC::RequestSpawnTestEnemies\(int32\s+Count\)[\s\S]*ServerSpawnTestEnemies\(Count\);/);
  assert.match(source, /void\s+ADungeonPC::ServerSpawnTestEnemies_Implementation\(int32\s+Count\)[\s\S]*DungeonGM->SpawnTestEnemiesForStressTest\(Count\);/);
});

test("dungeon game mode owns the authoritative stress-test enemy spawn", () => {
  const header = fs.readFileSync(dungeonGMHeaderPath, "utf8");
  const source = fs.readFileSync(dungeonGMSourcePath, "utf8");

  assert.match(header, /class\s+ACPP_EnemyBase;/);
  assert.match(header, /void\s+SpawnTestEnemiesForStressTest\(int32\s+Count\);/);
  assert.match(header, /TSubclassOf<ACPP_EnemyBase>\s+StressTestEnemyClass;/);
  assert.match(source, /void\s+ACPP_DungeonGM::SpawnTestEnemiesForStressTest\(int32\s+Count\)/);
  assert.match(source, /World->SpawnActor<ACPP_EnemyBase>/);
});
