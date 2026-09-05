const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const headerPath = path.join(
  projectRoot,
  "Source",
  "ProjectP",
  "GameInstance",
  "SubSystems",
  "Gauntlet",
  "GauntletLoginLobbySubsystem.h",
);
const sourcePath = path.join(
  projectRoot,
  "Source",
  "ProjectP",
  "GameInstance",
  "SubSystems",
  "Gauntlet",
  "GauntletLoginLobbySubsystem.cpp",
);

test("gauntlet login lobby subsystem declares the approved public and private surface", () => {
  assert.equal(fs.existsSync(headerPath), true, "GauntletLoginLobbySubsystem.h should exist");

  const header = fs.readFileSync(headerPath, "utf8");

  assert.match(header, /class\s+PROJECTP_API\s+UGauntletLoginLobbySubsystem\s*:\s*public\s+UGameInstanceSubsystem/);
  assert.match(header, /public:\s*[\s\S]*virtual void Initialize\(FSubsystemCollectionBase& Collection\) override;/);
  assert.match(header, /public:\s*[\s\S]*virtual void Deinitialize\(\) override;/);
  assert.match(header, /private:\s*[\s\S]*bool bIsRunning = false;/);
  assert.match(header, /private:\s*[\s\S]*int32 ClientIndex = 0;/);
  assert.match(header, /private:\s*[\s\S]*FString FailureCase;/);

  [
    "StartTestIfRequested",
    "RunExpectedFailureLogin",
    "HandleExpectedFailureResult",
    "RunSuccessLogin",
    "HandleSuccessLoginResult",
    "TravelToLobby",
    "WaitForLobbyAuthResult",
    "FinishSuccess",
    "FinishFailure",
  ].forEach((functionName) => {
    assert.match(header, new RegExp(`${functionName}\\(`), `${functionName} should be declared`);
  });
});

