const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const headerPath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyGSB.h");
const sourcePath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyGSB.cpp");

test("lobby game state exposes pending party join requests as read-only data", () => {
  assert.equal(fs.existsSync(headerPath), true, "CPP_LobbyGSB.h should exist");
  assert.equal(fs.existsSync(sourcePath), true, "CPP_LobbyGSB.cpp should exist");

  const header = fs.readFileSync(headerPath, "utf8");
  const source = fs.readFileSync(sourcePath, "utf8");

  assert.match(
    header,
    /UFUNCTION\(BlueprintPure,\s*Category\s*=\s*"Lobby\|Party"\)\s*const TArray<FLobbyPartyJoinRequestInfo>& GetPendingPartyJoinRequests\(\) const;/,
  );
  assert.match(
    source,
    /const TArray<FLobbyPartyJoinRequestInfo>& ACPP_LobbyGSB::GetPendingPartyJoinRequests\(\) const\s*\{\s*return PendingPartyJoinRequests;\s*\}/,
  );
});
