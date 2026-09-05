const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const jenkinsfilePath = path.join(projectRoot, "Jenkinsfile");
const jenkinsfile = fs.readFileSync(jenkinsfilePath, "utf8");
const gauntletOnlyJenkinsfilePath = path.join(projectRoot, "Jenkinsfile_GauntletOnly");
const gauntletOnlyJenkinsfile = fs.readFileSync(gauntletOnlyJenkinsfilePath, "utf8");

function stageIndex(stageName) {
  return jenkinsfile.indexOf(`stage('${stageName}')`);
}

function assertContains(text) {
  assert.notEqual(jenkinsfile.indexOf(text), -1, `Jenkinsfile should contain: ${text}`);
}

function assertDungeonStartupWaitGuard(pipelineText, label) {
  assert.match(pipelineText, /Copy-Item -LiteralPath \$secretBackendConfigPath -Destination \$backendConfigPath -Force[\s\S]*startupWaitMs[\s\S]*90000[\s\S]*ConvertTo-Json/, `${label} should force dungeon startupWaitMs to at least 90000 after copying Jenkins secret config`);
}

test("gauntlet stages run after server health check in the expected order", () => {
  const healthCheck = stageIndex("Health Check");
  const reset = stageIndex("Reset Gauntlet Test Data");
  const run = stageIndex("Run Gauntlet Login Lobby Test");
  const resetBeforePartyDungeon = stageIndex("Reset Gauntlet Test Data Before Party Dungeon Flow");
  const partyDungeonRun = stageIndex("Run Gauntlet Party Dungeon Flow Test");
  const postReset = stageIndex("Post Reset Gauntlet Test Data");

  assert.notEqual(healthCheck, -1, "Health Check stage should exist");
  assert.notEqual(reset, -1, "Reset Gauntlet Test Data stage should exist");
  assert.notEqual(run, -1, "Run Gauntlet Login Lobby Test stage should exist");
  assert.notEqual(resetBeforePartyDungeon, -1, "Reset before Party Dungeon Flow stage should exist");
  assert.notEqual(partyDungeonRun, -1, "Run Gauntlet Party Dungeon Flow Test stage should exist");
  assert.notEqual(postReset, -1, "Post Reset Gauntlet Test Data cleanup should exist");
  assert.ok(healthCheck < reset, "Reset should run after Health Check");
  assert.ok(reset < run, "Gauntlet should run after reset");
  assert.ok(run < resetBeforePartyDungeon, "Reset should run after login lobby test before party dungeon flow");
  assert.ok(resetBeforePartyDungeon < partyDungeonRun, "Party dungeon flow should run after its reset");
  assert.ok(partyDungeonRun < postReset, "Post reset cleanup should be defined after the Gauntlet run stages");
});

test("gauntlet reset stage uses Jenkins-managed secrets and prepares duplicate login state", () => {
  assertContains("projectp-gauntlet-test-auth");
  assertContains("projectp-gauntlet-test-1-password");
  assertContains("projectp-gauntlet-test-2-password");
  assertContains("projectp-gauntlet-test-3-password");
  assertContains("GAUNTLET_TEST_AUTH");
  assertContains("GAUNTLET_TEST_1_PASSWORD");
  assertContains("GAUNTLET_TEST_2_PASSWORD");
  assertContains("GAUNTLET_TEST_3_PASSWORD");
  assertContains("Run_GauntletResetTestData.ps1");
  assertContains("-WriteCredentialsFile");
  assertContains("-PrepareDuplicateLogin");
});

test("start servers stage injects gauntlet auth so GameBackend can verify Reset API calls", () => {
  assert.match(jenkinsfile, /stage\('Start Servers'\)\s*\{[\s\S]*withCredentials\(\[[\s\S]*projectp-gauntlet-test-auth[\s\S]*GAUNTLET_TEST_AUTH[\s\S]*Start_AllServers\.bat --no-pause/);
});

test("Jenkins runtime config keeps dungeon allocation startup wait stable for Gauntlet", () => {
  assertDungeonStartupWaitGuard(jenkinsfile, "Jenkinsfile");
  assertDungeonStartupWaitGuard(gauntletOnlyJenkinsfile, "Jenkinsfile_GauntletOnly");
});

test("gauntlet run stage invokes the login lobby test script", () => {
  assertContains("catchError(buildResult: 'FAILURE', stageResult: 'FAILURE')");
  assertContains("Run_GauntletLoginLobbyTest.ps1");
});

test("gauntlet party dungeon flow stage invokes the extended flow test script", () => {
  assertContains("Run_GauntletPartyDungeonFlowTest.ps1");
});

test("gauntlet party dungeon flow gets fresh reset data after login lobby test", () => {
  assert.match(jenkinsfile, /stage\('Reset Gauntlet Test Data Before Party Dungeon Flow'\)\s*\{[\s\S]*withCredentials\(\[[\s\S]*projectp-gauntlet-test-auth[\s\S]*projectp-gauntlet-test-1-password[\s\S]*projectp-gauntlet-test-2-password[\s\S]*projectp-gauntlet-test-3-password[\s\S]*Run_GauntletResetTestData\.ps1 -WriteCredentialsFile/);
});

test("gauntlet post cleanup always resets data and deletes the temporary credentials file", () => {
  assert.match(jenkinsfile, /stage\('Post Reset Gauntlet Test Data'\)\s*\{[\s\S]*withCredentials\(\[[\s\S]*projectp-gauntlet-test-auth[\s\S]*Run_GauntletResetTestData\.ps1[\s\S]*-DeleteCredentialsFile/);
});

test("publish distribution runs after gauntlet cleanup only when the build remains successful", () => {
  const postReset = stageIndex("Post Reset Gauntlet Test Data");
  const publish = stageIndex("Publish Distribution");

  assert.notEqual(publish, -1, "Publish Distribution stage should exist");
  assert.ok(postReset < publish, "Publish should run after Gauntlet cleanup");
  assert.match(jenkinsfile, /stage\('Publish Distribution'\)\s*\{[\s\S]*when\s*\{[\s\S]*currentBuild\.currentResult == 'SUCCESS'/);
});