test("gauntlet login lobby subsystem implements the login-lobby automation contract", () => {
  assert.equal(fs.existsSync(sourcePath), true, "GauntletLoginLobbySubsystem.cpp should exist");

  const source = fs.readFileSync(sourcePath, "utf8");

  assert.match(source, /TestFlag\[\] = TEXT\("GauntletLoginLobbyTest"\)/);
  assert.match(source, /ClientIndexKey\[\] = TEXT\("GauntletClientIndex="\)/);
  assert.match(source, /FailureCaseKey\[\] = TEXT\("GauntletLoginFailureCase="\)/);
  assert.match(source, /CredentialsFileKey\[\] = TEXT\("GauntletCredentialsFile="\)/);
  assert.match(source, /FParse::Param\(FCommandLine::Get\(\),\s*GauntletLoginLobby::TestFlag\)/);
  assert.match(source, /FParse::Value\(FCommandLine::Get\(\),\s*GauntletLoginLobby::ClientIndexKey/);
  assert.match(source, /FParse::Value\(FCommandLine::Get\(\),\s*GauntletLoginLobby::FailureCaseKey/);
  assert.match(source, /FParse::Value\(FCommandLine::Get\(\),\s*GauntletLoginLobby::CredentialsFileKey/);
  assert.match(source, /FFileHelper::LoadFileToString/);
  assert.match(source, /ULoginRequestAsyncAction::RequestLogin/);
  assert.match(source, /TEXT\("_wrong"\)/);
  assert.match(source, /ClearLoginTokenEndpoint\[\] = TEXT\("\/api\/test\/clear-login-token"\)/);
  assert.match(source, /TestAuthHeaderName\[\] = TEXT\("X-Gauntlet-Test-Auth"\)/);
  assert.match(source, /LoginFlowSubsystem->HandleLoginSuccess/);
  assert.match(source, /SessionTravelSubsystem->VerifySessionAndTravelToLobby/);
  assert.match(source, /OnLobbyTravelResult\.AddUObject\(this,\s*&UGauntletLoginLobbySubsystem::HandleLobbyTravelResult\)/);
  assert.match(source, /LobbyPC->IsLobbyAuthVerified\(\)/);
  assert.match(source, /GAUNTLET_LOGIN_LOBBY_CLIENT_%d_SUCCESS/);
  assert.match(source, /GAUNTLET_LOGIN_LOBBY_CLIENT_%d_FAILURE/);
});

test("gauntlet login lobby subsystem declares and implements the party dungeon flow contract", () => {
  const header = fs.readFileSync(headerPath, "utf8");
  const source = fs.readFileSync(sourcePath, "utf8");

  assert.match(header, /enum class EGauntletPartyDungeonStep/);
  assert.match(header, /bool bRunPartyDungeonFlow = false;/);
  assert.match(header, /TMap<int32,\s*FGauntletLoginLobbyAccountCredentials> AllAccountCredentials;/);
  assert.match(header, /FTimerHandle PartyDungeonFlowTimerHandle;/);

  [
    "StartPartyDungeonFlowAfterLobbyAuth",
    "TickPartyDungeonFlow",
    "HandlePartyDungeonCreateParty",
    "HandlePartyDungeonInviteClient2",
    "HandlePartyDungeonAcceptInvite",
    "HandlePartyDungeonRequestJoin",
    "HandlePartyDungeonAcceptJoinRequest",
    "HandlePartyDungeonSelectCharacter",
    "HandlePartyDungeonSetReady",
    "HandlePartyDungeonEnterDungeon",
    "HandlePartyDungeonAuth",
    "HandlePartyDungeonSurrenderVote",
    "HandlePartyDungeonLobbyReturn",
    "FinishPartyDungeonSuccess",
    "FinishPartyDungeonFailure",
  ].forEach((functionName) => {
    assert.match(header, new RegExp(`${functionName}\\(`), `${functionName} should be declared`);
  });

  assert.match(source, /PartyDungeonTestFlag\[\] = TEXT\("GauntletPartyDungeonFlowTest"\)/);
  assert.match(source, /FParse::Param\(FCommandLine::Get\(\),\s*GauntletLoginLobby::PartyDungeonTestFlag\)/);
  assert.match(source, /GetPendingPartyJoinRequests\(\)/);
  assert.match(source, /RequestCreateParty\(\)/);
  assert.match(source, /RequestInvitePlayer\(/);
  assert.match(source, /RequestAcceptPartyInvite\(/);
  assert.match(source, /RequestJoinParty\(/);
  assert.match(source, /RequestAcceptPartyJoinRequest\(/);
  assert.match(source, /RequestSelectPartyCharacter\(/);
  assert.match(source, /RequestSetPartyReady\(true\)/);
  assert.match(source, /RequestEnterDungeon\(\)/);
  assert.match(source, /IsDungeonAuthVerified\(\)/);
  assert.match(source, /RequestStartSurrenderVote\(\)/);
  assert.match(source, /RequestSubmitSurrenderVote\(true\)/);
  assert.match(source, /GetSurrenderVoteState\(\)/);
  assert.match(source, /GAUNTLET_PARTY_DUNGEON_CLIENT_%d_SUCCESS/);
  assert.match(source, /GAUNTLET_PARTY_DUNGEON_CLIENT_%d_FAILURE/);
});

test("party dungeon gauntlet flow waits for every client before starting surrender vote", () => {
  const source = fs.readFileSync(sourcePath, "utf8");

  const functionStart = source.indexOf("void UGauntletLoginLobbySubsystem::HandlePartyDungeonSurrenderVote()");
  const functionEnd = source.indexOf("void UGauntletLoginLobbySubsystem::HandlePartyDungeonLobbyReturn()", functionStart);
  assert.notEqual(functionStart, -1, "HandlePartyDungeonSurrenderVote should be implemented");
  assert.notEqual(functionEnd, -1, "HandlePartyDungeonLobbyReturn should follow surrender vote handling");

  const surrenderVoteFunction = source.slice(functionStart, functionEnd);
  assert.match(
    surrenderVoteFunction,
    /ClientIndex == GauntletLoginLobby::Client3Index[\s\S]*VoteState\.RequiredCount\s*<\s*GauntletLoginLobby::PartyDungeonClientCount[\s\S]*return;[\s\S]*RequestStartSurrenderVote\(\)/,
  );
});
