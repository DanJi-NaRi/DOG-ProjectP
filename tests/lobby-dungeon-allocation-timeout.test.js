const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const lobbyGameModePath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyGMB.cpp");
const defaultEnginePath = path.join(projectRoot, "Config", "DefaultEngine.ini");

function readFile(filePath) {
  return fs.readFileSync(filePath, "utf8");
}

function readIniSection(content, sectionName) {
  const section = {};
  let inSection = false;

  for (const line of content.split(/\r?\n/)) {
    const trimmedLine = line.trim();

    if (trimmedLine === `[${sectionName}]`) {
      inSection = true;
      continue;
    }

    if (inSection && /^\[.+\]$/.test(trimmedLine)) {
      break;
    }

    if (!inSection || !trimmedLine || trimmedLine.startsWith(";")) {
      continue;
    }

    const separatorIndex = trimmedLine.indexOf("=");
    if (separatorIndex > 0) {
      section[trimmedLine.slice(0, separatorIndex)] = trimmedLine.slice(separatorIndex + 1);
    }
  }

  return section;
}

test("dungeon allocation request sets total and activity HTTP timeouts", () => {
  const source = readFile(lobbyGameModePath);

  assert.match(source, /constexpr\s+float\s+DungeonAllocateHttpTimeoutSeconds\s*=\s*105\.0f\s*;/);
  assert.match(
    source,
    /HttpRequest->SetTimeout\(DungeonAllocateHttpTimeoutSeconds\);\s*HttpRequest->SetActivityTimeout\(DungeonAllocateHttpTimeoutSeconds\);/,
  );
});

test("project HTTP config keeps WinHTTP activity timeout above dungeon startup wait", () => {
  const defaultEngine = readFile(defaultEnginePath);
  const httpSection = readIniSection(defaultEngine, "HTTP");

  assert.equal(httpSection.HttpConnectionTimeout, "105");
  assert.equal(httpSection.HttpActivityTimeout, "105");
});
