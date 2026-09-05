const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const scriptPath = path.join(projectRoot, "Run_GauntletLoginLobbyTest.ps1");
const partyDungeonScriptPath = path.join(projectRoot, "Run_GauntletPartyDungeonFlowTest.ps1");

function makeTempGauntletLayout() {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), "projectp-gauntlet-run-"));
  const runUAT = path.join(tempDir, "RunUAT.bat");
  const clientBuildRoot = path.join(tempDir, "Client");
  const clientExe = path.join(clientBuildRoot, "Windows", "ProjectP.exe");
  const credentialsFile = path.join(tempDir, "credentials.json");

  fs.mkdirSync(path.dirname(runUAT), { recursive: true });
  fs.mkdirSync(path.dirname(clientExe), { recursive: true });
  fs.writeFileSync(runUAT, "@echo off\r\nexit /b 0\r\n", "utf8");
  fs.writeFileSync(clientExe, "", "utf8");
  fs.writeFileSync(credentialsFile, JSON.stringify({ testAuth: "secret", accounts: [] }), "utf8");

  return {
    clientBuildRoot,
    credentialsFile,
    runUAT,
  };
}

function runGauntletScript(args, selectedScriptPath = scriptPath) {
  return spawnSync(
    "powershell",
    [
      "-NoProfile",
      "-ExecutionPolicy",
      "Bypass",
      "-File",
      selectedScriptPath,
      ...args,
    ],
    {
      cwd: projectRoot,
      encoding: "utf8",
      windowsHide: true,
    },
  );
}

test("gauntlet run script fails validation when credentials file is missing", () => {
  const layout = makeTempGauntletLayout();
  const missingCredentialsFile = path.join(path.dirname(layout.credentialsFile), "missing.json");

  const result = runGauntletScript([
    "-ValidateOnly",
    "-RunUAT",
    layout.runUAT,
    "-ClientBuildRoot",
    layout.clientBuildRoot,
    "-CredentialsFile",
    missingCredentialsFile,
  ]);

  assert.notEqual(result.status, 0);
  assert.match(`${result.stdout}\n${result.stderr}`, /Gauntlet credentials file not found/);
});

test("gauntlet run script dry run prints RunUAT command with expected arguments", () => {
  const layout = makeTempGauntletLayout();

  const result = runGauntletScript([
    "-DryRun",
    "-RunUAT",
    layout.runUAT,
    "-ClientBuildRoot",
    layout.clientBuildRoot,
    "-CredentialsFile",
    layout.credentialsFile,
  ]);

  assert.equal(result.status, 0, `${result.stdout}\n${result.stderr}`);
  assert.match(result.stdout, /RunUnreal/);
  assert.match(result.stdout, /-ScriptsForProject=.*ProjectP\.uproject/);
  assert.match(result.stdout, /-test=ProjectPLoginLobbyTest/);
  assert.match(result.stdout, /-numclients=3/);
  assert.match(result.stdout, /-packaged/);
  assert.match(result.stdout, /-GauntletCredentialsFile=/);
  assert.match(result.stdout, /-GauntletLoginLobbyTest/);
});

test("party dungeon run script dry run prints RunUAT command with expected arguments", () => {
  const layout = makeTempGauntletLayout();

  const result = runGauntletScript(
    [
      "-DryRun",
      "-RunUAT",
      layout.runUAT,
      "-ClientBuildRoot",
      layout.clientBuildRoot,
      "-CredentialsFile",
      layout.credentialsFile,
    ],
    partyDungeonScriptPath,
  );

  assert.equal(result.status, 0, `${result.stdout}\n${result.stderr}`);
  assert.match(result.stdout, /RunUnreal/);
  assert.match(result.stdout, /-ScriptsForProject=.*ProjectP\.uproject/);
  assert.match(result.stdout, /-test=ProjectPPartyDungeonFlowTest/);
  assert.match(result.stdout, /-numclients=3/);
  assert.match(result.stdout, /-packaged/);
  assert.match(result.stdout, /-GauntletCredentialsFile=/);
  assert.match(result.stdout, /-GauntletPartyDungeonFlowTest/);
});
