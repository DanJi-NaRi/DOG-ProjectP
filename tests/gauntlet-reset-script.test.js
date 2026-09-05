const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const scriptPath = path.join(projectRoot, "Run_GauntletResetTestData.ps1");

function runResetScript(args, envOverrides = {}) {
  const env = {
    ...process.env,
    ...envOverrides,
  };

  return spawnSync(
    "powershell",
    [
      "-NoProfile",
      "-ExecutionPolicy",
      "Bypass",
      "-File",
      scriptPath,
      ...args,
    ],
    {
      cwd: projectRoot,
      encoding: "utf8",
      env,
      windowsHide: true,
    },
  );
}

test("reset script fails validation when GAUNTLET_TEST_AUTH is missing", () => {
  const result = runResetScript(["-ValidateOnly"], {
    GAUNTLET_TEST_AUTH: "",
  });

  assert.notEqual(result.status, 0);
  assert.match(`${result.stdout}\n${result.stderr}`, /GAUNTLET_TEST_AUTH is required/);
});

test("reset script writes credentials JSON from Jenkins secret environment variables", () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), "projectp-gauntlet-reset-"));
  const credentialsFile = path.join(tempDir, "credentials.json");

  const result = runResetScript(
    [
      "-ValidateOnly",
      "-WriteCredentialsFile",
      "-CredentialsFile",
      credentialsFile,
    ],
    {
      GAUNTLET_TEST_AUTH: "secret-value",
      GAUNTLET_TEST_1_PASSWORD: "testPW1",
      GAUNTLET_TEST_2_PASSWORD: "testPW2",
      GAUNTLET_TEST_3_PASSWORD: "testPW3",
    },
  );

  assert.equal(result.status, 0, `${result.stdout}\n${result.stderr}`);
  assert.equal(fs.existsSync(credentialsFile), true);

  const credentials = JSON.parse(fs.readFileSync(credentialsFile, "utf8"));
  assert.deepEqual(credentials, {
    testAuth: "secret-value",
    accounts: [
      {
        clientIndex: 1,
        id: "Gauntlet-test-1",
        password: "testPW1",
        username: "TestBot1",
      },
      {
        clientIndex: 2,
        id: "Gauntlet-test-2",
        password: "testPW2",
        username: "TestBot2",
      },
      {
        clientIndex: 3,
        id: "Gauntlet-test-3",
        password: "testPW3",
        username: "TestBot3",
      },
    ],
  });
});
