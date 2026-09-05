const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const headerPath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyPC.h");

test("lobby player controller inherits shared player controller base", () => {
  assert.equal(fs.existsSync(headerPath), true, "CPP_LobbyPC.h should exist");

  const header = fs.readFileSync(headerPath, "utf8");

  assert.match(header, /#include\s+"..\/MyPlayerController\.h"/);
  assert.match(header, /class\s+PROJECTP_API\s+ACPP_LobbyPC\s*:\s*public\s+AMyPlayerController/);
});
